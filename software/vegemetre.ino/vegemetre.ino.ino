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
int lcd_key = 0;
int ID = 0;
bool bulb = false;
float temp = NAN;

// === Boutons (shield DFROBOT) ===
#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5

// Lecture des boutons via A0
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

// Petit curseur logiciel (évite lcd.blink() qui fait grésiller)
void drawSoftCursor(uint8_t col) {
  // On dessine un petit "^" sur la 1ère ligne, en face du digit actif
  lcd.setCursor(0, 0);
  lcd.print("                "); // efface la ligne 0 proprement
  if (col < 16) { lcd.setCursor(col, 0); lcd.print("^"); }
}

// Ecrit l'entête CSV (18 bandes avec noms + NDVI etc.)
void writeCsvHeader(File &f) {
  // Ordre choisi = ordre “classique” R,S,T,U,V,W, G,H,I,J,K,L, A,B,C,D,E,F
  // avec lib SparkFun: getCalibratedR()... getCalibratedF()
  f.println(
    "ID,TempC,LED,NDVI,"
    "610nm-R,680nm-S,730nm-T,760nm-U,810nm-V,860nm-W,"
    "560nm-G,585nm-H,645nm-I,705nm-J,900nm-K,940nm-L,"
    "410nm-A,435nm-B,460nm-C,485nm-D,510nm-E,535nm-F"
  );
}

// Sauvegarde une ligne + écho série, puis auto-incrémente ID
void saveOneLine() {
  if (!sdOK || filename == "") return;

  // Lire T°, mesures (avec ou sans lampe)
  sensor.disableIndicator();
  temp = sensor.getTemperatureAverage();
  if (bulb) sensor.takeMeasurementsWithBulb();
  else      sensor.takeMeasurements();
  sensor.enableIndicator();

  // Récupère les 18 valeurs calibrées (float)
  float R = sensor.getCalibratedR();
  float S = sensor.getCalibratedS();
  float T = sensor.getCalibratedT();
  float U = sensor.getCalibratedU();
  float V = sensor.getCalibratedV();
  float W = sensor.getCalibratedW();

  float G = sensor.getCalibratedG();
  float H = sensor.getCalibratedH();
  float I = sensor.getCalibratedI();
  float J = sensor.getCalibratedJ();
  float K = sensor.getCalibratedK();
  float L = sensor.getCalibratedL();

  float A = sensor.getCalibratedA();
  float B = sensor.getCalibratedB();
  float C = sensor.getCalibratedC();
  float D = sensor.getCalibratedD();
  float E = sensor.getCalibratedE();
  float F = sensor.getCalibratedF();

  // NDVI = (NIR - RED) / (NIR + RED)
  // NIR ~ U (760) + V (810) + W (860) ; RED ~ I (645) + S (680) + J (705)
  float NIR = U + V + W;
  float RED = I + S + J;
  float NDVI = (NIR + RED != 0.0f) ? (NIR - RED) / (NIR + RED) : NAN;

  // Ouvre fichier et écrit
  File f = SD.open(filename, FILE_WRITE);
  if (f) {
    // Ligne CSV dans l'ordre du header
    f.print(ID); f.print(',');
    f.print(temp, 1); f.print(',');
    f.print(bulb ? 1 : 0); f.print(',');
    f.print(NDVI, 3); f.print(',');

    f.print(R, 6); f.print(','); f.print(S, 6); f.print(','); f.print(T, 6); f.print(',');
    f.print(U, 6); f.print(','); f.print(V, 6); f.print(','); f.print(W, 6); f.print(',');

    f.print(G, 6); f.print(','); f.print(H, 6); f.print(','); f.print(I, 6); f.print(',');
    f.print(J, 6); f.print(','); f.print(K, 6); f.print(','); f.print(L, 6); f.print(',');

    f.print(A, 6); f.print(','); f.print(B, 6); f.print(','); f.print(C, 6); f.print(',');
    f.print(D, 6); f.print(','); f.print(E, 6); f.print(','); f.print(F, 6);
    f.println();
    f.close();
  }

  // Echo identique sur le Moniteur Série
  Serial.print(ID); Serial.print(',');
  Serial.print(temp, 1); Serial.print(',');
  Serial.print(bulb ? 1 : 0); Serial.print(',');
  Serial.print(NDVI, 3); Serial.print(',');

  Serial.print(R, 6); Serial.print(','); Serial.print(S, 6); Serial.print(','); Serial.print(T, 6); Serial.print(',');
  Serial.print(U, 6); Serial.print(','); Serial.print(V, 6); Serial.print(','); Serial.print(W, 6); Serial.print(',');

  Serial.print(G, 6); Serial.print(','); Serial.print(H, 6); Serial.print(','); Serial.print(I, 6); Serial.print(',');
  Serial.print(J, 6); Serial.print(','); Serial.print(K, 6); Serial.print(','); Serial.print(L, 6); Serial.print(',');

  Serial.print(A, 6); Serial.print(','); Serial.print(B, 6); Serial.print(','); Serial.print(C, 6); Serial.print(',');
  Serial.print(D, 6); Serial.print(','); Serial.print(E, 6); Serial.print(','); Serial.print(F, 6);
  Serial.println();

  // Feedback LCD + auto-incrément ID pour la prochaine
  lcd.setCursor(0, 1);
  lcd.print("Sauvegarde OK   ");
  ID++;
}

