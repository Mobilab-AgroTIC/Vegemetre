/*
  Vegemetre - Arduino UNO
  Hardware:
    - DFROBOT LCD Keypad Shield (LCD: RS=8, E=9, D4..D7=4,5,6,7; Keys on A0)
    - SD card reader on CS = 2 (SPI on 11/12/13). Keep pin 10 OUTPUT to stay SPI master.
    - SparkFun AS7265x spectral sensor on I2C (A4=SDA, A5=SCL)

  Notes (v2 fixes):
    - Avoid Arduino String to prevent SRAM fragmentation → use C buffers/prints.
    - Backlight control on pin 10 for visual feedback (flash on save/acq). Pin 10 must remain OUTPUT.
    - UI: show " LED" only when ON (nothing if OFF) to fit 16 cols.
    - Safer SD writes using f.print() chunks instead of a big snprintf.

  Features:
    - Splash screen "Vegemetre" / "AgroTIC"
    - Check sensor, then SD (optional continue without SD)
    - Date selector MM/JJ; creates file MM-JJ.txt with CSV header
    - Modes: Ponctuel / Auto (interval in seconds)
    - NDVI from U,V,W (NIR) and S,J,I (red)
*/

#include <Wire.h>
#include <LiquidCrystal.h>
#include <SPI.h>
#include <SD.h>
#include <string.h>
#include "SparkFun_AS7265X.h"

// ===================== Data types =====================
struct DateMMJJ { uint8_t MM; uint8_t JJ; };

struct SpecData {
  float A,B,C,D,E,F,G,H,R,I,S,J,T,U,V,W,K,L; // 18 channels
};

// ===================== Prototypes =====================
int    readLCDButtons();
int    waitButtonPress(uint16_t releaseDelay = 120);
void   lcdClear();
void   printCentered(uint8_t row, const char* msg);
void   splash();
void   messageTwoLines(const char* l1, const char* l2);
void   drawDateScreen(const DateMMJJ& d, uint8_t cursorIdx);
void   makeFilename(const DateMMJJ& d, char out[16]);
void   writeCsvHeader(File &f);
bool   doAcquisition(SpecData &d, bool useBulb);
float  computeNDVI(const SpecData &d);
void   writeCsvLine(File &f, int id, float tempC, bool led, float ndvi, const SpecData &d);
void   showMainScreen();
void   showAutoIntervalScreen(uint16_t value);
void   showModeSelect();
void   backlightOn();
void   backlightOff();
void   flashBacklight(uint8_t blinks = 1, uint16_t onMs = 60, uint16_t offMs = 60);

// ================= Pins =================
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);
AS7265X sensor;

const uint8_t SD_CS = 2;
const uint8_t BACKLIGHT_PIN = 10; // DFRobot Shield backlight (also SS). Must stay OUTPUT.

// ================= App state =================
bool sdOK = false;
char filename[16] = {0};
int    ID = 0;
bool   bulbOn = false;      // illumination LED state for next acquisition
float  lastNDVI = NAN;
float  lastTempC = NAN;

// Modes
enum AcqMode { MODE_PONCTUEL = 0, MODE_AUTO = 1 };
AcqMode mode = MODE_PONCTUEL;

// Auto
uint16_t intervalSec = 5;           // user-selected interval
unsigned long lastAutoMs = 0;       // last acquisition timestamp
bool autoRunning = false;           // recording flag in auto mode
unsigned long noSaveUntilMs = 0;     // guard window to avoid unintended save after key events

// ================= Buttons (DFROBOT shield) =================
#define BTN_RIGHT  0
#define BTN_UP     1
#define BTN_DOWN   2
#define BTN_LEFT   3
#define BTN_SELECT 4
#define BTN_NONE   5

int readLCDButtons() {
  int adc = analogRead(A0);
  if (adc > 1000) return BTN_NONE;
  if (adc <  50) return BTN_RIGHT;
  if (adc < 250) return BTN_UP;
  if (adc < 450) return BTN_DOWN;
  if (adc < 650) return BTN_LEFT;
  if (adc < 850) return BTN_SELECT;
  return BTN_NONE;
}

