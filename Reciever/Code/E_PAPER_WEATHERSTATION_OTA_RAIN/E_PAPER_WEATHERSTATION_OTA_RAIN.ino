#define ENABLE_GxEPD2_GFX 1

#include <U8g2_for_Adafruit_GFX.h>
#include <Wire.h>

#include "RTClib.h"
RTC_DS3231 rtc;

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include "NTP.h"
WiFiUDP wifiUdp;
NTP ntp(wifiUdp);

//-----------------------------[SETTINGS]-----------------------------
const char* ssid = "";          //  OTA Wifi SSID
const char* password = "";//  OTA wifi password

const bool drawDebug = false;         //  display debug info instead of UI
const bool wipeOnStartup = true;      //  clear screen on startup to prevent old data showing

//  #define USEBWR                    //  choose between black/white screen or black/white/red screen

const int graphSize = 144;            //  how many datapoints in the graphs
const byte dataFreq = 5;              //  how often data arrives ( in minutes )
const bool drawBlackGraph = true;     //  enable/disable using red lines for graph
const float scaleMargin = 0.5;        //  small offset between min/max and actual data, to ensure lines stay in the graph

const byte dataCnt = 4;               //  how many diffrent datapoints are contained in the recieved data string

const byte resetHr = 0;               // what full hour to reset graphs and min/max at, 0 - 23
const byte clearCycleHr = 3;          // what full hour to do a clear cycle at, to minimise burn-in

const bool portOnRight = true;        // if true, flips screen orientation

const bool forceZambrettiRange = true;//  force pressure to be in-range for zambretti calculations
//--------------------------------------------------------------------

/*  ---E-PAPER CONNECTIONS FOR ESP32---
 * BUSY - 4
 * RST  - 15
 * DC   - 27
 * CS   - 5
 * CLK  - 18
 * DIN  - 23
 * GND  - GND
 * VCC  - 3.3V
 */

#ifdef USEBWR
  #include <GxEPD2_3C.h>  //  B/W/R
  GxEPD2_3C<GxEPD2_420c, GxEPD2_420c::HEIGHT> display(GxEPD2_420c(/*CS=5*/ SS, /*DC=*/27, /*RST=*/15, /*BUSY=*/4)); //  B/W/R screen constructor
  #warning "Black/White/Red display selected!"
#else
  #include <GxEPD2_BW.h>  //  B/W
  GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT> display(GxEPD2_420(/*CS=5*/ SS, /*DC=*/ 27, /*RST=*/ 15, /*BUSY=*/ 4)); //  B/W screen constructor
  //  drawBlackGraph = true;
  #warning "Black/White display selected!"
#endif

#define GxEPD2_DRIVER_CLASS GxEPD2_420c
#define colorBlack GxEPD_BLACK
#define colorWhite GxEPD_WHITE
#define colorRed GxEPD_RED
 
#define fontBig u8g2_font_inr33_mr            //33px
#define fontMid u8g2_font_lubR24_te           //25px
#define fontSml u8g2_font_fur20_tr            //20px
#define fontSpecial u8g2_font_t0_22_mf        //13px
#define fontxSml u8g2_font_helvB08_tf
 
#define batteryFont u8g2_font_battery19_tn
 
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

bool updateUI = false;
byte UIIndex = 0;
const byte UImax = 2;
int touchThresh = 0;

const int numYMarkers = 4;
const int numLines = 10;
 
float gDataT[graphSize];
float gDataH[graphSize];
float gDataP[graphSize];


const int rainGraphSize = 24; //  2 readings / hour
const int maxPadding = 0;
const byte numBarLines = 4;
const byte minScale = 5;
float gDataR[rainGraphSize];

String gDataTime[graphSize];

unsigned long lastRecieved = 0;
bool filled = false;
bool resetMinMax = true;
bool ranClearCycle = false;

float tMin = 0;
float tMax = 0;
float hMin = 0;
float hMax = 0;
float pMin = 0;
float pMax = 0;
float dewPoint = 0;

float rainAvg, rainLast, rainTotal;

String minMaxTimes[3][2];

String rawData = "";
String dataTime = "00:00";  //  timestamp of last probe data
String rainTime = "00:00";  //  timestamp of last rain data

bool showLightningData = false;
TaskHandle_t handleOTA;

bool wifiConnected = false;

enum textAligns { LEFT, CENTER, RIGHT };

const float elevation = 7.0;  //  elevation, in meters
byte currentMonth;

const String zambretti[26]  = {"Settled fine", "Fine weather", "Becoming fine", "Fine, becoming less settled", "Fine, possible showers", "Fairly fine, improving", "Fairly fine, possible showers early", "Fairly fine, showery later", "Showery early, improving", "Changeable, mending", "Fairly fine, showers likely", "Rather unsettled clearing later", "Unsettled, probably improving", "Showery, bright intervals", "Showery, becoming less settled", "Changeable, some rain", "Unsettled, short fine intervals", "Unsettled, rain later", "Unsettled, some rain", "Mostly very unsettled", "Occasional rain, worsening", "Rain at times, very unsettled", "Rain at frequent intervals", "Rain, very unsettled", "Stormy, may improve", "Stormy, much rain"};