// === Initialisation ===
void setup() {
  Serial.begin(115200);

  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(2, 0); lcd.print("Vegemetre");
  lcd.setCursor(3, 1); lcd.print("AgroTIC");
  delay(1200);
  lcd.clear();

  if (!SD.begin(SD_CS)) {
    lcd.print("Pas de SD.");
    lcd.setCursor(0, 1); lcd.print("Press Select");
    while (read_LCD_buttons() != btnSELECT) { delay(10); }
    lcd.clear();
    sdOK = false;
  } else {
    sdOK = true;
    selectDateAndCreateFile();
  }

  if (!sensor.begin()) {
    lcd.clear();
    lcd.print("Sensor failed!");
    while (1) { delay(10); }
  }

  // Optionnel: temps d’intégration / gain (réglages raisonnables)
  sensor.setIntegrationCycles(18);   // 18 * ~2.8 ms ≈ 50.4 ms
  //sensor.setGain(2);                 // 0=1x, 1=3.7x, 2=16x, 3=64x 
  sensor.setGain(AS7265X_GAIN_16X);

  lcd.clear();
  lcd.print("Pret !");
  delay(600);
  lcd.clear();
}

// === Saisie de la date MM/JJ, contraintes sur digits ===
void selectDateAndCreateFile() {
  int M1 = 0, M2 = 1, J1 = 0, J2 = 1;
  uint8_t pos = 0; // 0,1,2,3 -> indices : M1,M2,J1,J2
  lcd.clear();
  lcd.print("Date (MM/JJ)");
  delay(400);

  while (true) {
    // Affiche la date en ligne 1
    lcd.setCursor(0, 1);
    lcd.print("MM/JJ: ");
    lcd.print(M1); lcd.print(M2);
    lcd.print("/");
    lcd.print(J1); lcd.print(J2);
    lcd.print("   ");

    // Position du curseur logiciel (col = 7 + pos, mais +1 si après le '/')
    uint8_t col = 7 + pos + (pos >= 2 ? 1 : 0);
    drawSoftCursor(col);

    lcd_key = read_LCD_buttons();

    if (lcd_key == btnUP) {
      if (pos == 0) M1 = (M1 == 0) ? 1 : 0;
      else if (pos == 1) M2 = (M2 + 1) % 10;
      else if (pos == 2) J1 = (J1 + 1) % 4;
      else if (pos == 3) J2 = (J2 + 1) % 10;
      delay(150);
    }
    else if (lcd_key == btnDOWN) {
      if (pos == 0) M1 = (M1 == 0) ? 1 : 0;
      else if (pos == 1) M2 = (M2 + 9) % 10;
      else if (pos == 2) J1 = (J1 + 3) % 4;
      else if (pos == 3) J2 = (J2 + 9) % 10;
      delay(150);
    }
    else if (lcd_key == btnLEFT) {
      if (pos > 0) pos--;
      else pos = 3;
      delay(120);
    }
    else if (lcd_key == btnRIGHT) {
      pos = (pos + 1) % 4;
      delay(120);
    }
    else if (lcd_key == btnSELECT) {
      // Création fichier MM-JJ.txt
      String MM = String(M1) + String(M2);
      String JJ = String(J1) + String(J2);
      filename = MM + "-" + JJ + ".txt";
      File f = SD.open(filename, FILE_WRITE);
      if (f) {
        writeCsvHeader(f);
        f.close();
      }
      lcd.clear();
      lcd.print("File ");
      lcd.print(filename);
      lcd.setCursor(0, 1);
      lcd.print("cree");
      delay(800);
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("                ");
      break;
    }

    delay(20);
  }
}

// === Boucle principale ===
void loop() {
  // Bandeau d’état
  lcd.setCursor(0, 0);
  char buf[17];
  snprintf(buf, sizeof(buf), "Id:%-4d %s", ID, bulb ? "LUX" : "   ");
  lcd.print(buf);

  lcd_key = read_LCD_buttons();

  switch (lcd_key) {
    case btnRIGHT: {
      lcd.setCursor(0, 1);
      lcd.print("Mesure...       ");
      // Mesure “live” et affichage NDVI rapide
      sensor.disableIndicator();
      temp = sensor.getTemperatureAverage();
      if (bulb) sensor.takeMeasurementsWithBulb();
      else      sensor.takeMeasurements();
      sensor.enableIndicator();

      float U = sensor.getCalibratedU();
      float V = sensor.getCalibratedV();
      float W = sensor.getCalibratedW();
      float I = sensor.getCalibratedI();
      float S = sensor.getCalibratedS();
      float J = sensor.getCalibratedJ();
      float NIR = U + V + W;
      float RED = I + S + J;
      float NDVI = (NIR + RED != 0.0f) ? (NIR - RED) / (NIR + RED) : NAN;

      lcd.setCursor(0, 1);
      lcd.print("NDVI:");
      if (isnan(NDVI)) lcd.print("--  ");
      else { lcd.print(NDVI, 2); lcd.print("   "); }
      break;
    }

    case btnLEFT:
      bulb = !bulb;            // toggle LED d’illumination
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
      saveOneLine();           // écrit sur SD + série & auto-incrémente ID
      break;

    case btnNONE:
      break;
  }

  delay(60);
}
