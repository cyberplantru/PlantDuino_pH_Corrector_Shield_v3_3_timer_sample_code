/*
  Example code for the CyberPlant pH Corrector Shield v3.0:
  
    http://www.cyberplant.info
  by CyberPlant LLC, 17 April 2014
  This example code is in the public domain.
  

  
  ADC voltages for the 5 buttons on analog input pin A0:
  
    RIGHT:  0.00V :   0 @ 8bit ;   0 @ 10 bit
    UP:     0.71V :  36 @ 8bit ; 145 @ 10 bit
    DOWN:   1.61V :  82 @ 8bit ; 329 @ 10 bit
    LEFT:   2.47V : 126 @ 8bit ; 505 @ 10 bit
    SELECT: 3.62V : 185 @ 8bit ; 741 @ 10 bit
*/
/*--------------------------------------------------------------------------------------
  Includes
--------------------------------------------------------------------------------------*/
#include <Wire.h>
#include <LiquidCrystal_I2C.h>  // include LCD library
#include "RTClib.h"
#include <EEPROMVar.h>
#include <EEPROMex.h> 
#define ads1110 0x48
#define pumpPhup 5             // pump pH Up
#define pumpPhdown 6           // pump pH Down
#include <SPI.h>

/*--------------------------------------------------------------------------------------
  Defines
--------------------------------------------------------------------------------------*/
// Pins in use
#define BUTTON_ADC_PIN           A0  // A0 is the button ADC input

// ADC readings expected for the 5 buttons on the ADC input
#define SAVE_10BIT_ADC           0  // right
#define NEXT_10BIT_ADC            145  // up
#define UP_10BIT_ADC          329  // down
#define DOWN_10BIT_ADC          505  // left
#define PREV_10BIT_ADC        741  // right
#define BUTTONHYSTERESIS         10  // hysteresis for valid button sensing window
//return values for ReadButtons()
#define BUTTON_NONE               0  // 
#define BUTTON_SAVE              1  // 
#define BUTTON_NEXT                 2  // 
#define BUTTON_UP               3  // 
#define BUTTON_DOWN               4  // 
#define BUTTON_PREV             5  // 

/*--------------------------------------------------------------------------------------
  Variables
--------------------------------------------------------------------------------------*/
byte buttonJustPressed  = false;         //this will be true after a ReadButtons() call if triggered
byte buttonJustReleased = false;         //this will be true after a ReadButtons() call if triggered
byte buttonWas          = BUTTON_NONE;   //used by ReadButtons() for detection of button events
/*--------------------------------------------------------------------------------------
  Init the LCD library with the LCD pins to be used
--------------------------------------------------------------------------------------*/
LiquidCrystal_I2C lcd(0x38,8,2);  // set the LCD address to 0x20 for a 16 chars and 2 line display
RTC_DS1307 rtc;
/*--------------------------------------------------------------------------------------
  setup()
  Called by the Arduino framework once, before the main loop begins
--------------------------------------------------------------------------------------*/
int m=0; //переменная для экранов меню
int sm=0; //переменная для вабора часы минуты
int rsm=0; //переменная для вабора reset
int tpm=0; //переменная для Test Pump
int lgm=0; //переменная для Log

// Addrres storing settings in non-volatile memory Arduino:
int addressPhlow = 0;
int addressPhhigh = addressPhlow+sizeof(double);
int addressCentrePoint = addressPhhigh+sizeof(double);
int addressAlpha = addressCentrePoint+sizeof(double);
int addressDose = addressAlpha+sizeof(double);

int addressLogUp = addressDose+sizeof(double);
int addressLogDown = addressLogUp+sizeof(double);

int addressPhdelay = addressLogDown+sizeof(double);
int addressTube = addressPhdelay+sizeof(int);
int addressR = addressTube+sizeof(int);
int addressHrOn = addressR+sizeof(int);
int addressHrOff = addressHrOn+sizeof(int);
int addressMinOn = addressHrOff+sizeof(int);
int addressMinOff = addressMinOn+sizeof(int);
int addressRelayMode = addressMinOff+sizeof(int);


// Read the settings from non-volatile memory :
double pHlow = EEPROM.readDouble(addressPhlow);
double pHhigh = EEPROM.readDouble(addressPhhigh);
double CentrePoint = EEPROM.readDouble(addressCentrePoint);
double alpha = EEPROM.readDouble(addressAlpha);
double dose = EEPROM.readDouble(addressDose);
double LogUp = EEPROM.readDouble(addressLogUp);
double LogDown = EEPROM.readDouble(addressLogDown);

