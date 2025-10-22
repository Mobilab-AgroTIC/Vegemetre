#include <LiquidCrystal.h>
#include "SparkFun_AS7265X.h"
#include <SPI.h>
#include <SD.h>

// === LCD ===
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// === Capteur ===
AS7265X sensor;

// === SD ===
const int SD_CS = 2;
bool sdOK = false;
String filename = "";

// === Variables globales ===
int   lcd_key = 0;
int   ID = 0;
bool  bulb = false;
float temp = NAN;

// === Modes d'acquisition ===
enum AcqMode { MODE_PONCTUEL = 0, MODE_AUTO = 1 };
AcqMode mode = MODE_PONCTUEL;
int  intervalSec = 5;                // pour MODE_AUTO
unsigned long lastAutoSaveMs = 0;    // timer sauvegarde auto
unsigned long lastNdviUpdateMs = 0;  // timer affichage NDVI auto
bool autoRunning = false;            // Start/Stop en MODE_AUTO

// === Boutons (shield DFROBOT) ===
#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5

// ---------- Lecture boutons ----------
int read_LCD_buttons() {
  int adc_key_in = analogRead(A0);
  if (adc_key_in > 1000) return btnNONE;
  if (adc_key_in < 50)   return btnRIGHT;
  if (adc_key_in < 250)  return btnUP;
  if (adc_key_in < 450)  return btnDOWN;
  if (adc_key_in < 650)  return btnLEFT;
  if (adc_key_in < 850)  return btnSELECT;
  return btnNONE;
}

// ---------- Curseur logiciel ----------
void drawSoftCursor(uint8_t col) {
  lcd.setCursor(0, 0);
  lcd.print("                "); // efface la ligne 0
  if (col < 16) { lcd.setCursor(col, 0); lcd.print("^"); }
}

// ---------- Entête CSV (ordre = A,B,C,D,E,F,G,H,R,I,S,J,T,U,V,W,K,L) ----------
void writeCsvHeader(File &f) {
  f.println(
    "ID,TempC,LED,NDVI,"
    "410nm-A,435nm-B,460nm-C,485nm-D,510nm-E,535nm-F,"
    "560nm-G,585nm-H,610nm-R,645nm-I,680nm-S,705nm-J,"
    "730nm-T,760nm-U,810nm-V,860nm-W,900nm-K,940nm-L"
  );
}

// ---------- Mesure + NDVI ----------
float measureNDVI() {
  sensor.disableIndicator();
  temp = sensor.getTemperatureAverage();
  if (bulb) sensor.takeMeasurementsWithBulb();
  else      sensor.takeMeasurements();
  sensor.enableIndicator();

  // NDVI (NIR = U+V+W | RED = I+S+J)
  float U = sensor.getCalibratedU();
  float V = sensor.getCalibratedV();
  float W = sensor.getCalibratedW();
  float I = sensor.getCalibratedI();
  float S = sensor.getCalibratedS();
  float J = sensor.getCalibratedJ();

  float NIR = U + V + W;
  float RED = I + S + J;
  return (NIR + RED != 0.0f) ? (NIR - RED) / (NIR + RED) : NAN;
}