int waitButtonPress(uint16_t releaseDelay) {
  int b;
  do { b = readLCDButtons(); } while (b == BTN_NONE);
  delay(releaseDelay);
  while (readLCDButtons() != BTN_NONE) { delay(5); }
  return b;
}

// ================ Backlight ================
void backlightOn(){ digitalWrite(BACKLIGHT_PIN, HIGH); }
void backlightOff(){ digitalWrite(BACKLIGHT_PIN, LOW); }
void flashBacklight(uint8_t blinks, uint16_t onMs, uint16_t offMs){
  for(uint8_t i=0;i<blinks;i++){ backlightOff(); delay(offMs); backlightOn(); delay(onMs); }
}

// ================ UI helpers ==================
void lcdClear() { lcd.clear(); lcd.setCursor(0,0); }

void printCentered(uint8_t row, const char* msg) {
  uint8_t len = (uint8_t)strlen(msg);
  int8_t col = (16 - (int8_t)len) / 2; if (col < 0) col = 0;
  lcd.setCursor(col, row); lcd.print(msg);
}

void splash() {
  lcdClear();
  printCentered(0, "Vegemetre");
  printCentered(1, "AgroTIC");
  flashBacklight(2, 90, 90); // petit feedback visuel
}

void messageTwoLines(const char* l1, const char* l2) {
  lcdClear();
  lcd.setCursor(0,0); lcd.print(l1);
  lcd.setCursor(0,1); lcd.print(l2);
}

// ================ Date selection & file ==================
void drawDateScreen(const DateMMJJ& d, uint8_t cursorIdx) {
  lcdClear();
  printCentered(0, "Choix date MM/JJ");
  // fixed columns: 3,4,'/',6,7
  lcd.setCursor(3,1); lcd.print((d.MM/10)%10);
  lcd.setCursor(4,1); lcd.print(d.MM%10);
  lcd.setCursor(5,1); lcd.print('/');
  lcd.setCursor(6,1); lcd.print((d.JJ/10)%10);
  lcd.setCursor(7,1); lcd.print(d.JJ%10);
  uint8_t col = (cursorIdx==0)?3:(cursorIdx==1)?4:(cursorIdx==2)?6:7;
  lcd.setCursor(col,0); lcd.print('^');
}

void makeFilename(const DateMMJJ& d, char out[16]) {
  snprintf(out, 16, "%02u-%02u.txt", d.MM, d.JJ);
}

void writeCsvHeader(File &f) {
  f.println(F("ID,TempC,LED,NDVI,410nm-A,435nm-B,460nm-C,485nm-D,510nm-E,535nm-F,560nm-G,585nm-H,610nm-R,645nm-I,680nm-S,705nm-J,730nm-T,760nm-U,810nm-V,860nm-W,900nm-K,940nm-L"));
}

// ================ Sensor acquisition ==================
bool doAcquisition(SpecData &d, bool useBulb) {
  if (useBulb) sensor.takeMeasurementsWithBulb();
  else         sensor.takeMeasurements();

  d.A = sensor.getCalibratedA(); d.B = sensor.getCalibratedB(); d.C = sensor.getCalibratedC(); d.D = sensor.getCalibratedD();
  d.E = sensor.getCalibratedE(); d.F = sensor.getCalibratedF(); d.G = sensor.getCalibratedG(); d.H = sensor.getCalibratedH();
  d.R = sensor.getCalibratedR(); d.I = sensor.getCalibratedI(); d.S = sensor.getCalibratedS(); d.J = sensor.getCalibratedJ();
  d.T = sensor.getCalibratedT(); d.U = sensor.getCalibratedU(); d.V = sensor.getCalibratedV(); d.W = sensor.getCalibratedW();
  d.K = sensor.getCalibratedK(); d.L = sensor.getCalibratedL();
  return true;
}

