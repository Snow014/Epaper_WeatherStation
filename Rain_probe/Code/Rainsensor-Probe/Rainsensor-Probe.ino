//  LIGHTNING SENSING IS CURRENTLY DISABLED ON RECIEVER

#include "DFRobot_AS3935_I2C.h"

#define LIGHTNING_INTERRUPT D0 
#define LIGHTNING_I2C 0x03

DFRobot_AS3935_I2C  lightningSensor(LIGHTNING_INTERRUPT, LIGHTNING_I2C);
#define AS3935_CAPACITANCE   96

#include "DFRobot_RainfallSensor.h"
DFRobot_RainfallSensor_I2C Sensor(&Wire);

#include "RTClib.h"
RTC_DS3231 rtc;

#include <Wire.h>
const byte lightningInt = D0;

#define HOUR          3600
#define HALF_HOUR     1800
#define QUARTER_HOUR  900

#define SEND_INTERVAL HALF_HOUR

unsigned long interval;

unsigned long last;

const byte dataPin = D8;
float rainOld;

TaskHandle_t lightningTask;
bool hasLightning = false;
String lightningString;

void setup(){
  Serial.begin(115200);
  Serial2.begin(2400, SERIAL_8N1, D9, D10);

  bool rainState = Sensor.begin();
  bool clockState = rtc.begin();

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  //  turn on yellow LED if either RTC or rainsensor didn't start up
  if(!rainState || !clockState){ 
    digitalWrite(LED_BUILTIN, LOW);
    delay(5000);
    ESP.restart(); 
  }

  //  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  
  DateTime now = rtc.now();
  Serial.println(String(now.hour()) + ":" + String(now.minute()));

  pinMode(dataPin, OUTPUT);
  digitalWrite(dataPin, HIGH);

  Serial2.println("");

  //  set up second core to check lightning
  xTaskCreatePinnedToCore(
    handleLightning,    /* Task function. */
    "Task1",            /* name of task. */
    10000,              /* Stack size of task */
    NULL,               /* parameter of the task */
    1,                  /* priority of the task */
    &lightningTask,     /* Task handle to keep track of created task */
    0);                 /* pin task to core 0 */

}

void loop(){
  // check lightning flag, send data if there is lightning
  if(hasLightning){
    hasLightning = false;

    Serial.println("LIGHTNING!");
    Serial.println(lightningString);

    Serial2.print(lightningString);
    //  for debugging, set string to "NONE" after having handled the strike.
    lightningString = "NONE";
  }

  //  handle serial messages
  while(Serial.available()){
    String data = Serial.readStringUntil('\n');

    //  handle command
    if(data.indexOf("AT") == 0){
      Serial.println("> " + data);

      digitalWrite(dataPin, LOW);
      delay(100);
      Serial2.println(data);
      delay(100);
      digitalWrite(dataPin, HIGH);
    }
    else if(data.indexOf("checkR") == 0){
      Serial.println("> " + data);
      Serial.println("< " + String(Sensor.getRainfall(1)));
    }
    else{
      Serial.println("> " + data);
      Serial2.println(data);
    }
  }

  //  handle HC-12 messages
  while(Serial2.available()){
    String data = Serial2.readStringUntil('\n');
    Serial.println("< " + data);
  }

  //  handle collecting and sending rain data every 30 min.
  if(millis() - last > interval){

    DateTime now = rtc.now();

    //  temporary workaround of the "o9 bug"
    if(now.minute() == 0 || now.minute() == 30){
      Serial.println("Checking rainfall..");

      float rainNew = Sensor.getRainfall();
      String rainString = "r" + String(rainNew - rainOld) + "|";
      rainOld = rainNew;

      //  append time
      rainString += getTime();

      Serial.println(rainString);
      Serial2.println(rainString);
    }
    else{
      Serial.println("o9 bug, skipping reading!");
    }
    //  wake up on the next hour
    // interval = HOUR - ((now.minute() * 60) + now.second());
    // interval *= 1000;
    // 
    
    //  wake up on the next half hour
    interval = 1800 - ((( now.minute() * 60 ) + now.second() ) % 1800);
    interval *= 1000;

    Serial.println("Sleeping for " + String(interval) + " milliseconds.");

    last = millis();
  }
}

void handleLightning(void* pvParameters){

  pinMode(lightningInt, INPUT);

  (lightningSensor.defInit()) ? Serial.println("Lightning sensor started!") : Serial.println("Lightning sensor error!"); 
  lightningSensor.manualCal(AS3935_CAPACITANCE, 0, 1);

  for (;;) {
    //  if we do detect lightning, prepare a string with all the data, and set the flag so the main core can take over.
    if(digitalRead(lightningInt == HIGH)){
      int intVal = lightningSensor.getInterruptSrc();

      //  LIGHTNING!
      if(intVal == 1){
        int distance = lightningSensor.getLightningDistKm();

        lightningString = "rl" + String(distance, 0) + "|" + getTime();

        hasLightning = true;
      }
      else if(intVal == 2){ Serial.println("Disturber!"); }
      else if(intVal == 3){ Serial.println("Noise floor too high!"); }

    }
    delay(1);  //  everyone needs time for themselves, even MCU's
  }
}

//  returns time as a string in format HH:MM
String getTime(){
  DateTime now = rtc.now();
  String timeString;

  timeString = String(now.hour());
  timeString += ":";
  if(now.minute() < 9){
    timeString += "0";
  }

  timeString += String(now.minute());

  return timeString;
}