// ---------- Sauvegarde une ligne + écho série + auto-ID ----------
void saveOneLine() {
  if (!sdOK || filename == "") return;

  // Re-mesure pour cohérence
  sensor.disableIndicator();
  temp = sensor.getTemperatureAverage();
  if (bulb) sensor.takeMeasurementsWithBulb();
  else      sensor.takeMeasurements();
  sensor.enableIndicator();

  // 18 bandes (ordre d'impression = header corrigé)
  float A = sensor.getCalibratedA();
  float B = sensor.getCalibratedB();
  float C = sensor.getCalibratedC();
  float D = sensor.getCalibratedD();
  float E = sensor.getCalibratedE();
  float F = sensor.getCalibratedF();

  float G = sensor.getCalibratedG();
  float H = sensor.getCalibratedH();
  float Rr = sensor.getCalibratedR(); // 610 nm (lib appelle ce canal 'R')
  float I = sensor.getCalibratedI();
  float S = sensor.getCalibratedS();
  float J = sensor.getCalibratedJ();

  float T = sensor.getCalibratedT();
  float U = sensor.getCalibratedU();
  float V = sensor.getCalibratedV();
  float W = sensor.getCalibratedW();
  float K = sensor.getCalibratedK();
  float L = sensor.getCalibratedL();

  float NIR = U + V + W;
  float RED = I + S + J;
  float NDVI = (NIR + RED != 0.0f) ? (NIR - RED) / (NIR + RED) : NAN;

  File f = SD.open(filename, FILE_WRITE);
  if (f) {
    f.print(ID); f.print(',');
    f.print(temp, 1); f.print(',');
    f.print(bulb ? 1 : 0); f.print(',');
    f.print(NDVI, 3); f.print(',');

    f.print(A, 6); f.print(','); f.print(B, 6); f.print(','); f.print(C, 6); f.print(',');
    f.print(D, 6); f.print(','); f.print(E, 6); f.print(','); f.print(F, 6); f.print(',');

    f.print(G, 6); f.print(','); f.print(H, 6); f.print(','); f.print(Rr,6); f.print(',');
    f.print(I, 6); f.print(','); f.print(S, 6); f.print(','); f.print(J, 6); f.print(',');

    f.print(T, 6); f.print(','); f.print(U, 6); f.print(','); f.print(V, 6); f.print(',');
    f.print(W, 6); f.print(','); f.print(K, 6); f.print(','); f.print(L, 6);
    f.println();
    f.close();
  }

  // Echo série
  Serial.print(ID); Serial.print(',');
  Serial.print(temp, 1); Serial.print(',');
  Serial.print(bulb ? 1 : 0); Serial.print(',');
  Serial.print(NDVI, 3); Serial.print(',');

  Serial.print(A, 6); Serial.print(','); Serial.print(B, 6); Serial.print(','); Serial.print(C, 6); Serial.print(',');
  Serial.print(D, 6); Serial.print(','); Serial.print(E, 6); Serial.print(','); Serial.print(F, 6); Serial.print(',');

  Serial.print(G, 6); Serial.print(','); Serial.print(H, 6); Serial.print(','); Serial.print(Rr,6); Serial.print(',');
  Serial.print(I, 6); Serial.print(','); Serial.print(S, 6); Serial.print(','); Serial.print(J, 6); Serial.print(',');

  Serial.print(T, 6); Serial.print(','); Serial.print(U, 6); Serial.print(','); Serial.print(V, 6); Serial.print(',');
  Serial.print(W, 6); Serial.print(','); Serial.print(K, 6); Serial.print(','); Serial.print(L, 6);
  Serial.println();

  lcd.setCursor(0, 1);
  lcd.print("Sauvegarde OK   ");
  ID++;
}

// ---------- Sélection du mode ----------
void selectMode() {
  int choice = 0; // 0=Ponctuel, 1=Spontane
  while (true) {
    lcd.setCursor(0, 0); lcd.print(" Choix mode     ");
    lcd.setCursor(0, 1);
    if (choice == 0) lcd.print("> Ponctuel      ");
    else             lcd.print("> Auto          ");

    lcd_key = read_LCD_buttons();
    if (lcd_key == btnUP || lcd_key == btnDOWN || lcd_key == btnLEFT || lcd_key == btnRIGHT) {
      choice = 1 - choice;
      delay(150);
    } else if (lcd_key == btnSELECT) {
      mode = (choice == 0) ? MODE_PONCTUEL : MODE_AUTO;
      lcd.clear();
      break;
    }
    delay(20);
  }
}

// ---------- Sélection de l'intervalle (s) pour MODE_AUTO ----------
int selectIntervalSeconds() {
  int val = 5;
  while (true) {
    lcd.setCursor(0, 0); lcd.print(" Intervalle (s) ");
    lcd.setCursor(0, 1);
    char line[17];
    snprintf(line, sizeof(line), " t=%-4d  Sel=OK ", val);
    lcd.print(line);

    lcd_key = read_LCD_buttons();
    if      (lcd_key == btnUP)    { val = (val < 3600) ? val + 1 : 3600; delay(120); }
    else if (lcd_key == btnDOWN)  { val = (val > 1)    ? val - 1 : 1;    delay(120); }
    else if (lcd_key == btnLEFT)  { val = (val > 10)   ? val - 10 : 1;   delay(120); }
    else if (lcd_key == btnRIGHT) { val = (val <= 3590)? val + 10 : 3600;delay(120); }
    else if (lcd_key == btnSELECT){ lcd.clear(); return val; }
    delay(20);
  }
}

// ================== Initialisation ==================
void setup() {
  Serial.begin(115200);

  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(2, 0); lcd.print("Vegemetre");
  lcd.setCursor(3, 1); lcd.print("AgroTIC");
  delay(900);
  lcd.clear();

  if (!SD.begin(SD_CS)) {
    lcd.print("Pas de SD.");
    lcd.setCursor(0, 1); lcd.print("Press Select");
    while (read_LCD_buttons() != btnSELECT) { delay(10); }
    lcd.clear();
    sdOK = false;
  } else {
    sdOK = true;
    selectDateAndCreateFile();   // crée fichier + header
    selectMode();                // choix du mode
    if (mode == MODE_AUTO) {
      intervalSec = selectIntervalSeconds();
      autoRunning = true;        // démarre en RUN
      lastAutoSaveMs = millis();
      lastNdviUpdateMs = 0;
    }
  }

  if (!sensor.begin()) {
    lcd.clear();
    lcd.print("Sensor failed!");
    while (1) { delay(10); }
  }

  sensor.setIntegrationCycles(18);   // ~50 ms (18 * ~2.8 ms)
  sensor.setGain(AS7265X_GAIN_16X);

  lcd.clear();
  lcd.print("Pret !");
  delay(500);
  lcd.clear();
}