int pHdelay = EEPROM.readInt(addressPhdelay);
int tube = EEPROM.readInt(addressTube);
int c = 0;
int r = EEPROM.readInt(addressR);
int HrOn = EEPROM.readInt(addressHrOn);
int HrOff = EEPROM.readInt(addressHrOff);
int MinOn = EEPROM.readInt(addressMinOn);
int MinOff = EEPROM.readInt(addressMinOff);
int RelayMode = EEPROM.readInt(addressRelayMode);


int d=0; //переменная для delay
int dmi=0;
char * pHmode[2] = {"auto", "manual"};
char * Rmode[3] = {"on", "auto", "off"}; //Relay mode


int hr;
int mi;
int sc;
int dy;
int mo;
int yh;
int ft=0;
int ftdy;
int ftmo;
int ftyh;
int ftmi;

float testpump;
float timepump;
int secpump;
float Utimepump;
float Dtimepump;
unsigned int dInterval=1000, phInterval=100;
long previousMillis = 0;
long pHpreviousMillis = 0;
unsigned long dTime, phTime;

enum { REG = 9 }; // pin D9 is SS line for LMP91200

float voltage, data;
float pH;
byte highbyte, lowbyte, configRegister;

const int numReadings = 10;

float readings[numReadings];      // the readings from the analog input
int index = 0;                  // the index of the current reading
float total = 0;                  // the running total
float average = 0;                // the average

