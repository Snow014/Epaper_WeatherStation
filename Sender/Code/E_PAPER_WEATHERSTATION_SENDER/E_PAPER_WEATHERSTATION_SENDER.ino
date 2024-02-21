#include <SoftwareSerial.h>
#include "Adafruit_SHT4x.h"

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#include <ESP8266WiFi.h>

const byte switchPin = 14;
const int switchDelay = 100;

SoftwareSerial HC12(13, 12);      //RX - TX

Adafruit_SHT4x sht4 = Adafruit_SHT4x();
Adafruit_BME280 bme;
//  RTC_PCF8563 rtc;

#define LED_BUILTIN D4

void setup(){
  //  pinMode(LED_BUILTIN, OUTPUT);
  //  digitalWrite(LED_BUILTIN, LOW); //  turn on activity indicator

  WiFi.mode(WIFI_OFF);

  pinMode(switchPin, OUTPUT);

  digitalWrite(switchPin, HIGH);
  delay(50);  //  give voltage time to stabilise

  Wire.begin(5, 4);    //needed for specific wemos, that has I2C pins turned around :P

  Serial.begin(115200);

  HC12.begin(2400);

  bme.begin(0x76, &Wire);
  bme.setSampling(Adafruit_BME280::MODE_FORCED,
                    Adafruit_BME280::SAMPLING_X1, // temperature
                    Adafruit_BME280::SAMPLING_X1, // pressure
                    Adafruit_BME280::SAMPLING_X1, // humidity
                    Adafruit_BME280::FILTER_OFF   );

  sht4.begin();
  sht4.setPrecision(SHT4X_HIGH_PRECISION);
  sht4.setHeater(SHT4X_NO_HEATER);
}

void loop(){
  
  String payload = getData();
  HC12.println(payload);
  HC12.flush(); //  waiiiiiiit for data to send
  
  Serial.println(payload);

  delay(switchDelay);
  digitalWrite(switchPin, LOW);

  //  prevent phantom power on I2C lines
  pinMode(5, INPUT);
  pinMode(4, INPUT);

  //  digitalWrite(LED_BUILTIN, HIGH);  //  turn off activity indicator
  
  ESP.deepSleep(300e6, WAKE_RF_DISABLED); //sleep for 5 min and don't turn on wifi
  //  this will never be reached.
}

String getData(){

  sensors_event_t humidity, temp;
  sht4.getEvent(&humidity, &temp);

  String data = String(temp.temperature) + "|";

  //  meassure humidity with high heater, NEEDED IN WINTER ONLY
  sht4.setHeater(SHT4X_HIGH_HEATER_1S);
  sht4.getEvent(&humidity, &temp);

  bme.takeForcedMeasurement();

  data += String(humidity.relative_humidity) + "|";
  data += String(int(bme.readPressure() / 100.0F)) + "|";

  //  read battery 
  //  data += String(analogRead(A0));

  return data;
}