// ---------- Saisie date MM/JJ ----------
void selectDateAndCreateFile() {
  int M1 = 0, M2 = 1, J1 = 0, J2 = 1;
  uint8_t pos = 0; // 0..3 -> M1,M2,J1,J2
  lcd.clear();
  lcd.print("Date (MM/JJ)");
  delay(300);

  while (true) {
    lcd.setCursor(0, 1);
    lcd.print("MM/JJ: ");
    lcd.print(M1); lcd.print(M2);
    lcd.print("/");
    lcd.print(J1); lcd.print(J2);
    lcd.print("   ");

    uint8_t col = 7 + pos + (pos >= 2 ? 1 : 0);
    drawSoftCursor(col);

    lcd_key = read_LCD_buttons();
    if      (lcd_key == btnUP)    { if (pos==0) M1=(M1==0)?1:0; else if(pos==1) M2=(M2+1)%10; else if(pos==2) J1=(J1+1)%4; else J2=(J2+1)%10; delay(150); }
    else if (lcd_key == btnDOWN)  { if (pos==0) M1=(M1==0)?1:0; else if(pos==1) M2=(M2+9)%10; else if(pos==2) J1=(J1+3)%4; else J2=(J2+9)%10; delay(150); }
    else if (lcd_key == btnLEFT)  { pos = (pos == 0) ? 3 : (pos - 1); delay(120); }
    else if (lcd_key == btnRIGHT) { pos = (pos + 1) % 4;              delay(120); }
    else if (lcd_key == btnSELECT){
      String MM = String(M1) + String(M2);
      String JJ = String(J1) + String(J2);
      filename = MM + "-" + JJ + ".txt";
      File f = SD.open(filename, FILE_WRITE);
      if (f) { writeCsvHeader(f); f.close(); }
      lcd.clear();
      lcd.print("File "); lcd.print(filename);
      lcd.setCursor(0, 1); lcd.print("cree");
      delay(700);
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("                ");
      break;
    }
    delay(20);
  }
}

// ---------- Affiche NDVI (ligne 2) ----------
void showNDVI(float ndvi) {
  lcd.setCursor(0, 1);
  lcd.print("NDVI:");
  if (isnan(ndvi)) lcd.print("--      ");
  else { lcd.print(ndvi, 2); lcd.print("      "); }
}

// ================== Boucle principale ==================
void loop() {
  // Ligne 1: Id + état
  lcd.setCursor(0, 0);
  char head[17];
  if (mode == MODE_AUTO) {
    // "Au>" = RUN ; "Au||" = PAUSE ; "L" = lampe ON
    snprintf(head, sizeof(head), "Id:%-4d Au%s%s",
             ID, autoRunning ? ">" : "||", bulb ? "L" : " ");
  } else {
    snprintf(head, sizeof(head), "Id:%-4d %s", ID, bulb ? "LUX" : "   ");
  }
  lcd.print(head);

  if (mode == MODE_PONCTUEL) {
    lcd_key = read_LCD_buttons();
    switch (lcd_key) {
      case btnRIGHT: {
        float ndvi = measureNDVI();
        showNDVI(ndvi);
        break;
      }
      case btnLEFT:
        bulb = !bulb;
        delay(150);
        break;
      case btnUP:
        ID++;
        delay(120);
        break;
      case btnDOWN:
        if (ID > 0) ID--;
        delay(120);
        break;
      case btnSELECT:
        saveOneLine();
        break;
      case btnNONE:
        break;
    }
    delay(60);
  } else {
    // MODE_AUTO
    unsigned long now = millis();

    // NDVI rafraîchi ~1 Hz (même en pause)
    if (now - lastNdviUpdateMs >= 1000UL) {
      float ndvi = measureNDVI();
      showNDVI(ndvi);
      lastNdviUpdateMs = now;
    }

    // Sauvegarde auto seulement en RUN
    if (autoRunning && (now - lastAutoSaveMs >= (unsigned long)intervalSec * 1000UL)) {
      saveOneLine();
      lastAutoSaveMs = now;
    }

    // Contrôles utilisateur
    lcd_key = read_LCD_buttons();
    if (lcd_key == btnLEFT) {
      bulb = !bulb;
      delay(150);
    } else if (lcd_key == btnRIGHT) {
      autoRunning = !autoRunning;           // Start/Stop
      if (autoRunning) {
        lastAutoSaveMs = millis();
        lastNdviUpdateMs = 0;
      }
      lcd.setCursor(0, 1);
      lcd.print(autoRunning ? "AUTO: RUN       " : "AUTO: PAUSE     ");
      delay(250);
    }
    delay(20);
  }
}