const byte rise_options[22] = {25,25,25,24,24,19,16,12,11,9,8,6,5,2,1,1,0,0,0,0,0,0};
const byte steady_options[22] = {25,25,25,25,25,25,23,23,22,18,15,13,10,4,1,1,0,0,0,0,0,0};
const byte falling_options[22] = {25,25,25,25,25,25,25,25,23,23,21,20,17,14,7,3,1,1,1,0,0,0};

const int z_min = 950;
const int z_max = 1050;


void setup() {
  Serial.begin(115200);
  //  Serial1.begin(115200, SERIAL_8N1, -1, 33);  //  rain info screen
  Serial2.begin(2400);  //  HC-12
  
  Wire.begin();

  rtc.begin();
  syncRTC("Wifi24", "mjv3kncyKkyk");

  // xTaskCreatePinnedToCore(
  //                   OTA,         /* Task function. */
  //                   "Task1",     /* name of task. */
  //                   10000,       /* Stack size of task */
  //                   NULL,        /* parameter of the task */
  //                   1,           /* priority of the task */
  //                   &handleOTA,  /* Task handle to keep track of created task */
  //                   0);          /* pin task to core 0 */

  display.init(0, true, 2, false);
  u8g2Fonts.begin(display);

  // set screen approperiate screen orientation
  if(!portOnRight){ display.setRotation(2); }

  u8g2Fonts.setForegroundColor(colorBlack); // apply Adafruit GFX color
  u8g2Fonts.setBackgroundColor(colorWhite);

  // for(int i = 0; i < graphSize; i++){
  //   gDataT[i] = random(20);
  //   gDataP[i] = random(20);
  //   gDataH[i] = random(100);
  // }
  
  // for(int i = 0; i < rainGraphSize; i++){
  //   gDataR[i] = 10;
  // }

  if(wipeOnStartup){

    DateTime now = rtc.now();
    String startTime = String(now.hour()) + ":";
    if(now.minute() < 10) { startTime += "0"; }
    startTime += now.minute();

    display.fillScreen(GxEPD_WHITE);

    u8g2Fonts.setFont(fontMid);
    drawText("E-WWS ready!", 200, 50, CENTER);

    String wifiText;
    (wifiConnected) ? wifiText = "RTC Sync Succes!" : wifiText = "RTC sync Fail!";

    u8g2Fonts.setFont(fontSml);
    drawText(wifiText, 200, 80, CENTER);
    drawText(startTime, 200, 130, CENTER);

    //  WWSUIAlt();

    display.display();
    display.powerOff();
  }
}
 
void loop() {
  if(recievedData()){ WWSUIAlt(); }
}

void OTA(void * pvParameters){
  connectWifi();

  //  PASS IS "admin"!
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

  //  handle OTA on core #0
  for(;;){
    if(WiFi.status() == WL_CONNECTED){ wifiConnected = true; }
    else{ wifiConnected = false; }

    connectWifi();  //  keep wifi connection alive
    ArduinoOTA.handle();
    delay(1);
  }
}

void connectWifi(){
  static int tries = 0;

  if(WiFi.status() == WL_CONNECTED){
    tries = 0;
    return; 
  }


  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    if(tries > 500){ return; }
    Serial.print("(" + String(tries) + "/500)");
    delay(500);
    tries++; 
  }
  Serial.println("Connected!");
}

