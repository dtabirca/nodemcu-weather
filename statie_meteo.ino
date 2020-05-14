/*
 * 
   Wireless Outdoor Weather Station
   PM10 PM2.5 Air Quality Monitor

   Hardware:
   NodeMCU v3 (esp8266 12e)
   Shinyei Optical Dust sensor PPD42NS (modified with 3.3v voltage divider on pin2 and pin4, 100uF capacitor as C8)
   BMP180 Barometric BMP/Temperature/Altitude Sensor
   DHT21/AM2301 Temperature and humidity module

   Wire Connections:
   NodeMCU     Sensors 
   3.3V        VCC(BMP180, DHT21)
   GND         GND(BMP180, DHT21)
   D1          SCL(BMP180)          
   D2          SDA(BMP180)
   D3          DHT DATA
   D5          PPD42 P2(pin 2)
   D6          PPD42 P1(pin 4)    

   More about the PPD42 sensor:
   https://files.seeedstudio.com/wiki/Grove_Dust_Sensor/resource/Grove_-_Dust_sensor.pdf
   https://www.shinyei.co.jp/stc/eng/products/optical/ppd42nj.html
   https://www.researchgate.net/publication/324171671_Understanding_the_Shinyei_PPD42NS_low-cost_dust_sensor

   PM10, PM25:
   For now, it calculates the average based on the last three readings (data from the last aprox. 20min),
   since it is powered during short intervals and not all readings are valid.

   Conclusions:
   It detects successfully household dust, cigarette smoke, pollen, gypsum plaster powder, etc.
   If compared with data from other sources, there is a larger difference between PM10 and PM25, probably because:
   - most of the dust particles at the place of measurement are in the 10um channel
   - the minimum detected particle for PPD42 is 1um
   - for the same number of particles, the mass in PM10 channel is about 200 times higher than in 2.5 channel
   Daily and monthly average calculations may be more precise.
*/

#include <ESP8266WiFi.h>
#include "DHT.h"
#include <SFE_BMP180.h>
#include <Wire.h>

// BMP
#define ALTITUDE 650.0 // m
SFE_BMP180 bmp;
// DHT
#define DHTPIN 0 // pin D3
#define DHTTYPE DHT21
DHT dht(DHTPIN, DHTTYPE);
// PPD42
#define PM25 14 // pin D5
#define PM10 12 // pin D6
// Wifi
const char* ssid     = "*******";
const char* password = "*******";
WiFiClient client;
// ThingsSpeak
String apiKey = "*******";
String apiUrl = "api.thingspeak.com";
// readings and calculations
// PM
unsigned long sampletime_ms = 30000;//sample 30s ;
float averagePM10 = 0.0;
float avdataPM10[3] = {0.0, 0.0, 0.0};
int avindexPM10 = 0;
float averagePM25 = 0.0;
float avdataPM25[3] = {0.0, 0.0, 0.0};
int avindexPM25 = 0;

float countP1data[3] = {0.0, 0.0, 0.0};
float countP2data[3] = {0.0, 0.0, 0.0};
int countIndex = 0;

// DHT
double t = 0.0;
float h = 0.0;
// BMP
double p = 0.0; 
    

/*
 * counts particles
 * 
 */
float getPM(int PIN)
{
  unsigned long duration;
  unsigned long starttime = millis();
  unsigned long lowpulseoccupancy = 0;
  float ratio = 0;
  float count = 0;
  
  while ((millis() - starttime) <= sampletime_ms) {
    duration = pulseIn(PIN, LOW);
    lowpulseoccupancy = lowpulseoccupancy + duration;
    yield();
  }
  ratio = lowpulseoccupancy / (sampletime_ms * 10.0);
  count = 1.1 * pow(ratio, 3) - 3.8 * pow(ratio, 2) + 520 * ratio + 0.62;
  return count;
}


/*
 * converts pcs/0.01cf to ug/m3
 * assumptions:
 * all particles are spherical, with a density of 1.65E12 μg/m3*
 * the radius of a particle in the PM2.5 channel is 0.44 μm
 * yet, minimum detected particle for PPD42 is 1um
 * the radius of a particle in the PM10 channel is 2.60 μm
 * 0.01 ft3 can be converted to m3 by multiplying by 3531.5
 * useful: http://aqicn.org/data/dylos/Air-Quality-Sensor-Network-for-Philadelphia.pdf
 * humidity correction factors are not used
 * 
 */
float convertPCStoUGperM3(int CHANNEL, float count)
{
  double pi = 3.14159;
  double density = 1.65 * pow(10, 12);
  double K = 3531.5;
  double r10 = 2.6 * pow(10, -6);
  double vol10 = (4 / 3) * pi * pow(r10, 3);
  double mass10 = density * vol10; // ug
  double r25 = 1 * pow (10, -6);
  double vol25 = (4 / 3) * pi * pow (r25, 3);
  double mass25 = density * vol25; // ug  

  switch (CHANNEL) {
    case 10:
      return (count) * K * mass10;  
    break;
    case 25:
      return (count) * K * mass25;  
    break;
    default:
    return 0.00;
  }
}


