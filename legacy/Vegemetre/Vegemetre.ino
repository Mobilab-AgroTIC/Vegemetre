#include <LiquidCrystal.h>
#include "SparkFun_AS7265X.h" //Click here to get the library: http://librarymanager/All#SparkFun_AS7265X
#include <SPI.h>
#include <SD.h>


// Declare Objects
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);
AS7265X sensor;
File myFile;

// define some values used by the panel and buttons
int lcd_key     = 0;
int adc_key_in  = 0;
#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5
int J = 0;
int JJ = 0;
int M = 0;
int MM = 0;
String day = "00";
String month = "00";

int ID=0;
bool bulb;
float temp;
int read_LCD_buttons()
{
 adc_key_in = analogRead(0);      // read the value from the sensor 
 if (adc_key_in > 1000) return btnNONE; // We make this the 1st option for speed reasons since it will be the most likely result
 if (adc_key_in < 50)   return btnRIGHT;  
 if (adc_key_in < 195)  return btnUP; 
 if (adc_key_in < 380)  return btnDOWN; 
 if (adc_key_in < 555)  return btnLEFT; 
 if (adc_key_in < 790)  return btnSELECT;   
 return btnNONE;  // when all others fail, return this...
}

void setup()
{
 lcd.begin(16, 2);              // start the library
 lcd.setCursor(0,0);
 lcd.print("Vegemetre"); // print a simple message
 lcd.setCursor(0,1);
 lcd.print("AGROTIC"); // print a simple message
 lcd.clear();
 
 if(sensor.begin() == false)
  {
    lcd.clear();
    lcd.print("Sensor failed!");
    while(1);
  }
}
void loop()
{
 delay(100);
   lcd.setCursor(0,0);
   lcd.print("Id : " + String(ID) + "  ");
   if (bulb ==1) {
    lcd.setCursor(11,0);
    lcd.print("LUX");
   }

   lcd_key = read_LCD_buttons();  // read the buttons
  
   switch (lcd_key)               // depending on which button was pushed, we perform an action
   {
     case btnRIGHT:
       {
       lcd.setCursor(0,1);
       lcd.print("............ ");
       sensor.disableIndicator();
       temp = sensor.getTemperatureAverage();
       if (bulb ==0){
       sensor.takeMeasurements(); }
       else if (bulb ==1) {
       sensor.takeMeasurementsWithBulb();}
       sensor.enableIndicator();
       float NDVI = (
       ((sensor.getCalibratedU()+sensor.getCalibratedV()+sensor.getCalibratedW()) - (sensor.getCalibratedS()+sensor.getCalibratedJ()+sensor.getCalibratedI()))  
       /  
       ((sensor.getCalibratedU()+sensor.getCalibratedV()+sensor.getCalibratedW()) + (sensor.getCalibratedS()+sensor.getCalibratedJ()+sensor.getCalibratedI()))
       );
       lcd.setCursor(0,1);
       lcd.print("NDVI : ");
       lcd.print(NDVI,2);
       lcd.print(" ");
       break;
       }
     case btnLEFT:
       {
       lcd.setCursor(11,0);
       if (bulb == 0){
        bulb=1;
        lcd.print("LUX");
        }
       else if (bulb ==1) {
        bulb=0;
        lcd.print("   ");
  
        }
       delay(300);
       break;
       }
     case btnUP:
       {
       ID++;
       delay(200);
       break;
       }
     case btnDOWN:
       {
       if(ID>0){
       ID--;}
       delay(200);
       break;
       }
     case btnSELECT:
       {
       lcd.setCursor(0,0);
       lcd.print("Saving...");
       ID++;
       lcd.clear();
       break;
       }
       case btnNONE:
       {
       break;
       }
   }
  }
