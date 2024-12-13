#include <LiquidCrystal.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
//#include "Symbols.h"

#define SSID  "WPI Sailbot"
#define KEY   "xxxxxxxxxxxx"

// N2YO API Reference: https://www.n2yo.com/api/
// https://api.n2yo.com/rest/v1/satellite/radiopasses/25544/42.35111/-71.16504/0/1/40/&apiKey=xxxxxxxxxxxxxxxxx
const String ISS_ID = "25544";
const String LAT = "42.35111";
const String LONG = "-71.16504";
const String ALT = "0";
const String DAYS = "1";
const String MIN_ELE = "10"; //degrees
//const String MIN_VIS = "60"; //seconds visible
const String API_KEY = "xxxxxxxxxxxxxxxxxxxx"; //NOTE: 1000 req/hour

//String URLV = "/rest/v1/satellite/visualpasses/" + ISS_ID + "/" + LAT + "/" + LONG + "/" + ALT + "/" + DAYS + "/" + MIN_VIS + "/&apiKey=" + API_KEY;
String URLR = "/rest/v1/satellite/radiopasses/" + ISS_ID + "/" + LAT + "/" + LONG + "/" + ALT + "/" + DAYS + "/" + MIN_ELE + "/&apiKey=" + API_KEY;
#define HOST  "api.n2yo.com"
#define PORT  443

// ARISS Station Status: https://www.ariss.org/current-status-of-iss-stations.html


WiFiClientSecure client;
HTTPClient https;
DynamicJsonBuffer jsonBuffer(1024);
LiquidCrystal lcd(14, 12, 2, 0, 4, 5);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "north-america.pool.ntp.org", 0, 6000);

#define UTC-4 -4*60*60

long lastUpdate = -60000;
long nextPassStart = 0;

struct Flyby {
  double startAz;         //Degrees       ex. 331.17
  String startAzCompass;  //Compass rose  ex. "NW"
  unsigned long startUTC; //Unix time     ex. 1521451295
  double maxAz;           //Degrees       ex. 37.98
  String maxAzCompass;    //Compass rose  ex. "NE"
  double maxEl;           //Degrees       ex. 52.19
  unsigned long maxUTC;   //Unix time     ex. 1521451615
  double endAz;           //Degrees       ex. 118.6
  String endAzCompass;    //Compass rose  ex. "ESE"
  unsigned long endUTC;   //Unix time     ex. 1521451925
};

void setup() {
  Serial.begin(9600);
  timeClient.begin();


  //###############
  //## LCD Setup ##
//  //###############
//  lcd.createChar(0, wifiDisconected);
//  lcd.createChar(1, wifiConnected);
//  lcd.createChar(2, APIDisconnected);
//  lcd.createChar(3, APIConnected);
//  lcd.createChar(4, badHTTPReq);
//  lcd.createChar(5, goodHTTPReq);
  lcd.begin(16, 2);
//  
//  //################
//  //## WIFI Setup ##
//  //################
  WiFi.begin(SSID, KEY);
  lcd.print("Connecting WiFI");
  String str1 = "................";
  for (int i = 0; (WiFi.status() != WL_CONNECTED); i++) {
    lcd.setCursor(0, 1);
    lcd.print(str1.substring(0, i % 17) + "                ");
    delay(500);
  }
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

//  //###############
//  //## API Setup ##
//  //###############
  Serial.println("Connecting1");
  lcd.clear();
  lcd.print("Connecting API:");
  lcd.setCursor(0, 1);
  lcd.print("N2YO");
  client.setInsecure();
  https.begin(client, HOST, PORT, URLR);
//  Serial.println(URLV);

}

void loop() {
  if (!client.connected()) https.begin(client, HOST, PORT, URLR);

  if(millis()-lastUpdate > 60000){
    timeClient.update();
    int httpsCode = https.GET();
    lastUpdate = millis();
    String testString = https.getString();
    jsonBuffer.clear();
    JsonObject& root = jsonBuffer.parseObject(testString);
  
    JsonObject& info = root["info"];
    JsonArray& passes = root["passes"];
    JsonObject& nextPass = passes[0];
  
    Serial.println( (double) passes[0]["maxEl"]);
    nextPassStart = nextPass["startUTC"];
    Serial.println(testString);
    Serial.println(nextPassStart);
  }
 
  Serial.println(nextPassStart);
  Serial.println(timeClient.getEpochTime());
  Serial.println(nextPassStart-timeClient.getEpochTime());


  lcd.clear();
//  lcd.autoscroll();
  lcd.setCursor(0,0);
//  lcd.print("ISS T: " + timeClient.getFormattedTime());
  lcd.print("Next ISS Pass in");
  lcd.setCursor(0,1);

  // in __h __m __s
  lcd.print(getFormattedTime(nextPassStart-timeClient.getEpochTime()));

//  printLCDHeader(httpsCode);
//  printLCDRate(currentRate);
  delay(500);
}