float computeNDVI(const SpecData &d) {
  float nir = d.U + d.V + d.W;      // U(760) + V(810) + W(860)
  float red = d.S + d.J + d.I;      // S(680) + J(705) + I(645)
  float denom = nir + red;
  if (denom == 0) return NAN;
  return (nir - red) / denom;
}

void writeCsvLine(File &f, int id, float tempC, bool led, float ndvi, const SpecData &d) {
  f.print(id); f.print(',');
  f.print(tempC, 2); f.print(',');
  f.print(led?1:0); f.print(',');
  f.print(ndvi, 5); f.print(',');
  f.print(d.A,5); f.print(','); f.print(d.B,5); f.print(','); f.print(d.C,5); f.print(','); f.print(d.D,5); f.print(',');
  f.print(d.E,5); f.print(','); f.print(d.F,5); f.print(','); f.print(d.G,5); f.print(','); f.print(d.H,5); f.print(',');
  f.print(d.R,5); f.print(','); f.print(d.I,5); f.print(','); f.print(d.S,5); f.print(','); f.print(d.J,5); f.print(',');
  f.print(d.T,5); f.print(','); f.print(d.U,5); f.print(','); f.print(d.V,5); f.print(','); f.print(d.W,5); f.print(',');
  f.print(d.K,5); f.print(','); f.println(d.L,5);
}

// ================ Screens ==================
void showMainScreen() {
  lcdClear();
  // Line 0: "Id:xx  M:P/A [ LED]"
  lcd.setCursor(0,0);
  lcd.print(F("Id:")); if (ID<10) lcd.print('0'); lcd.print(ID);
  lcd.print(F("  M:")); lcd.print(mode==MODE_PONCTUEL? 'P':'A');
  if (bulbOn) lcd.print(F("  LED")); // rien si OFF

  // Line 1: "NDVI: x.xxx  "+ REC if auto
  lcd.setCursor(0,1);
  lcd.print(F("NDVI:"));
  if (isnan(lastNDVI)) { lcd.print(F(" ---")); }
  else { lcd.print(' '); lcd.print(lastNDVI,3); }
  if (autoRunning) { lcd.print(F("  REC")); }
}

void showAutoIntervalScreen(uint16_t value) {
  lcdClear();
  lcd.print(F("Intervalle (s):"));
  lcd.setCursor(0,1); lcd.print(value);
  lcd.print(F("  Up/Down Sel=OK"));
}

void showModeSelect() {
  lcdClear();
  lcd.print(F("Mode: Up=Ponc Down=Auto"));
  lcd.setCursor(0,1); lcd.print(F("Select=valider"));
}