void writeLMP91200(int ss_pin, uint16_t value)
{
  digitalWrite(ss_pin, LOW);
  /* Фокус вот в чём: сначала шлём старший байт */
  SPI.transfer(highByte(value));
  /* А потом младший */
  SPI.transfer(lowByte(value));
  digitalWrite(ss_pin, HIGH);
  
}
void setup()
{

  dTime=millis();
phTime=millis();

    lcd.init();                      // initialize the lcd 
  Serial.begin(57600);
#ifdef AVR
  Wire.begin();
#else
  Wire1.begin(); // Shield I2C pins connect to alt I2C bus on Arduino Due
#endif
  rtc.begin();

  if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running!");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }
   //button adc input
   pinMode( BUTTON_ADC_PIN, INPUT );         //ensure A0 is an input
   digitalWrite( BUTTON_ADC_PIN, LOW );      //ensure pullup is off on A0
   //lcd backlight control


   //Print some initial text to the LCD.
  lcd.setCursor( 0, 0 );
  lcd.print( "pH Corr." );
  lcd.setCursor( 0, 1 );
  lcd.print( "ver. 3.3" );
  delay (2000);
  
  SPI.setClockDivider(SPI_CLOCK_DIV8);
    SPI.setDataMode(SPI_MODE1);
  SPI.begin();

  pinMode(REG, OUTPUT);
   writeLMP91200(REG, 0b0000000000000000); // read pH
  //writeShiftRegister16(REG, 0b1001000010000000);
  //writeShiftRegister16(REG, 0b1000100000000000);
  //writeShiftRegister16(REG, 0b1100000000100000);
  
  lcd.clear ();
   
     for (int thisReading = 0; thisReading < numReadings; thisReading++)
    readings[thisReading] = 0;   

}
  void rotateLeft(uint16_t &bits)
{
  uint16_t high_bit = bits & (1 << 15) ? 1 : 0;
  bits = (bits << 1) | high_bit;
}

          
/*--------------------------------------------------------------------------------------
  loop()
  Arduino main loop
--------------------------------------------------------------------------------------*/
void loop()
{

  // Чтение pH
  
  if (millis()-phTime>=phInterval)
  {  
    phTime = millis();
    voltage = ADSread();

 // float pHread = ((alpha * voltage / (273.15 + 25)) + CentrePoint);
 total= total - readings[index];
 readings[index] = ((alpha * voltage / (273.15 + 25)) + CentrePoint);
 total= total + readings[index];
 index = index + 1;
   // if we're at the end of the array...
  if (index >= numReadings)              
    // ...wrap around to the beginning: 
    index = 0;    
    average = total / numReadings;
    pH = average;
    
     DateTime now = rtc.now();
  
    
      
   hr = now.hour();
   mi = now.minute();
   sc = now.second();
   dy = now.day();
   mo = now.month();
   yh = now.year();
   
   DateTime future (now.unixtime() + ft * 86400L + 00);
   
   ftdy = future.day();
   ftmo = future.month();
   ftyh = future.year();
   
    //----------------------------------function timer----------------------------------------------------- 
   if (RelayMode==0)
{
  lcd.relayOn(); //Relay On
}
  if (RelayMode==1){
    if ((HrOff*60+MinOff)-(HrOn*60+MinOn) > 0)
{
    if ((hr*60+mi) >= (HrOn*60+MinOn) && (hr*60+mi) < (HrOff*60+MinOff))
    {
      lcd.relayOn(); //Relay On

    }
    else
    {
      lcd.relay(); //Relay off

    }
   
    if ((hr*60+mi) >= (HrOff*60+MinOff))
    {    
      lcd.relay(); //Relay off

    }
}
else if ((HrOff*60+MinOff)-(HrOn*60+MinOn) < 0)
{
  if ((hr*60+mi) >= (HrOn*60+MinOn))
    {
      lcd.relayOn(); //Relay On

    }
   
    if ((hr*60+mi) >= (HrOff*60+MinOff) && (hr*60+mi) < (HrOn*60+MinOn))
    {    
      lcd.relay(); //Relay off

    } 
}
else if ((HrOff*60+MinOff)-(HrOn*60+MinOn) == 0)
{
  lcd.relay(); //Relay off
}
  }
  if (RelayMode==2)
  {
    lcd.relay(); //Relay Off
  }
  }

   byte button;
   
   
   //get the latest button pressed, also the buttonJustPressed, buttonJustReleased flags
   button = ReadButtons();

   //show text label for the button pressed
   switch( button )
   {
      case BUTTON_NONE:
      {
        break;
      }
      case BUTTON_SAVE:
      {
if (m==0){
  break;
  }
if (m==1){
  break;
  }
if (m==2){
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  Save  ");
  lcd.setCursor(0, 1);
  lcd.print("pH Low");
  lcd.blink();
  EEPROM.updateDouble(addressPhlow, pHlow); 
  delay(2000);
  pHlow = EEPROM.readDouble(addressPhlow);
  lcd.noBlink();
  lcd.setCursor(0, 0);
  lcd.print("Complete");
  delay(1000);
  lcd.clear();
  break;
  }
if (m==3){
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  Save  ");
  lcd.setCursor(0, 1);
  lcd.print("pH High");
  lcd.blink();
  EEPROM.updateDouble(addressPhhigh, pHhigh); 
  delay(2000);
  pHhigh = EEPROM.readDouble(addressPhhigh);
  lcd.noBlink();
  lcd.setCursor(0, 0);
  lcd.print("Complete");
  delay(1000);
  lcd.clear();
  break;
}
if (m==4){
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  Save  ");
  lcd.setCursor(0, 1);
  lcd.print("Dose");
  lcd.blink();
  EEPROM.updateDouble(addressDose, dose); 
  delay(2000);
  dose = EEPROM.readDouble(addressDose);
  lcd.noBlink();
  lcd.setCursor(0, 0);
  lcd.print("Complete");
  delay(1000);
  lcd.clear();
  break;
}
if (m==5){ 
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  Save  ");
  lcd.setCursor(0, 1);
  lcd.print("pH Delay");
  lcd.blink();
  EEPROM.updateInt(addressPhdelay, pHdelay); 
  delay(2000);
  pHdelay = EEPROM.readInt(addressPhdelay);
  lcd.noBlink();
  lcd.setCursor(0, 0);
  lcd.print("Complete");
  delay(1000);
  lcd.clear();
  break;
}
if (m==6){ 
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  Save  ");
  lcd.setCursor(0, 1);
  lcd.print("Tube");
  lcd.blink();
  EEPROM.updateInt(addressTube, tube); 
  delay(2000);
  tube = EEPROM.readInt(addressTube);
  lcd.noBlink();
  lcd.setCursor(0, 0);
  lcd.print("Complete");
  delay(1000);
  lcd.clear();
  break;
}
if (m==7 && tpm==0) // Test Pump pH Down
{
  digitalWrite (pumpPhdown,HIGH);
  delay (dose*60000/tube); 
  digitalWrite (pumpPhdown,LOW);
  float D1timepump = EEPROM.readDouble(addressLogDown);
  EEPROM.updateDouble(addressLogDown, (D1timepump+dose));
  LogDown = EEPROM.readDouble(addressLogDown);
  delay(200);
  break;
} 
if (m==7 && tpm==1) // Test Pump pH Up
{
  digitalWrite (pumpPhup,HIGH);  // turn on the pump pH UP  
  delay (dose*60000/tube);                                                        
  digitalWrite (pumpPhup,LOW); // turn off the pump
  float U1timepump = EEPROM.readDouble(addressLogUp);
  EEPROM.updateDouble(addressLogUp, (U1timepump+dose));
  LogUp = EEPROM.readDouble(addressLogUp);
  delay(200);
  break;
} 
if (m==8){ 
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  Save  ");
  lcd.setCursor(0, 1);
  lcd.print("IP");
  lcd.blink();
  EEPROM.updateDouble(addressCentrePoint, CentrePoint); 
  delay(2000);
  CentrePoint = EEPROM.readDouble(addressCentrePoint);
  lcd.noBlink();
  lcd.setCursor(0, 0);
  lcd.print("Complete");
  delay(1000);
  lcd.clear();
  break;
}
if (m==9){ 
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  Save  ");
  lcd.setCursor(0, 1);
  lcd.print("mV/pH");
  lcd.blink();
  EEPROM.updateDouble(addressAlpha, alpha);
  delay(2000);
  alpha = EEPROM.readDouble(addressAlpha);
  lcd.noBlink();
  lcd.setCursor(0, 0);
  lcd.print("Complete");
  delay(1000);
  lcd.clear();
  break;
}
  if (m==10 && rsm==0) // RESET All
  {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ResetAll");
  lcd.setCursor(0, 1);
  lcd.blink();
  EEPROM.updateDouble(addressPhlow, 1); 
  EEPROM.updateDouble(addressPhhigh, 14); 
  EEPROM.updateDouble(addressDose, 0); 
  EEPROM.updateDouble(addressLogUp, 0); 
  EEPROM.updateDouble(addressLogDown, 0);
  EEPROM.updateInt(addressPhdelay, 5);
  EEPROM.updateInt(addressTube, 130);
  EEPROM.updateDouble(addressCentrePoint, 7);
  EEPROM.updateDouble(addressAlpha, -59.16);
  EEPROM.updateInt(addressR, 1);
  delay(2000);
  pHlow = EEPROM.readDouble(addressPhlow);
  pHhigh = EEPROM.readDouble(addressPhhigh);
  dose = EEPROM.readDouble(addressDose);
  alpha = EEPROM.readDouble(addressAlpha);
  CentrePoint = EEPROM.readDouble(addressCentrePoint);
  r = EEPROM.readInt(addressR);
  LogUp = EEPROM.readDouble(addressLogUp);
  LogDown = EEPROM.readDouble(addressLogDown);
  tube = EEPROM.readInt(addressTube);
  pHdelay = EEPROM.readInt(addressPhdelay);
  lcd.noBlink();
  lcd.setCursor(0, 0);
  lcd.print("Complete");
  delay(1000);
  lcd.clear();
  break;
  }
   if (m==10 && rsm==1) // RESET Log
  {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ResetLog");
  lcd.setCursor(0, 1);
  lcd.blink();
  EEPROM.updateDouble(addressLogUp, 0.00); 
  EEPROM.updateDouble(addressLogDown, 0.00);
  delay(2000);
  LogUp = EEPROM.readDouble(addressLogUp);
  LogDown = EEPROM.readDouble(addressLogDown);
  lcd.noBlink();
  lcd.setCursor(0, 0);
  lcd.print("Complete");
  delay(1000);
  lcd.clear();
  break;
  }
if (m==11){
  break;
}
if (m==12){
  break;
}
if (m==13){ //nulling seconds

  rtc.adjust(DateTime(yh, mo, dy, hr, mi, 0));

  lcd.clear();
  delay(200);
  break;
}
if (m==14){ //save date to actual
  
rtc.adjust(DateTime(ftyh, ftmo, ftdy, hr, mi, sc));
lcd.setCursor(0, 0);
lcd.print("  SAVE  ");
delay(1000);
ft=0;
  lcd.clear();
  delay(200);
  break;
}
if (m==15){
  break;
}
if (m==16 || m==17){ //mode hour or minute

sm++;
if (sm > 1)
sm=0;
delay(200);
  lcd.clear();
  break;
}
      }
      case BUTTON_NEXT:
      {
  m++;//увеличиваем переменную уровня меню

  if (m>17)//если уровень больше 3
  {
  m=0;// то вернуться к началу
  }

  delay (200);
  lcd.clear();
break;
      }
    
      
      case BUTTON_UP:
      {
            if (m==0) // Mode pH
        {
          r++;

  if (r > 1)
  {
  r = 1;
  }
  EEPROM.updateInt(addressR, r);
  delay (200);
  lcd.clear ();
         break;
      }
        if (m==1) // Log mode
        {
          lgm++;

  if (lgm > 1)
  {
  lgm=1;
  }
  delay (200);
  lcd.clear ();
         break;
      }
          if (m==2)
        {

          pHlow += 0.1;

  if (pHlow > pHhigh)
  {
  pHlow = pHhigh;
  }
  delay (200);
  lcd.clear ();
         break;
      }
       if (m==3)
        {
          pHhigh += 0.1;

  if (pHhigh > 14)
  {
  pHhigh = 14;
  } 
  delay (200);
  lcd.clear ();
         break;
      }
      if (m==4)
        {
          dose += 0.1;

  if (dose > 10)
  {
  dose = 10;
  }
  delay (200);
  lcd.clear ();
         break;
      }
      if (m==5) // ЗАДЕРЖКА
        {
          pHdelay++;

  if (pHdelay>60)
  {
  pHdelay+=60;
  }
  delay (200);
  lcd.clear ();
         break;
      }
      if (m==6) // ТРУБКА
        {
          tube++;

  if (tube>1000)
  {
  tube = 1000;
  }
  delay (200);
  lcd.clear ();
         break;
      }
       if (m==7) //                   ТЕСТ НАСОСА
        { 
tpm++;
if (tpm > 1)
tpm = 1;

   delay (200);
   lcd.clear ();
   break;

      }
      if (m==8) //                  ЦЕНТРАЛЬНАЯ ТОЧКА
        {
          CentrePoint += 0.01;

  if (CentrePoint > 14)
  {
  CentrePoint = 14;
  }
  delay (200);
  lcd.clear ();
         break;
      }
      if (m==9)
        {
          alpha -= 0.1;

  if (alpha > -30.00)
  {
  alpha = -30.00;
  }
  delay (200);
  lcd.clear ();
         break;
      }
  
  if (m==10) //                    reset
  {
  rsm++;
  if (rsm > 1)
  rsm = 1;
  delay (200);
  lcd.clear ();
  break;
  }
  
  if (m==11) //пропускаем
  {
  break;
  }
  
  if (m==12)// set hour
  {
  hr++;
  if (hr > 23)
  {
  hr = 0;
  }
  rtc.adjust(DateTime(yh, mo, dy, hr, mi, sc));
  delay (200);
  lcd.clear ();
  break;
  }
         if (m==13)
        {

          mi++;

  if (mi > 59)
  {
  mi = 0;
  }
  rtc.adjust(DateTime(yh, mo, dy, hr, mi, sc));
  delay (200);
  lcd.clear ();
         break;
        }
         if (m==14)
        {

          ft++;
          

  delay (200);
  lcd.clear ();
         break;
        }
   if (m==15)
        {

          RelayMode++;
     if (RelayMode>2)
RelayMode=2;
EEPROM.updateInt(addressRelayMode, RelayMode);

  delay (200);
  lcd.clear ();
         break;
        }
       if (m==16 && sm==0)
        {

          HrOn++;

  if (HrOn>23)
  
  HrOn = 0;
  EEPROM.updateInt(addressHrOn, HrOn);
  delay (200);
  lcd.clear ();
         break;
      }
       if (m==16 && sm==1)
        {

          MinOn++;
          
          if (MinOn > 59)
          MinOn = 0;
          EEPROM.updateInt(addressMinOn, MinOn);

  delay (200);
  lcd.clear ();
         break;
        }
      if (m==17 && sm==0)
        {

          HrOff++;
          
          if (HrOff > 23)
          HrOff = 0;
          EEPROM.updateInt(addressHrOff, HrOff);

  delay (200);
  lcd.clear ();
         break;
      }
         if (m==17 && sm==1)
           {

          MinOff++;
          
          if (MinOff > 59)
          MinOff = 0;
          EEPROM.updateInt(addressMinOff, MinOff);

  delay (200);
  lcd.clear ();
         break;
        }
         
      }
      
//------------------------- ДЛЯ УМЕНЬШЕНИЯ ПАРАМЕТРОВ -------------------------------
      
      case BUTTON_DOWN:
      {
         if (m==0) //                    РЕЖИМ
        {
          r--;

  if (r < 0)
  {
  r = 0;
  }
  EEPROM.updateInt(addressR, r);
  delay (200);
  lcd.clear ();
         break;
      }
       if (m==1) // Log mode
        {
          lgm--;

  if (lgm < 0)
  {
  lgm=0;
  }
  delay (200);
  lcd.clear ();
         break;
      }
          if (m==2)
        {
          pHlow -= 0.1;

  if (pHlow < 1)
  {
  pHlow = 1;
  }
  delay (200);
  lcd.clear ();
         break;
      }
       if (m==3)
        {
          pHhigh -= 0.1;

  if (pHhigh < pHlow)
  {
  pHhigh = pHlow;
  }
  delay (200);
  lcd.clear ();
         break;
      }
      if (m==4)
        {
          dose -= 0.1;

  if (dose < 0)
  {
  dose = 0;
  }
  delay (200);
  lcd.clear ();
         break;
      }
      if (m==5) // ЗАДЕРЖКА
        {
          pHdelay--;

  if (pHdelay>60)
  {
  pHdelay -= 60;
  }

  if (pHdelay<1)
      {
  pHdelay = 1;
  }
  delay (200);
  lcd.clear ();
         break;
      }
      if (m==6) // ТРУБКА
        {
          tube--;

  if (tube<1)
  {
  tube = 1;
  }
  delay (200);
  lcd.clear ();
         break;
      }
          if (m==7) //                   ТЕСТ НАСОСА
        {
tpm--;
if (tpm < 0)
tpm=0;
   delay (200);
   break;

      }
      if (m==8) //                  ЦЕНТРАЛЬНАЯ ТОЧКА
        {
          CentrePoint -= 0.01;

  if (CentrePoint < 1)
  {
  CentrePoint = 1;
  }
  delay (200);
  lcd.clear ();
         break;
      }
      if (m==9)
        {
          alpha += 0.1;

  if (alpha < -90.00)
  {
  alpha = -90.00;
  }
  delay (200);
  lcd.clear ();
         break;
      }
     
      if (m==10){ // reset 
  rsm--;
  if (rsm < 0)
  rsm = 0;
  delay (200);
  lcd.clear ();
  break;
      }
        if (m==11){ // пропускаем
  break;
      }
  
     if (m==12)
        {

          hr--;

  if (hr < 0)
  {
  hr = 23;
  }
  rtc.adjust(DateTime(yh, mo, dy, hr, mi, sc));
  delay (200);
  lcd.clear ();
         break;
        }
         if (m==13)
        {

          mi--;

  if (mi < 0)
  {
  mi = 59;
  }
  rtc.adjust(DateTime(yh, mo, dy, hr, mi, sc));
  delay (200);
  lcd.clear ();
         break;
        }
          if (m==14)
        {
         ft--;
  delay (200);
  lcd.clear ();
         break;
      }
       if (m==15)
        {

          RelayMode--;
     if (RelayMode<0)
RelayMode=0;
EEPROM.updateInt(addressRelayMode, RelayMode);

  delay (200);
  lcd.clear ();
         break;
        }
        if (m==16 && sm==0)
        {

          HrOn--;
          
          if (HrOn < 0)
          HrOn = 23;
          EEPROM.updateInt(addressHrOn, HrOn);

  delay (200);
  lcd.clear ();
         break;
        }
         if (m==16 && sm==1)
        {

          MinOn--;
          
          if (MinOn < 0)
          MinOn = 59;
          EEPROM.updateInt(addressMinOn, MinOn);

  delay (200);
  lcd.clear ();
         break;
        }
        if (m==17 && sm==0)
        {

          HrOff--;
          
          if (HrOff < 0)
          HrOff = 23;
          EEPROM.updateInt(addressHrOff, HrOff);

  delay (200);
  lcd.clear ();
         break;
        }
         if (m==17 && sm==1)
        {

          MinOff--;
          
          if (MinOff < 0)
          MinOff = 59;
          EEPROM.updateInt(addressMinOff, MinOff);

  delay (200);
  lcd.clear ();
         break;
        }

      }
       case BUTTON_PREV:
     {
        m--;//увеличиваем переменную уровня меню

  if (m<0)//если уровень больше 3
  {
  m=17;// то вернуться к началу
  }

  delay (200);
  lcd.clear();
      }
     
      default:
     {
        break;
     }
   }

  if (m==0)
  {
  lcd.setCursor(0, 0);
  lcd.print("pH ");
  lcd.setCursor(4, 0);
  lcd.print(pH);
  lcd.setCursor(0, 1);
  lcd.print(pHmode[r]);
  } 
  else if (m==1)// view Log File
  {
  lcd.setCursor(0, 0);
  lcd.print("Log ");
  if (lgm==0)
  {
  lcd.print("Down");
  lcd.setCursor(0, 1);
  lcd.print(LogDown);
  lcd.print("ml");
  if (LogDown > 1000)
  {
  lcd.print(LogDown/1000);
  lcd.print(" l");
  }
  }
  else if (lgm==1)
  {
  lcd.print("Up");
  lcd.setCursor(0, 1);
  lcd.print(LogUp);
  lcd.print("ml");
  if (LogUp > 1000)
  {
  lcd.print(LogUp/1000);
  lcd.print(" l");
  }
  }
  } 
  else if (m==2)
  {
  lcd.setCursor(0, 0);
  lcd.print("pH low");
  lcd.setCursor(0, 1);
  lcd.print("");
  lcd.print(pHlow);
  }
  
 else if (m==3)
  {
  lcd.setCursor(0, 0);
  lcd.print("pH high");
  lcd.setCursor(0, 1);
  lcd.print("");
  lcd.print(pHhigh);
  }
 else if (m==4)
  {
  lcd.setCursor(0, 0);
  lcd.print("dose");
  lcd.setCursor(0, 1);
  lcd.print(dose);
  lcd.print(" ml");
  }
  else if (m==5)
  {
  lcd.setCursor(0, 0);
  lcd.print("delay");
  lcd.setCursor(0, 1);
  if (pHdelay<60)
  {
  lcd.print(pHdelay);
  lcd.print(" min");
  }
   if (pHdelay>=60)
  {
  lcd.print(pHdelay/60);
  lcd.print(" hor");
  }
  
 }
  else if (m==6)
  {
  lcd.setCursor(0, 0);
  lcd.print("tube");
  lcd.setCursor(5, 0);
  lcd.print(tube);
  lcd.setCursor(0, 1);
  lcd.print("ml/min");

  }
   else if (m==7)
  {
  lcd.setCursor(0, 0);
  lcd.print("TestPump");
  lcd.setCursor(0, 1);
  if (tpm==0)
  lcd.print("pH Down");
  if (tpm==1)
  lcd.print("pH Up");
  }
   else if (m==8)
  {
  lcd.setCursor(0, 0);
  lcd.print("IP");
  lcd.setCursor(4, 0);
  lcd.print(CentrePoint);
  lcd.setCursor(0, 1);
  lcd.print("pH");
  lcd.setCursor(4, 1);
  lcd.print(pH);
  }
  else if (m==9)
  {
  lcd.setCursor(0, 0);
  lcd.print("mV");
  lcd.setCursor(4, 0);
  lcd.print(-1*alpha);
  lcd.setCursor(0, 1);
  lcd.print("pH");
  lcd.setCursor(4, 1);
  lcd.print(pH);
  }
  else if (m==10)
  {
  lcd.setCursor(0, 0);
  lcd.print("Reset");
  lcd.setCursor(0, 1);
  if (rsm==0)
  lcd.print("All");
  if (rsm==1)
  lcd.print("Log");
  }
 
  
//------------------------TIMER-----------------------
   if (m==11)
  {   
      lcd.setCursor (0,0);
   if (hr<10)
    lcd.print('0');
     lcd.print(hr);
    lcd.print(':');
    if (mi<10)
    lcd.print('0');
    lcd.print(mi);
    lcd.print(':');
    if (sc<10)
    lcd.print('0');
    lcd.print(sc);  
    lcd.setCursor (0,1);
    if (dy<10)
    lcd.print('0');
    lcd.print(dy);
    lcd.print('/');
     if (mo<10)
    lcd.print('0');
    lcd.print(mo);
    lcd.print('/');
    int y = (yh-2000);
    lcd.print(y);

  } 
  else if (m==12)
  {

      lcd.setCursor(0, 0);
  lcd.print("Set Hour");
   lcd.setCursor(0, 1);
  if (hr<10)
    lcd.print('0');
 lcd.print(hr);
 lcd.print(':');
    if (mi<10)
    lcd.print('0');
    lcd.print(mi);
    lcd.print(':');
    if (sc<10)
    lcd.print('0');
    lcd.print(sc);  
  
  }
  
  else if (m==13)
  {  
  lcd.setCursor(0, 0);
  lcd.print("Set Min");
  lcd.setCursor(0, 1);
  if (hr<10)
  lcd.print('0');
  lcd.print(hr);
  lcd.print(':');
  if (mi<10)
  lcd.print('0');
  lcd.print(mi);
  lcd.print(':');
  if (sc<10)
  lcd.print('0');
  lcd.print(sc);  
  }
 else if (m==14)
  {
  lcd.setCursor(0, 0);
  lcd.print("Set Date");
  lcd.setCursor (0,1);
  if (ftdy<10)
  lcd.print('0');
  lcd.print(ftdy);
  lcd.print('/');
  if (ftmo<10)
  lcd.print('0');
  lcd.print(ftmo);
  lcd.print('/');
  int y = (ftyh-2000);
  lcd.print(y);
  }
 else if (m==15)
  {
  lcd.setCursor(0, 0);
  lcd.print("SetRelay");
  lcd.setCursor(0, 1);
  lcd.print(Rmode[RelayMode]);
  }
  else if (m==16)//время включения реле
 {
   lcd.setCursor(0, 0);
  lcd.print("Reley On");
  
  if (sm==0)
  {
  lcd.setCursor(0, 1);
  lcd.print(">");
  }
  lcd.setCursor(1, 1);
   if (HrOn<10)
    lcd.print('0');
  lcd.print(HrOn);
  lcd.print(':');
  if (MinOn<10)
    lcd.print('0');
   lcd.print(MinOn);
   if (sm==1)
  lcd.print("<");
  }
  else if (m==17)//время выключения реле
  {
 lcd.setCursor(0, 0);
  lcd.print("ReleyOff");
  
  if (sm==0)
  {
  lcd.setCursor(0, 1);
  lcd.print(">");
  }
  lcd.setCursor(1, 1);
   if (HrOff<10)
    lcd.print('0');
  lcd.print(HrOff);
  lcd.print(':');
  if (MinOff<10)
    lcd.print('0');
   lcd.print(MinOff);
   if (sm==1)
  lcd.print("<");
  }
  

   if( buttonJustPressed )
      buttonJustPressed = false;
   if( buttonJustReleased )
      buttonJustReleased = false;



  
  if (millis()-dTime>=dInterval)
  {  
    dTime = millis();

    if (dmi != mi)
    {
      d++;
      dmi=mi;
    }
      if (d>=pHdelay)
      {

    
if (pHlow>pH && r==0)               // if the automatic mode is selected and the pH dropped
                                               // Parameter below pH Low
{

  digitalWrite (pumpPhup,HIGH);  // turn on the pump pH UP
    
  delay (dose*60000/tube);                          
                               
  digitalWrite (pumpPhup,LOW);                 // turn off the pump
  float U1timepump = EEPROM.readDouble(addressLogUp);
  EEPROM.updateDouble(addressLogUp, (U1timepump+dose));
  LogUp = EEPROM.readDouble(addressLogUp);
  }
       



  if (pHhigh<pH && r==0)           // if the automatic mode is selected and the pH rose
                                              // Parameter above pH High
{

  digitalWrite (pumpPhdown,HIGH);
  delay (dose*60000/tube); 
  digitalWrite (pumpPhdown,LOW);
  float D1timepump = EEPROM.readDouble(addressLogDown);
  EEPROM.updateDouble(addressLogDown, (D1timepump+dose));
  LogDown = EEPROM.readDouble(addressLogDown);
  }
        d = 0;
    }
   
}
}
/*--------------------------------------------------------------------------------------
  ReadButtons()
  Detect the button pressed and return the value
  Uses global values buttonWas, buttonJustPressed, buttonJustReleased.
--------------------------------------------------------------------------------------*/
byte ReadButtons()
{
   unsigned int buttonVoltage;
   byte button = BUTTON_NONE;   // return no button pressed if the below checks don't write to btn
   
   //read the button ADC pin voltage
   buttonVoltage = analogRead( BUTTON_ADC_PIN );
   //sense if the voltage falls within valid voltage windows
   if( buttonVoltage < ( SAVE_10BIT_ADC + BUTTONHYSTERESIS ) )
   {
      button = BUTTON_SAVE;
   }
   else if(   buttonVoltage >= ( NEXT_10BIT_ADC - BUTTONHYSTERESIS )
           && buttonVoltage <= ( NEXT_10BIT_ADC + BUTTONHYSTERESIS ) )
   {
      button = BUTTON_NEXT;
   }
   else if(   buttonVoltage >= ( UP_10BIT_ADC - BUTTONHYSTERESIS )
           && buttonVoltage <= ( UP_10BIT_ADC + BUTTONHYSTERESIS ) )
   {
      button = BUTTON_UP;
   }
   else if(   buttonVoltage >= ( DOWN_10BIT_ADC - BUTTONHYSTERESIS )
           && buttonVoltage <= ( DOWN_10BIT_ADC + BUTTONHYSTERESIS ) )
   {
      button = BUTTON_DOWN;
   }
   else if(   buttonVoltage >= ( PREV_10BIT_ADC - BUTTONHYSTERESIS )
           && buttonVoltage <= ( PREV_10BIT_ADC + BUTTONHYSTERESIS ) )
   {
      button = BUTTON_PREV;
   }
   //handle button flags for just pressed and just released events
   if( ( buttonWas == BUTTON_NONE ) && ( button != BUTTON_NONE ) )
   {
      //the button was just pressed, set buttonJustPressed, this can optionally be used to trigger a once-off action for a button press event
      //it's the duty of the receiver to clear these flags if it wants to detect a new button change event
      buttonJustPressed  = true;
      buttonJustReleased = false;
   }
   if( ( buttonWas != BUTTON_NONE ) && ( button == BUTTON_NONE ) )
   {
      buttonJustPressed  = false;
      buttonJustReleased = true;
   }
   
   //save the latest button value, for change event detection next time round
   buttonWas = button;
   
   return( button );
}

// read pH

 float ADSread()
    {

           Wire.requestFrom(ads1110, 3);
 while(Wire.available()) // ensure all the data comes in
 {
 highbyte = Wire.read(); // high byte * B11111111
 lowbyte = Wire.read(); // low byte
 configRegister = Wire.read();
 }

 data = highbyte * 256;
 data = data + lowbyte;

 voltage = data * 2.048 ;
 voltage = voltage / 327.68; // mV
 return voltage;
    }