/*
 * calculates count average, igore null readings
 */
float averageCount(float countData[])
{    
  int validNumber = 0;
  float validSum  = 0.0;
  float average   = 0.0;
  for (int i=0;i < 3; i++){
    if (countData[i] > 1){ // bad readings always return 0.62
      validSum += countData[i];
      validNumber += 1;
    }
  }
  if (validNumber > 0){
    average = validSum/validNumber;
  }
  return average;
}


void setup()
{
  Serial.begin(115200);
  dht.begin();
  bmp.begin();
  pinMode(PM25, INPUT);
  pinMode(PM10, INPUT);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
}


void loop()
{
  // DHT
  float newT = dht.readTemperature();
  if (!isnan(newT)) {
    t = newT;
  }
  float newH = dht.readHumidity();
  if (!isnan(newH)) {
    h = newH;
  }
  float dew = t - (100 - h) / 5;

  // BMP
  char status;
  double T,P;
  //get T from DHT sensor which works from -40C to 80C
  //status = bmp.startTemperature();
  //status = bmp.getTemperature(T);
  status = bmp.startPressure(3); // high resolution
  if (status != 0) {
    delay(status);// wait for the measurement to complete
    status = bmp.getPressure(P,t);
    if (status != 0) {
      p = bmp.sealevel(P,ALTITUDE);
    }
  }
 
  // PPD42
  float countP1 = getPM(PM10);// particles > 1um < 10um
  float countP2 = getPM(PM25);// particles > 2.5um < 10um

  // always use last three readings
  if (countP1 > 1) {
    countP1data[countIndex] = countP1;
  }
  if (countP2 > 1) {
    countP2data[countIndex] = countP2;
  }
  if (countIndex < 2) {
    countIndex += 1;
  }
  else {
    countIndex = 0;
  }

  // average from last three counts
  float countP1average = averageCount(countP1data);
  float countP2average = averageCount(countP2data);

  Serial.print("countP1average:");
  Serial.print(countP1average);
  Serial.print(", countP2average:");
  Serial.println(countP2average);

  // P1 should include P2
  // 1um|------------------P1--------------------|10um
  // 1um|-------2.5um|-------------P2------------|10um
  float countPM10 = 0.0;
  float countPM25 = 0.0;
  if (countP2average > 1) {
    countPM10 = countP2average;
    // countP1average < 1 shouldn't be possible in theory, yet we have this case
    // also, because we read P1 and P2 independently, 
    // => we assume all detected particles are in the 2.5um
    if (countP1average > countP2average) {
      countPM25 = countP1average - countP2average;
    }
    // else - null reading for P1 or P2 > P1, cannot calculate countPM25
  }
  else {
    // null reading for P2, 
    if (countP1average > 1) { 
      countPM25 = countP1average;
    }
  }

  Serial.print("PM10 pcs/0.01cf:");
  Serial.print(countPM10);
  Serial.print(", PM25 pcs/0.01cf:");
  Serial.println(countPM25);

  float concentrationPM10 = 0.0;
  if (countPM10 > 0){
    concentrationPM10 = convertPCStoUGperM3(10, countPM10);
    Serial.print("PM10 ug/m3:");
    Serial.print(concentrationPM10);    
  }
  float concentrationPM25 = 0.0;
  if (countPM25 > 0){
    concentrationPM25 = convertPCStoUGperM3(25, countPM25);
    Serial.print(", PM25 ug/m3:");
    Serial.println(concentrationPM25);
  }

  // send data
  if (client.connect(apiUrl,80)) {  
                          
    String postStr = apiKey;
    postStr +="&field1=";
    postStr += String(t);
    postStr +="&field2=";
    postStr += String(h);
    postStr +="&field3=";
    postStr += String(p);
    if (concentrationPM10 > 0) {
      postStr +="&field4=";
      postStr += String(concentrationPM10);
    }
    if (concentrationPM25 > 0) {
      postStr +="&field5=";
      postStr += String(concentrationPM25);
    }
    postStr +="&field6=";
    postStr += String(countP1);
    postStr +="&field7=";
    postStr += String(countP2);
    postStr +="&field8=";
    postStr += String(dew);    
    postStr += "\r\n\r\n";
    
    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: "+apiKey+"\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(postStr.length());
    client.print("\n\n");
    client.print(postStr);
    Serial.println(postStr);                   
                         }
    client.stop();
    ESP.deepSleep(60e6 * 5);// 5 min           
}