void syncRTC(String ssid, String pass){
  Serial.println("Connecting to wifi..");

  WiFi.begin(ssid, pass);
  
  byte tries = 0;
  while (WiFi.status() != WL_CONNECTED) {

    if(tries > 100){
      Serial.println("Failed to connect to wifi!");
      return; 
    }
    tries++;

    delay(500);
    Serial.print("-");
  }
  Serial.println("Connected!");

  ntp.ruleDST("CEST", Last, Sun, Mar, 2, 120); // last sunday in march 2:00, timetone +120min (+1 GMT + 1h summertime offset)
  ntp.ruleSTD("CET", Last, Sun, Oct, 3, 60); // last sunday in october 3:00, timezone +60min (+1 GMT)

  ntp.begin();

  ntp.update();

  if(ntp.year() > 2020){
    rtc.adjust(DateTime(ntp.year(), ntp.month(), ntp.day(), ntp.hours(), ntp.minutes(), ntp.seconds()));
    Serial.println("Got time, RTC synced!");

    Serial.println(ntp.formattedTime("%d %B %Y")); // dd. Mmm yyyy
    Serial.println(ntp.formattedTime("%A %T")); // Www hh:mm:ss
    wifiConnected = true;
  }
  else{
    Serial.print("Failed to get time, RTC not synced!");
    wifiConnected = false;
  }
  
  delay(1000);

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

bool recievedData(){
  bool hasData = false;
  float tempArray[dataCnt];

  while(Serial2.available()){
    rawData = Serial2.readStringUntil('\n');
    hasData = true;

    //save timestamp
    DateTime now = rtc.now();

    currentMonth = now.month(); //  for zambretti forecast
    byte currentHour = now.hour();
    byte currentMin = now.minute();

    if(currentHour == resetHr && currentMin <= 5 && !resetMinMax){
      resetMinMax = true;
      Serial.println("Resetting min/max!"); 
    }
    else if(currentHour == clearCycleHr && !ranClearCycle){
      ranClearCycle = true;

      Serial.println("Running clear cycle..");
      clearCycle(5);
      WWSUIAlt();
    }
    else if(currentHour == 10 && ranClearCycle){ ranClearCycle = false; }

    String tempData = rawData;
    Serial.print(rawData);

    //  if it's rain data, send to other screen and return 
    if(rawData.startsWith("r")){
      parseRainData(rawData);
      return true;
    }

    //  only save timestamp if it was probe data
    dataTime = String(currentHour) + ":";
    if(currentMin < 10) { dataTime += "0"; }
    dataTime += currentMin;

    for(int i = 0; i < dataCnt; i++){ // xx.xx|xx.xx|xxxx--

      String partOfData = tempData.substring(0, tempData.indexOf('|'));
      tempArray[i] = partOfData.toFloat();
      
      tempData = tempData.substring(tempData.indexOf('|')+1);
    }
    
  if(tempArray[1] < 10 || tempArray[2] < 800 ){ return false; }    // if recieved erronous data ( humidity == 0), don't show it

  if(!filled ) {   // fill whole graph with recieved data once
    for(int i = 0; i < graphSize; i++){
      gDataT[i] = tempArray[0];
      gDataH[i] = tempArray[1];
      gDataP[i] = tempArray[2];
      gDataTime[i] = dataTime;
    }
    filled = true;
  }
  if(resetMinMax){
      tMin = gDataT[graphSize-1];
      tMax = gDataT[graphSize-1];

      hMin = gDataH[graphSize-1];
      hMax = gDataH[graphSize-1];

      pMin = gDataP[graphSize-1];
      pMax = gDataP[graphSize-1];

      //  reset min/max times
      for(int i = 0; i < 2; i++){
        minMaxTimes[0][i] = dataTime;
        minMaxTimes[1][i] = dataTime;
        minMaxTimes[2][i] = dataTime;
      }
      resetMinMax = false;
  }
  else{
  
  //shift all data in graphs to the left
    for(int i = 0; i < graphSize-1; i++){
      gDataT[i] = gDataT[i+1];
      gDataH[i] = gDataH[i+1];
      gDataP[i] = gDataP[i+1];
      gDataTime[i] = gDataTime[i+1];
    }

    gDataT[graphSize-1] = tempArray[0];
    gDataH[graphSize-1] = tempArray[1];
    gDataP[graphSize-1] = tempArray[2];
    gDataTime[graphSize-1] = dataTime;
    }
  }
  return hasData;
}

void parseRainData(String rawData){
  //  extract data from raw string
  rawData.trim();
  
  rainLast = rawData.substring(1, rawData.indexOf("|")).toFloat();      //  rain value 
  rainTime = rawData.substring(rawData.indexOf("|")+1);          //  full time ( String format )
  int dataHour = rainTime.substring(0, rainTime.indexOf(":")).toInt();  //  hour from time

  Serial.println("rainLast: " + String(rainLast));
  Serial.println("dataHour: " + String(dataHour));
  Serial.println("rainTime: " + String(rainTime));

  //  shift data into array
  for(int i = 0; i < rainGraphSize-1; i++){
    gDataR[i] = gDataR[i+1];
  }
  gDataR[rainGraphSize-1] = rainLast;

  //  update total/last
  rainTotal += rainLast;
  (dataHour) ? rainAvg = (rainTotal / dataHour) : rainAvg = rainLast;

  //  reset at 0:00
  if(rainTime.equals("0:00") || rainTime.equals("00:00")){
    rainAvg = 0;
    rainTotal = rainLast;
  }
}

void drawWWSUI(){

  int h = 0;
  int l = 100000;

  float tLast = gDataT[graphSize-1];
  float hLast = gDataH[graphSize-1];
  float pLast = gDataP[graphSize-1];

  //  [0][0/1] = TEMP
  if(tLast < tMin){
    tMin = tLast;
    minMaxTimes[0][0] = dataTime;
  }
  if(tLast > tMax){
    tMax = tLast;
    minMaxTimes[0][1] = dataTime;
  }

  //  [1][0/1] = HUM
  if(hLast < hMin){
    hMin = hLast;
    minMaxTimes[1][0] = dataTime;
  }
  if(hLast > hMax){
    hMax = hLast;
    minMaxTimes[1][1] = dataTime;
  }

  //  [2][0/1] = PRESSURE
  if(pLast < pMin){
    pMin = pLast;
    minMaxTimes[2][0] = dataTime;
  }
  if(pLast > pMax){
    pMax = pLast;
    minMaxTimes[2][1] = dataTime;
  }
  
  for(int i = 0; i < 3; i++){
    Serial.println(minMaxTimes[i][0]);
    Serial.println(minMaxTimes[i][1]);
  }

  String fixedpMin = String(int(pMin));
  String fixedpMax = String(int(pMax));

  //only save last two chars 
  if(pMin > 999){ fixedpMin = fixedpMin.substring(2); }
  else { fixedpMin = fixedpMin.substring(1); }
  
  if(pMax > 999) { fixedpMax = fixedpMax.substring(2); }
  else { fixedpMax = fixedpMax.substring(1); }
  
  display.fillScreen(GxEPD_WHITE);
 
  //draw graph section
  drawGraph(260, 20, 120, 75, true, 0, 100, 2, gDataT, "Temperature", minMaxTimes[0][0], minMaxTimes[0][1]);   //xPos, yPos, width, height, enable autoscaling, yMin, yMax, minScale, data array, title, minlabel, maxLabel
  drawGraph(260, 110, 120, 75, false, 0, 100, 2, gDataH, "Humidity", minMaxTimes[1][0], minMaxTimes[1][1]);
  drawGraph(260, 200, 120, 75, true, 0, 100, 1, gDataP, "Air pressure", minMaxTimes[2][0], minMaxTimes[2][1]);
  
  //disabled since we don't get battery telemetry ( yet? )
  //drawBattery(5, 2, 2.4, true, false);
 
  //draw temp section
  drawMainDisplay(5, 20, 250, 165, 2, String(gDataT[graphSize-1]), String(tMin, 0), String(tMax, 0), "TEMP");
 
  //draw humidity/pressure section
  drawSecDisplay(5, 190, 122, 105, 2, String(gDataH[graphSize-1]), String(hMin, 0), String(hMax, 0), "RH"); 
  drawSecDisplay(132, 190, 122, 105, 2, String(int(gDataP[graphSize-1])), fixedpMin, fixedpMax, "hPa");

  // draw timestamp
  u8g2Fonts.setFont(fontSpecial);


  drawText(dataTime, 200, 15, CENTER);

  String wifiStatus;
  if(wifiConnected){
    wifiStatus = WiFi.localIP().toString();
  }
  else{ wifiStatus = "Wifi err!"; }

  u8g2Fonts.setFont(fontxSml);
  drawText(wifiStatus, 5, 15, LEFT);

  float oldPressure = gDataP[(graphSize-1)-36];
  if(oldPressure > 900){
    String forecast = get_zambretti(oldPressure, pLast, currentMonth, tLast);

    u8g2Fonts.setFont(fontxSml);
    drawText(forecast, 400, 10, RIGHT);
  }

  Serial.println(WiFi.localIP());

  //  calculate dew point ( formula from: https://github.com/finitespace/BME280/blob/master/src/EnvironmentCalculations.cpp )
  float temp = gDataT[graphSize-1];
  float hum = gDataH[graphSize-1];

  dewPoint = 243.04 * (log(hum/100.0) + ((17.625 * temp)/(243.04 + temp))) /(17.625 - log(hum/100.0) - ((17.625 * temp)/(243.04 + temp)));

  String dewString = "dew point: " + String(dewPoint, 1);

  u8g2Fonts.setFont(fontSpecial);

  drawText(dewString, 130, 175, CENTER);

  display.display();
  display.powerOff();
}

void WWSUIAlt(){
  float tLast = gDataT[graphSize-1];
  float hLast = gDataH[graphSize-1];
  float pLast = gDataP[graphSize-1];

  tMin = min(tLast, tMin);
  tMax = max(tLast, tMax);

  hMin = min(hLast, hMin);
  hMax = max(hLast, hMax);

  pMin = min(pLast, pMin);
  pMax = max(pLast, pMax);

  display.fillScreen(GxEPD_WHITE);

  display.drawFastHLine(20, 50, 360, colorBlack);

  String wifiStatus;
  if(wifiConnected){
    wifiStatus = WiFi.localIP().toString();
  }
  else{ wifiStatus = "Wifi err!"; }

  u8g2Fonts.setFont(fontxSml);
  drawText(wifiStatus, 5, 400, CENTER);

  u8g2Fonts.setFont(fontSpecial);
  drawText(dataTime, 400, 15, RIGHT);

  //  calculate dew point ( formula from: https://github.com/finitespace/BME280/blob/master/src/EnvironmentCalculations.cpp )

  dewPoint = 243.04 * (log(hLast/100.0) + ((17.625 * tLast)/(243.04 + tLast))) /(17.625 - log(hLast/100.0) - ((17.625 * tLast)/(243.04 + tLast)));

  String dewString = "dp: " + String(dewPoint, 1);

  drawText(dewString, 0, 15);

  float oldPressure = gDataP[(graphSize-1)-36];
  if(oldPressure > 900){
    String forecast = get_zambretti(oldPressure, pLast, currentMonth, tLast);
    drawText(forecast, 200, 40, CENTER);
  }

  display.drawFastVLine(130, 75, 90, colorBlack);
  display.drawFastVLine(275, 75, 90, colorBlack);

  u8g2Fonts.setFont(fontxSml);
  drawText("Temperature", 60, 70, CENTER);

  drawText("Humidity", 200, 70, CENTER);
  drawText("Air pressure", 345, 70, CENTER);

  drawSegment(60, 80, tLast, tMin, tMax);
  drawSegment(200, 80, hLast, hMin, hMax);
  drawSegment(345, 80, pLast, pMin, pMax);

  //  old values, so I can always revert to them
  // drawSimpleGraph(0, 195, 105, 80, true, 0, 30, 5, gDataT, "", "min", "max");
  // drawSimpleGraph(145, 195, 105, 80, true, 40, 100, 5, gDataH, "", "min", "max");
  // drawSimpleGraph(290, 195, 105, 80, true, 0, 30, 5, gDataP, "", "min", "max");

  drawSimpleGraph(0, 190, 105, 30, true, 0, 30, 5, gDataT, "", "min", "max");
  drawSimpleGraph(145, 190, 105, 30, true, 40, 100, 5, gDataH, "", "min", "max");
  drawSimpleGraph(290, 190, 105, 30, true, 0, 30, 5, gDataP, "", "min", "max");

  u8g2Fonts.setFont(fontxSml);

  drawText("Avg/hr: " + String(rainAvg), 0, 235, LEFT);
  drawText("Last: " + String(rainLast), 200, 235, CENTER);
  drawText("Total:" + String(rainTotal), 400, 235, RIGHT);

  drawBargraph(0, 230, 400, 70, gDataR);

  drawText(rainTime, 400, 25, RIGHT);

  display.display();
  display.powerOff();
}

String get_zambretti(float z_hpa, float z_hpa_old, int month, float temp){

  //  convert to sea level adjusted pressure
  z_hpa = (z_hpa / pow(1 - ((0.0065 *elevation) / (temp + (0.0065 *elevation) + 273.15)), 5.257));
  z_hpa_old = (z_hpa_old / pow(1 - ((0.0065 *elevation) / (temp + (0.0065 *elevation) + 273.15)), 5.257));

  float z_range = z_max - z_min;
  float z_constant = round(z_range / 22);

  byte z_trend = 100;

  if(z_hpa_old - z_hpa < -1.6) { z_trend = 0; }        //  RISING
  else if(z_hpa_old - z_hpa > 1.6) { z_trend = 1;}     //  FALLING
  else { z_trend = 2; }                                //  STEADY

  //  change value if it's winter
  if(month <= 3 && month >= 10){
    if(z_trend == 0){
      z_hpa = z_hpa + 7 / 100 * z_range;
    }
    else if(z_trend == 1){
      z_hpa = z_hpa - 7 / 100 * z_range;
    }
  }

  if(z_hpa == z_max) { z_hpa = z_max - 1;}
  byte z_out = floor((z_hpa - z_min) / z_constant);

  
  if(z_out < 0){
    z_out = 0;
  }
  if(z_out > 21){
    z_out = 21;
  }

  String output;

  if(z_trend == 0){       //  RISING
    output = zambretti[rise_options[z_out]];
  }
  else if(z_trend == 1){  //  FALLING
    output = zambretti[falling_options[z_out]];
  }
  else{                   //  STEADY
    output = zambretti[steady_options[z_out]];
  }

  return output;
}

//-----------------------------[UI  FUNCTIONS]-----------------------------
// UI STUFF //
//xPos    = xPosition of upper left corner of the box
//yPos    = yPosition of upper left corner of the box
//w       = width of box
//h       = height of box, going downward!
//border  = thickness of box border, in pixels
//mainNum = String that gets displayed at the top
//subNum  = Strings that get displayed under mainNum
//title   = text to display in the left upper corner
 
void drawSecDisplay(int xPos, int yPos, int w, int h, int border, String mainNum, String subNumOne, String subNumTwo, String title){
  
  drawBox(xPos, yPos, w, h, border);
 
  u8g2Fonts.setFont(fontMid);
  //u8g2Fonts.setCursor(xPos, yPos);
 
  u8g2Fonts.setCursor((w/2) - (u8g2Fonts.getUTF8Width(mainNum.c_str())/2) + xPos, yPos+50);
  u8g2Fonts.print(mainNum);
 
  u8g2Fonts.setFont(fontSpecial);
  u8g2Fonts.setCursor(xPos+5, yPos+15);
  u8g2Fonts.print(title); 
  
  u8g2Fonts.setFont(fontSml);
 
  String subNum = subNumOne + "|" + subNumTwo;
  u8g2Fonts.setCursor((w/2) - (u8g2Fonts.getUTF8Width(subNum.c_str())/2) + xPos, yPos+80);   //25, 270
  u8g2Fonts.print(subNum);
}
 
void drawMainDisplay(int xPos, int yPos, int w, int h, int border, String mainNum, String subNumOne, String subNumTwo, String title){
 
  drawBox(xPos, yPos, w, h, border);
 
  u8g2Fonts.setFont(fontBig);
  u8g2Fonts.setCursor((w/2) - (u8g2Fonts.getUTF8Width(mainNum.c_str())/2) + xPos, yPos+70);
  u8g2Fonts.print(mainNum);
 
  u8g2Fonts.setFont(fontMid);
  String subNum = subNumOne + "|" + subNumTwo;
  u8g2Fonts.setCursor((w/2) - (u8g2Fonts.getUTF8Width(subNum.c_str())/2) + xPos, yPos+105);
  u8g2Fonts.print(subNum);
 
  u8g2Fonts.setFont(fontSpecial); 
  u8g2Fonts.setCursor(xPos+5, yPos+15);
  u8g2Fonts.print("TEMP");
 
}
 
void drawSegment(int xPos, int yPos, float current, float min, float max){

  u8g2Fonts.setFont(fontBig);
  (current > 99) ? drawText(String(current, 0), xPos, yPos + 40, CENTER) : drawText(String(current, 1), xPos, yPos + 40, CENTER);
  
  u8g2Fonts.setFont(fontMid);

  String subtext;

  if(min > 99){
    String tempString = String(min, 0);
    subtext += tempString.substring(tempString.length() - 2);
  }
  else{
    subtext += String(min, 0);
  }

  subtext += "|";
  
  if(max > 99){
    String tempString = String(max, 0);
    subtext += tempString.substring(tempString.length() - 2);
  }
  else{
    subtext += String(max, 0);
  }

  drawText(subtext, xPos, yPos + 75, CENTER);

}

void drawBargraph(int xPos, int yPos, int w, int h, float data[rainGraphSize]){
  float max = -100000;
  float min = 100000;
  
  for(int i = 0; i < rainGraphSize; i++){
    if(data[i] > max) { max = data[i]; }
    if(data[i] < min) { min = data[i]; }
  }

  max += maxPadding;
  drawBargraph(xPos, yPos, w, h, data, min, max);
}

//  draw graph
void drawBargraph(int xPos, int yPos, int w, int h, float data[rainGraphSize], float minRange, float maxRange){
  int screenWidthLocal = xPos + w;
  int screenHeightLocal = yPos + h;

  if(maxRange - minRange < minScale){
    maxRange += (minScale / 2);
    // minRange -= (minScale / 2);
  }
  if(minRange < 0) { minRange = 0; }

  byte spacing = 1;
  int startOffset = 1;
  byte barWidth = (screenWidthLocal / rainGraphSize) - spacing;

  //  drawSquare(xPos, yPos, w, h, 2);

  //  draw horizontal scale lines
  // for(int i = 0; i < numLines; i++){
  //   int lineY = map(i, 0, numBarLines, yPos, screenHeightLocal);
  //   String label = String(mapf(i, 0, numBarLines, maxRange, minRange), 1);
    
  //   u8g2Fonts.setFont(fontxSml);
  //   drawText(label, xPos - 2, lineY + 4, RIGHT);

  //   display.drawFastHLine(xPos, lineY, w, colorBlack);
  // }

  //  draw graph bars
  for(int i = 0; i < rainGraphSize; i++){

    //  calculate X and Y positions for bars
    // int xPoint = xPos + (((screenWidthLocal - xPos)/maxGraphSize)*i) + (spacing/2) + (startOffset / 2);
    int xPoint = map(i, 0, rainGraphSize, xPos, xPos+w) + startOffset;
    float yPoint = mapf(data[i], minRange, maxRange, yPos, screenHeightLocal);

    int barHeight = yPos + (screenHeightLocal - yPoint);
    display.fillRect(xPoint, barHeight, barWidth, yPoint - yPos, colorBlack);

    u8g2Fonts.setFont(fontxSml);
    //  draw data under bar

    byte yOffset = 0;
    if(rainGraphSize > 12){ (i % 2) ? yOffset = 0 : yOffset = 10; }


    //  drawText(String(data[i]), xPoint + (barWidth / 2), screenHeightLocal + 15 + yOffset, CENTER);

    //  draw timestamp above bar
    //  drawText(graphTimes[i], xPoint + (barWidth / 2), yPos - 5 - yOffset, CENTER);
  }
}

//  graph without any of the UI elements, just the graph line
void drawSimpleGraph(int xPos, int yPos, int width, int height, bool autoScale, float yMinStatic, float yMaxStatic, float minScale, float data[graphSize], String title, String minTitle, String maxTitle){
 u8g2Fonts.setFont(fontxSml);
 
  u8g2Fonts.setCursor(xPos, yPos);
  u8g2Fonts.print(title);
 
  float yCoords[graphSize];
  float yMin = 10000;
  float yMax = -10000;
 
  //find min/max if autoscaling is enabled
  if(autoScale){
    for(int i = 0; i < graphSize; i++){
      if(data[i] > yMax) { yMax = data[i]; }
      if(data[i] < yMin) { yMin = data[i]; }
    }
    yMax += scaleMargin;
    yMin -= scaleMargin;
  }
  else{
    yMin = yMinStatic;
    yMax = yMaxStatic;
  }

  if(yMax - yMin < minScale){
    yMax += minScale/2;
    yMin -= minScale/2;
  }
 
  //  transform datapoints to screen coordinates
  for (int i = 0; i < graphSize; i++) { yCoords[i] = mapf(data[i], yMin, yMax, yPos+height, yPos); }
  
  //  mark hi/lo point on graph
  //  int mappedMinIndex, mappedMaxIndex;
  float lastBig =  100000.0;
  float lastSml = -100000.0;

  //  draw data into array
  for(int gx = 0; gx < graphSize-1; gx++){
    int xOne = 0;
    int xTwo = 0;
 
    xOne = map(gx, 0, graphSize, xPos, xPos+width);
    xTwo = map(gx+1, 0, graphSize, xPos, xPos+width);

    (drawBlackGraph) ? display.drawLine(xOne, yCoords[gx], xTwo, yCoords[gx+1], GxEPD_BLACK) : display.drawLine(xOne, yCoords[gx], xTwo, yCoords[gx+1], GxEPD_RED);
  }
  u8g2Fonts.setCursor(xPos + 3, (yPos+height)-5);
  String graphLen = String((graphSize * dataFreq) / 60) + "h";
  u8g2Fonts.print(graphLen);
}

void drawGraph(int xPos, int yPos, int width, int height, bool autoScale, float yMinStatic, float yMaxStatic, float minScale, float data[graphSize], String title, String minTitle, String maxTitle) {
  u8g2Fonts.setFont(fontxSml);
 
  u8g2Fonts.setCursor(xPos, yPos);
  u8g2Fonts.print(title);
 
  float yCoords[graphSize];
  float yMin = 10000;
  float yMax = -10000;
 
  //find min/max if autoscaling is enabled
  if(autoScale){
    for(int i = 0; i < graphSize; i++){
      if(data[i] > yMax) { yMax = data[i]; }
      if(data[i] < yMin) { yMin = data[i]; }
    }
    yMax += scaleMargin;
    yMin -= scaleMargin;
  }
  else{
    yMin = yMinStatic;
    yMax = yMaxStatic;
  }

  if(yMax - yMin < minScale){
    yMax += minScale/2;
    yMin -= minScale/2;
  }

  //draw graph outline
  //display.drawRect(xPos, yPos, width, height, GxEPD_BLACK);
  drawBox(xPos, yPos, width, height, 2);
 
  //  transform datapoints to screen coordinates
  for (int i = 0; i < graphSize; i++) { yCoords[i] = mapf(data[i], yMin, yMax, yPos+height, yPos); }
  
  //  mark hi/lo point on graph
  int mappedMinIndex, mappedMaxIndex;
  float lastBig =  100000.0;
  float lastSml = -100000.0;

  //  search for highest and lowest point, starting at newest data
  for(int i = graphSize-1; i > -1; i--){
    //  graph is "inverted" ( "high" value on graph is low Y coord ), so this needs to be inverted too
    if(yCoords[i] > lastSml){
      mappedMinIndex = i;
      lastSml = yCoords[i];
    }

    if(yCoords[i] < lastBig){ 
      mappedMaxIndex = i;
      lastBig = yCoords[i];
    }
  }

  // int xPosLow = map(mappedMinIndex, 0, graphSize, xPos, xPos+width);
  // display.fillCircle(xPosLow, yCoords[mappedMinIndex], 3, GxEPD_BLACK);

  // u8g2Fonts.setFont(fontxSml);
  // (mappedMinIndex < (graphSize / 2)) ? drawText(minTitle, xPosLow, yCoords[mappedMinIndex]+15, LEFT) : drawText(minTitle, xPosLow, yCoords[mappedMinIndex]+15, RIGHT);
  // u8g2Fonts.setCursor(xPosLow, yCoords[mappedMinIndex]+15) 
  
  // u8g2Fonts.print(minTitle);

  // int xPosHi = map(mappedMaxIndex, 0, graphSize, xPos, xPos+width);
  // display.fillCircle(xPosHi, yCoords[mappedMaxIndex], 3, GxEPD_BLACK);

  // u8g2Fonts.setCursor(xPosHi, yCoords[mappedMaxIndex]-3);
  // (mappedMaxIndex < (graphSize / 2)) ? drawText(maxTitle, xPosHi, yCoords[mappedMaxIndex]-3, LEFT) : drawText(maxTitle, xPosHi, yCoords[mappedMaxIndex]-3, RIGHT);

  //  draw data into array
  for(int gx = 0; gx < graphSize-1; gx++){
    int xOne = 0;
    int xTwo = 0;
 
    xOne = map(gx, 0, graphSize, xPos, xPos+width);
    xTwo = map(gx+1, 0, graphSize, xPos, xPos+width);

    (drawBlackGraph) ? display.drawLine(xOne, yCoords[gx], xTwo, yCoords[gx+1], GxEPD_BLACK) : display.drawLine(xOne, yCoords[gx], xTwo, yCoords[gx+1], GxEPD_RED);
  }
 
  //draw horizontal lines
  for(int i = 0; i < numYMarkers+1; i++){
    if(i > 0 && i < numYMarkers) {
      int markYPosLine = (height / numYMarkers * i) + yPos;
      for(int j = 0; j < numLines; j++){
        int lineX = width / numLines * j;
        display.drawFastHLine(lineX+(5/2)+xPos, markYPosLine, 5, GxEPD_BLACK);
      }
    } 

  //draw Y numbers
  int markYPosText = (height / numYMarkers * i) + yPos; 
  u8g2Fonts.setCursor(xPos+width+1, markYPosText+4);

  float displayMin = yMin;
  float displayMax = yMax;

  //only show last 2 digits
  if(displayMin > 1000){ displayMin -= 1000; }
  else if(displayMin > 900) { displayMin -= 900; }

  if(displayMax > 1000){ displayMax -= 1000; } 
  else if(displayMax > 900) { displayMax -= 900; }

  if(yMax < yMin+10) { u8g2Fonts.print(String(displayMax - (float)(displayMax - displayMin) / numYMarkers * i + 0.01, 1)); }  //show decimals if range is smaller than 10
  else { u8g2Fonts.print(String(displayMax - (float)(displayMax - displayMin) / numYMarkers * i, 0)); }

  u8g2Fonts.setCursor(xPos + 3, (yPos+height)-5);
  String graphLen = String((graphSize * dataFreq) / 60) + "h";
  u8g2Fonts.print(graphLen);
  }
}
 
void drawBattery(int xPos, int yPos, float level, bool showLvl, bool useFont){
  
  if(useFont){
      u8g2Fonts.setFont(batteryFont);
      u8g2Fonts.setCursor(xPos, yPos-5);
      if(level > 3.5){ u8g2Fonts.print(7); }
      else if(level > 2.8 && level < 3.5) { u8g2Fonts.print(6); }
      else if(level > 2.1 && level < 2.8) { u8g2Fonts.print(5); }
      else if(level > 1.4 && level < 2.1) { u8g2Fonts.print(4); }
      else if(level < 0.7) { u8g2Fonts.print(3); }
      else { u8g2Fonts.print(0); }
  }
  else{
    int fill = mapf(level, 0, 4.2, 0, 34);
    display.drawRect(xPos, yPos, 35, 15, colorBlack);
    display.drawRect(xPos+34, yPos+3, 6, 10, colorBlack);
    display.fillRect(xPos+1, yPos, fill, 14, colorBlack);
  }
 
  if(showLvl) {
    display.setCursor(xPos+50, yPos+5);
    display.print(String(level) + "v"); 
  }
}

//-----------------------------[HELPER FUNCTIONS]-----------------------------

//  draw text, aligned by txt
void drawText(String text, int x, int y, textAligns txt){
  int textW = u8g2Fonts.getUTF8Width(text.c_str());

  switch (txt){

    case LEFT:
      drawText(text, x, y);
      break;
    
    case CENTER:
      drawText(text, x - (textW/2), y);
      break;
    
    case RIGHT:
      drawText(text, x - textW, y);
      break;
  }
}

// draws the actual text
void drawText(String text, int x, int y) {
  u8g2Fonts.setCursor(x, y);

  u8g2Fonts.print(text);
}

//  clears the screen a couple of times, to help with burn-in hopefully
void clearCycle(byte cycles){
  for(int i = 0; i <= cycles; i++){
    
    //  alternate between filling black and filling white
    (i % 2) ? display.fillScreen(GxEPD_WHITE) : display.fillScreen(GxEPD_BLACK);

    String clearString = "Clearing screen.. (" + String(i) + "/" + String(cycles) + ")";

    u8g2Fonts.setFont(fontSpecial);
    
    u8g2Fonts.setCursor(random(400 - u8g2Fonts.getUTF8Width(clearString.c_str())), random(20, 300));
    u8g2Fonts.print(clearString);

    display.display();
    delay(2000);
  }
}

//erases whatever is inside the box area!
void drawBox(int xPos, int yPos, int w, int h, int borderSize){
  display.fillRect(xPos, yPos, w, h, colorBlack);
  display.fillRect(xPos+borderSize, yPos+borderSize, w-(borderSize*2), h-(borderSize*2), colorWhite);
}

void debugScreen(String data){

  display.fillScreen(GxEPD_WHITE);
  u8g2Fonts.setFont(fontMid);

  u8g2Fonts.setCursor(0, 50);
  u8g2Fonts.print(data);

  String timeSince = String((millis() - lastRecieved)/1000) + "s.";
  lastRecieved = millis();

  u8g2Fonts.setCursor(0, 100);
  u8g2Fonts.print(timeSince);

  display.display();
  display.powerOff();
}

float mapf(float x, float in_min, float in_max, float out_min, float out_max){
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}