// ================ Setup ==================
void setup() {
  pinMode(BACKLIGHT_PIN, OUTPUT); // pin 10
  backlightOn();                  // backlight ON by default

  lcd.begin(16,2);
  splash();

  Wire.begin();
  delay(50);
  bool sensorOK = sensor.begin();
  delay(20);
  if (sensorOK) messageTwoLines("Capteur OK", "");
  else          messageTwoLines("Capteur KO", "Verif. cablage");
  flashBacklight(1);
  delay(600);

  if (sensorOK) {
    if (SD.begin(SD_CS)) { sdOK = true; messageTwoLines("SD OK", ""); }
    else { sdOK = false; messageTwoLines("SD absente.", "Select=continue"); waitButtonPress(); }
    flashBacklight(1);
    delay(400);
  }

  if (sensorOK && sdOK) {
    DateMMJJ d = { .MM = 1, .JJ = 1 };
    uint8_t cursor = 0;
    while (true) {
      drawDateScreen(d, cursor);
      int b = waitButtonPress();
      if (b == BTN_LEFT) { if (cursor>0) cursor--; }
      else if (b == BTN_RIGHT) { if (cursor<3) cursor++; }
      else if (b == BTN_UP)    {
        // Edit digit-by-digit with locked tens ranges
        if (cursor == 0) { // Month tens: 0 or 1
          uint8_t tens = d.MM / 10; tens = (tens==0)?1:0; // toggle 0<->1
          uint8_t units = d.MM % 10;
          if (tens == 0) { if (units==0) units = 1; }   // months 01..09
          else { if (units>2) units = 2; }              // months 10..12
          d.MM = tens*10 + units;
        } else if (cursor == 1) { // Month units
          uint8_t tens = d.MM / 10; uint8_t units = d.MM % 10;
          if (tens==0) { units++; if (units>9) units=1; } // 1..9
          else { units++; if (units>2) units=0; }         // 0..2
          d.MM = tens*10 + units;
        } else if (cursor == 2) { // Day tens: 0..3
          uint8_t tens = d.JJ / 10; tens = (tens+1); if (tens>3) tens=0;
          uint8_t units = d.JJ % 10;
          if (tens==0) { if (units==0) units=1; }       // 01..09
          else if (tens==3) { if (units>1) units=1; }   // 30..31
          d.JJ = tens*10 + units;
        } else { // Day units
          uint8_t tens = d.JJ / 10; uint8_t units = d.JJ % 10;
          if (tens==0) { units++; if (units>9) units=1; } // 1..9
          else if (tens==3) { units++; if (units>1) units=0; } // 0..1
          else { units++; if (units>9) units=0; } // 10..29
          d.JJ = tens*10 + units;
        }
      }
      else if (b == BTN_DOWN)  {
        if (cursor == 0) { // Month tens: 0 or 1
          uint8_t tens = d.MM / 10; tens = (tens==0)?1:0; // toggle 0<->1
          uint8_t units = d.MM % 10;
          if (tens == 0) { if (units==0) units = 1; } else { if (units>2) units = 2; }
          d.MM = tens*10 + units;
        } else if (cursor == 1) { // Month units
          uint8_t tens = d.MM / 10; uint8_t units = d.MM % 10;
          if (tens==0) { if (units==1) units=9; else units--; }
          else { if (units==0) units=2; else units--; }
          d.MM = tens*10 + units;
        } else if (cursor == 2) { // Day tens 0..3
          uint8_t tens = d.JJ / 10; tens = (tens==0)?3:(uint8_t)(tens-1);
          uint8_t units = d.JJ % 10;
          if (tens==0) { if (units==0) units=1; } else if (tens==3) { if (units>1) units=1; }
          d.JJ = tens*10 + units;
        } else { // Day units
          uint8_t tens = d.JJ / 10; uint8_t units = d.JJ % 10;
          if (tens==0) { if (units==1) units=9; else units--; }
          else if (tens==3) { if (units==0) units=1; else units--; }
          else { if (units==0) units=9; else units--; }
          d.JJ = tens*10 + units;
        }
      }
      else if (b == BTN_SELECT) {
        makeFilename(d, filename);
        File f = SD.open(filename, FILE_WRITE);
        if (!f) { messageTwoLines("Err. creation", filename); delay(1200); }
        else { writeCsvHeader(f); f.flush(); f.close(); break; }
      }
    }
  }

  // Mode selection
  lcdClear();
  lcd.print(F("Up=Ponc  Down=Auto"));
  lcd.setCursor(0,1); lcd.print(F("Select=valider"));
  mode = MODE_PONCTUEL;
  while (true) {
    int b = waitButtonPress();
    if (b == BTN_UP) mode = MODE_PONCTUEL;
    else if (b == BTN_DOWN) mode = MODE_AUTO;
    else if (b == BTN_SELECT) break;
    lcd.setCursor(0,1);
    lcd.print(F("Mode:")); lcd.print(mode==MODE_PONCTUEL?"Ponctuel  ":"Auto      ");
  }

  if (mode == MODE_AUTO) {
    uint16_t val = intervalSec;
    while (true) {
      showAutoIntervalScreen(val);
      int b = waitButtonPress();
      if (b == BTN_UP)   { if (val < 3600) val++; }
      else if (b == BTN_DOWN) { if (val > 1) val--; }
      else if (b == BTN_SELECT) { intervalSec = val; break; }
    }
  }

  showMainScreen();
}

