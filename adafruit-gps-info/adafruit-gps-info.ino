#include <Time.h>
#include <TimeLib.h>
#include <Adafruit_GPS.h>
#include <SoftwareSerial.h>
#include <LiquidCrystal_PCF8574.h>

#define GPSECHO  false

const char TIMEZONE_NAMES[2][6] = { "India", "Sydney" }; //IST, SYDNEY
const float TIMEZONES[2] = { 5.5, 10 }; //IST, SYDNEY
int dotCount = 0;

boolean usingInterrupt = true;
//LCD Vars
LiquidCrystal_PCF8574 lcd(0x27);
char LINES[2][17] = {"", ""};
uint32_t busyMsgTimer = millis();
uint32_t fixDataUpdateTimer = millis();

int opMode = 0; //Output Mode
uint32_t opModeTimer = millis();

// GPS vars
SoftwareSerial mySerial(3, 2);
Adafruit_GPS GPS(&mySerial);  //TX,RX

//displayDateTimeVars

int TIMEZONE = 1;  // SYDNEY
int DAYLIGHT = 0;
uint32_t GPSSYNC_TIME = millis();
boolean timeSynced = false;

char buff[11] = "";

void setup() {
  pinMode(6, INPUT);
  pinMode(7, INPUT);
  
  //Serial init
  Serial.begin(9600);

  //LCD Init
  lcd.begin(16, 2);
  lcd.setBacklight(150);
  lcd.setCursor(0, 0);
  lcd.print("Booting...");
  
  GPS.begin(9600);
  //GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  delay(1000);
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  delay(1000);
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);
  useInterrupt(true);
}

void changeOutputMode() {
  if(millis() - opModeTimer > 1000) {
    int tmpMode;
    int pin6 = digitalRead(6);
    int pin7 = digitalRead(7);
    if(pin6 == 0 && pin7 == 0) {
      tmpMode=0;
    } else if(pin6 == 0 && pin7 == 1) {
      tmpMode=1;
    } else if(pin6 == 1 && pin7 == 0) {
      tmpMode = 2;
    } else {
      return;
    }

    if(tmpMode != opMode) {
      opMode = tmpMode;
      opModeTimer = millis();
      Serial.println(opMode);
    }
  }
}

void loop() {
  changeOutputMode();
  if (GPS.newNMEAreceived()) {
    if (!GPS.parse(GPS.lastNMEA())) // this also sets the newNMEAreceived() flag to false
      return; // we can fail to parse a sentence in which case we should just wait for another
  }

  if(opMode == 0 && timeSynced) {
    updateTime();
  } else if ((opMode>0 || !timeSynced) && GPS.fix) {
    setSystemTime();
    if((millis() - fixDataUpdateTimer)>=500) {
      switch(opMode) {
        case 1:
          updateSpeed();
          break;
        case 2:
          updatePos();
          break;
      }

      updateDisplay();
      fixDataUpdateTimer = millis();
    }
  } else {
    busyDisplay();
  }
}

void updatePos() {
  dtostrf(GPS.latitudeDegrees, 4, 6, buff);
  strcpy(LINES[0], "Lat:");
  strcat(LINES[0], buff);

  strcpy(buff, "");
  dtostrf(GPS.longitudeDegrees, 4, 6, buff);
  strcpy(LINES[1], "Lon:");
  strcat(LINES[1], buff);
}

void updateTime() {
  char TMP_DISPLAY[16]="";
  if(timeSynced) {
    //sprintf(LINES[0],"%02d%s%d %02d:%02d ",day(),MONTH_SHORT[(month()-1)],year(),hour(),minute());
    sprintf(TMP_DISPLAY,"%02d-%02d-%d %02d:%02d ",day(),month(),year(),hour(),minute());
    //strcpy(LINES[1], TIMEZONE_NAMES[TIMEZONE]);
  }
  if((strcmp(TMP_DISPLAY, LINES[0]) != 0)) {
    strcpy(LINES[0], TMP_DISPLAY);
    strcpy(LINES[1], TIMEZONE_NAMES[TIMEZONE]);
    updateDisplay();
  }
}

void setSystemTime() {
  if(!timeSynced || (millis() - GPSSYNC_TIME > 5000)) {
    setTime(GPS.hour, GPS.minute, GPS.seconds, GPS.day, GPS.month, GPS.year);
    adjustTime((TIMEZONES[TIMEZONE]+DAYLIGHT) * SECS_PER_HOUR);
    timeSynced = true;
    GPSSYNC_TIME = millis();
  }
}

void updateSpeed() {
  char buff[5];
  char gpsSpeed[6];
  dtostrf((GPS.speed * 1.48), 4, 2, gpsSpeed);
  dtostrf(GPS.HDOP,2,2,buff);
  strcpy(LINES[0], "Ac/Sat:");
  strcat(LINES[0], buff);
  strcpy(buff, "");
  itoa(GPS.satellites ,buff, 5);
  strcat(LINES[0], "/");
  strcat(LINES[0], buff);
  
  strcpy(LINES[1], gpsSpeed);
  strcat(LINES[1]," Kmph");
}

void busyDisplay() {
  if(millis() - busyMsgTimer >= 1000) {
    switch(opMode) {
      case 0:
        strcpy(LINES[0], "Date Time");
        break;
      case 1:
        strcpy(LINES[0], "Speed");
        break;
      case 2:
        strcpy(LINES[0], "Location");
        break;
    }
    
    strcpy(LINES[1], "Fix");
    for(int i=0;i<dotCount;i++)
      strcat(LINES[1], ".");
    
    dotCount++;
    if(dotCount == 4)
      dotCount = 0;

    busyMsgTimer = millis();
    updateDisplay();
  }
}

void updateDisplay() {
  updateDisplay(0);
  updateDisplay(1);
}

void updateDisplay(int line) {
  lcd.setCursor(0, line);
  int len = strlen(LINES[line]);
  if (len == 16) {
    //do nothing
  } else if(len < 16) {
    for (int i = len; i < 16; i++)
      strcat(LINES[line], " ");
  } else {
    // do nothing
  }
  lcd.print(LINES[line]);
  //Serial.println(LINES[line]);
}

SIGNAL(TIMER0_COMPA_vect) {
  char c = GPS.read();
  // if you want to debug, this is a good time to do it!
  if (GPSECHO)
    if (c) UDR0 = c;  
    // writing direct to UDR0 is much much faster than Serial.print 
    // but only one character can be written at a time. 
}

void useInterrupt(boolean v) {
  if (v) {
    // Timer0 is already used for millis() - we'll just interrupt somewhere
    // in the middle and call the "Compare A" function above
    OCR0A = 0xAF;
    TIMSK0 |= _BV(OCIE0A);
    usingInterrupt = true;
  } else {
    // do not call the interrupt function COMPA anymore
    TIMSK0 &= ~_BV(OCIE0A);
    usingInterrupt = false;
  }
}