//AWAKE : 7:30-19:30 UTC
//GREAT : 7:30-8:30, 18:30-19:30 UTC
//ASLEEP: 19:31-7:29 UTC
/*
 * {
 *  "info":
 *      {"satid":25544,"satname":"SPACE STATION","transactionscount":16,"passescount":1},
 *  "passes":
 *      [ {"startAz":301.43,"startAzCompass":"NW","startUTC":1696459300,"maxAz":221.57,"maxAzCompass":"SW","maxEl":59.14,"maxUTC":1696459620,"endAz":134.74,"endAzCompass":"SE","endUTC":1696459935}]}
 */

//{"info":{"satid":25544,"satname":"SPACE STATION","transactionscount":54,"passescount":5},
//"passes":[
//{"startAz":211.83,"startAzCompass":"SW","startUTC":1696519485,"maxAz":137.07,"maxAzCompass":"SE","maxEl":31.73,"maxUTC":1696519795,"endAz":63.17,"endAzCompass":"ENE","endUTC":1696520100},
//{"startAz":256.07,"startAzCompass":"W","startUTC":1696525280,"maxAz":336.41,"maxAzCompass":"NNW","maxEl":36.62,"maxUTC":1696525600,"endAz":52.92,"endAzCompass":"NE","endUTC":1696525910},
//{"startAz":289.92,"startAzCompass":"WNW","startUTC":1696531155,"maxAz":354.62,"maxAzCompass":"N","maxEl":16.97,"maxUTC":1696531445,"endAz":58.56,"endAzCompass":"NE","endUTC":1696531730},
//{"startAz":305.76,"startAzCompass":"NW","startUTC":1696537000,"maxAz":13.62,"maxAzCompass":"N","maxEl":21.22,"maxUTC":1696537300,"endAz":83.66,"endAzCompass":"E","endUTC":1696537600},
//{"startAz":304.24,"startAzCompass":"NW","startUTC":1696542810,"maxAz":20.88,"maxAzCompass":"NNE","maxEl":81,"maxUTC":1696543130,"endAz":123.04,"endAzCompass":"SE","endUTC":1696543450}
//]}

 /*
  * {
  *   "info":
  *      {"satid":25544,"satname":"SPACE STATION","transactionscount":14,"passescount":6},
*     "passes": 
*           [
*               {"startAz":306.9,"startAzCompass":"NW","startUTC":1696453495,"maxAz":20.18,"maxAzCompass":"NNE","maxEl":26.36,"maxUTC":1696453805,"endAz":92.78,"endAzCompass":"E","endUTC":1696454110},
*               {"startAz":301.43,"startAzCompass":"NW","startUTC":1696459300,"maxAz":221.57,"maxAzCompass":"SW","maxEl":59.14,"maxUTC":1696459620,"endAz":134.74,"endAzCompass":"SE","endUTC":1696459935},
*               {"startAz":211.8,"startAzCompass":"SW","startUTC":1696519485,"maxAz":136.66,"maxAzCompass":"SE","maxEl":31.73,"maxUTC":1696519795,"endAz":63.14,"endAzCompass":"ENE","endUTC":1696520100},
*               {"startAz":256.09,"startAzCompass":"W","startUTC":1696525280,"maxAz":332.96,"maxAzCompass":"NNW","maxEl":36.63,"maxUTC":1696525595,"endAz":52.95,"endAzCompass":"NE","endUTC":1696525910},
*               {"startAz":289.54,"startAzCompass":"WNW","startUTC":1696531150,"maxAz":354.87,"maxAzCompass":"N","maxEl":16.98,"maxUTC":1696531445,"endAz":58.62,"endAzCompass":"NE","endUTC":1696531730},
*               {"startAz":305.81,"startAzCompass":"NW","startUTC":1696537000,"maxAz":13.93,"maxAzCompass":"N","maxEl":21.22,"maxUTC":1696537300,"endAz":72.1,"endAzCompass":"ENE","endUTC":1696537490}]}

  */

String getFormattedTime(unsigned long rawTime) {
  unsigned long hours = rawTime / 3600;
  String hoursStr = hours < 10 ? "0" + String(hours) : String(hours);

  unsigned long minutes = (rawTime % 3600) / 60;
  String minuteStr = minutes < 10 ? "0" + String(minutes) : String(minutes);

  unsigned long seconds = rawTime % 60;
  String secondStr = seconds < 10 ? "0" + String(seconds) : String(seconds);

  return hoursStr + "h " + minuteStr + "m " + secondStr + "s";
}

void printLCDHeader(int httpsCode){
  lcd.clear();
  lcd.print("BTC-USD:");
  lcd.setCursor(13, 0);
  lcd.write(WiFi.status() == WL_CONNECTED); 
  lcd.write(client.connected() + 2); //TODO: Make Symbols
  lcd.write((httpsCode == HTTP_CODE_OK) + 4);
}

double lastRate = 0;
void printLCDRate(double rate){
  if (!rate) rate = lastRate;
  lcd.setCursor(0,1);
  String rate_str = String(rate, 4);
  lcd.print("$" + rate_str.substring(0, rate_str.length()-8) + "," + rate_str.substring(rate_str.length()-8));
  lastRate = rate;
}