// ================ Loop ==================
void loop() {
  // Auto mode timing
  if (mode == MODE_AUTO && autoRunning) {
    unsigned long now = millis();
    unsigned long intervalMs = (unsigned long)intervalSec * 1000UL;
    if (now >= noSaveUntilMs && (now - lastAutoMs) >= intervalMs) {
      lastAutoMs = now;
      SpecData d; if (doAcquisition(d, bulbOn)) {
        lastNDVI = computeNDVI(d);
        lastTempC = sensor.getTemperature();
        showMainScreen();
        if (sdOK && filename[0]) {
          File f = SD.open(filename, FILE_WRITE);
          if (f) {
            writeCsvLine(f, ID, lastTempC, bulbOn, lastNDVI, d);
            f.flush(); f.close();
            ID++; flashBacklight(1);
          }
        }
      } else {
        messageTwoLines("Acq. echouee", ""); flashBacklight(2,50,50); delay(400);
      }
    }
  }

  // Buttons
  int b = readLCDButtons();
  if (b == BTN_NONE) return;
  delay(120);
  while (readLCDButtons() != BTN_NONE) { delay(5); }

  if (mode == MODE_PONCTUEL) {
    if (b == BTN_RIGHT) {
      SpecData d; if (doAcquisition(d, bulbOn)) {
        lastNDVI = computeNDVI(d);
        lastTempC = sensor.getTemperature();
        flashBacklight(1);
        showMainScreen();
      } else { messageTwoLines("Acq. echouee", ""); flashBacklight(2,50,50); delay(400); }
    }
    else if (b == BTN_LEFT) { bulbOn = !bulbOn; if (autoRunning) { lastAutoMs = millis(); noSaveUntilMs = millis() + 600; } showMainScreen(); }
    else if (b == BTN_UP)   { if (ID < 99) ID++; showMainScreen(); }
    else if (b == BTN_DOWN) { if (ID > 0)  ID--; showMainScreen(); }
    else if (b == BTN_SELECT) {
      if (!sdOK) { messageTwoLines("SD absente", "Pas de sauvegarde"); flashBacklight(2,40,40); delay(500); showMainScreen(); return; }
      if (isnan(lastNDVI)) { messageTwoLines("Rien a sauver", "Faites RIGHT avant"); flashBacklight(2,40,40); delay(600); showMainScreen(); return; }
      File f = SD.open(filename, FILE_WRITE);
      if (!f) { messageTwoLines("Erreur fichier", filename); flashBacklight(3,40,40); delay(600); }
      else {
        SpecData d; if (doAcquisition(d, bulbOn)) {
          float nd = computeNDVI(d);
          float tc = sensor.getTemperature();
          writeCsvLine(f, ID, tc, bulbOn, nd, d);
          f.flush(); f.close();
          ID++; lastNDVI = nd; lastTempC = tc; flashBacklight(1);
          showMainScreen();
        } else { f.close(); messageTwoLines("Acq. echouee", ""); flashBacklight(2,50,50); delay(500); }
      }
    }
  } else { // MODE_AUTO
    if (b == BTN_SELECT) { autoRunning = !autoRunning; if (autoRunning) { lastAutoMs = millis(); noSaveUntilMs = millis() + 600; } flashBacklight(1); showMainScreen(); }
    else if (b == BTN_LEFT) { bulbOn = !bulbOn; showMainScreen(); }
    else if (b == BTN_UP)   { if (!autoRunning && ID < 99) ID++; showMainScreen(); }
    else if (b == BTN_DOWN) { if (!autoRunning && ID > 0)  ID--; showMainScreen(); }
    else if (b == BTN_RIGHT) {
      SpecData d; if (doAcquisition(d, bulbOn)) {
        lastNDVI = computeNDVI(d);
        lastTempC = sensor.getTemperature();
        flashBacklight(1);
        showMainScreen();
      }
    }
  }
}
