#include "RTClib.h"
RTC_DS3231 rtc;

#include <Wire.h>

#define rainPin  4

const float rainPerTick = 0.2794; //  mm of rain per signal from sensor
float rainTicks = 0;

bool rainPulse = false;
unsigned long buttonTime = 0;
unsigned long lastButton = 0;


unsigned long lastRainUpdate = 0;
unsigned long sleepMillis = 0;


bool lightningPulse = false;
//  not used yet
void IRAM_ATTR lightningISR(){
  Serial.println("WIP..");
  lightningPulse = true;
}

void IRAM_ATTR rainISR(){
  buttonTime = millis();

  if(buttonTime - lastButton > 100){
    rainPulse = true;
    lastButton = buttonTime;
  }
}

void setup(){
  Serial.begin(9600);
  Serial.println(""); //  to activate serial?

  Wire.begin(8, 9);

  if(!rtc.begin()){
    Serial.println("Clock issue!");
  }
  Serial.println(getTime());

  Serial1.begin(2400, SERIAL_8N1, 5, 6);
  attachInterrupt(rainPin, rainISR, FALLING);
}

void loop(){
  while(Serial.available()){
    String data = Serial.readStringUntil('\n');
    if(data.equalsIgnoreCase("send")){

      String rainString = "r" + String(rainTicks * rainPerTick, 2) + "|";
      rainString += getTime();

      Serial.println(rainString);
    }
    else if(data.equalsIgnoreCase("clear")){
      Serial.println("resetting count.");
      rainTicks = 0;  //  reset count
    }
  }

  //  send data every 30 min
  if(millis() - lastRainUpdate > sleepMillis){
    DateTime now = rtc.now();
    if(now.minute() == 0 || now.minute() == 30){
      
      String rainString = "r" + String(rainTicks * rainPerTick, 2) + "|";
      rainString += getTime();

      Serial.println(rainString);
      Serial1.println(rainString);
    }
    
    sleepMillis = 1800 - ((( now.minute() * 60 ) + now.second() ) % 1800);
    sleepMillis *= 1000;

    Serial.println("Sleeping for " + String(sleepMillis) + " milliseconds.");

    lastRainUpdate = millis();
    rainTicks = 0;
  }

  //  count up
  if(rainPulse){
    rainPulse = false;

    rainTicks++;
    Serial.println(rainTicks);
  }
}

String getTime() {
  DateTime now = rtc.now();
  String timeString;

  timeString = String(now.hour());
  timeString += ":";
  if (now.minute() < 9) {
    timeString += "0";
  }

  timeString += String(now.minute());

  return timeString;
}