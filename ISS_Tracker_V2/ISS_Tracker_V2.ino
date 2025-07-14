#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include "Symbols.h"
#include <WifiLocation.h>

// ARISS Station Status: https://www.ariss.org/current-status-of-iss-stations.html

//TODOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO
/*
 * Updated
 *  Timezone
 *  Buttons handling
 *  60 second timeout
 *  
 * 
 * Return to Page 1 after 30ish seconds inactivity
 * Refactor to TIMEZONE
 * Flashy wifi selection (make it a choice at wifi selection)
 * Discord & screen awake status Jank af (use array?) 
 * 
 * 
 * WIFIMANAGER
   * LED feedback and LCD directions
   * Button override
   * Add location input
   * if inputted bypass location API
   * Default location
  
 * handle location result  
 * DISCORD!!
   * link ny2o
   * Boot info
  
 * Progress bar use max-(end-max) for start 
    * check if max gets sckewed after peak 
    * duration? xxx
    
 * SWITCH ALL ??:: to arrays
 * Call Signs
 */

// const String LATs[] = {}
// const String LONGs[] ={}

#define RED_LED 2
#define RAINBOW_LED 0

#define B1 16
#define B2 14
#define B3 12
#define B4 13
#define B5 15

#define UTC-5 -4*60*60 //TODO FIXME BADDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD

//#define GOOGLE_API_KEY "xxxxxxxxxxxxx"
//#define WEBHOOK "xxxxxxxxxxxxxxxx"
#define GOOGLE_API_KEY "AIzaSyCMebnUWyOQuw8xNj6oeMZsH7qBqbOqBbU"
#define WEBHOOK "https://discord.com/api/webhooks/1168805418320015411/7dY7KrnK_vq1H93M6f442cF5OTR29IlqCDzZrd_AXwy8re_8UZMLB1fqOnFgLUAWLLr7"

struct SSID_Loc {
  String NAME;
  String SSID;
  String KEY;
  String LAT;
  String LONG;
};

SSID_Loc LOC_ARR[5] = {
  {"Brackett",    "WPI Sailbot",  "xxxxxxxx",     "42.35111",   "-71.16504"},   //Brackett
  {"Albion",      "36Albion",     "xxxxxxxx",   "42.399647",  "-71.106448"},  //Albion
  {"Chatham",     "Tesfamily",    "Tes8628125601",  "40.740686",  "-74.384478"},  //Chatham
  {"Hi (MA)",     "Hi (3)",       "xxxxxxxx",       "42.399647",  "-71.106448"},  //Albion
  {"Hi (Pemi)",   "Hi (3)",       "xxxxxxxx",       "44.14429",   "-71.60423"},   //Pemi
};
SSID_Loc SelectedLocation;

// N2YO API Reference: https://www.n2yo.com/api/
// https://api.n2yo.com/rest/v1/satellite/radiopasses/25544/42.35111/-71.16504/0/1/10/&apiKey=C84895-P3LRCE-AFE97B-54RM
const String ISS_ID = "25544";

const String ALT = "0";
const String DAYS = "1";
const String MIN_ELE = "5"; 
const String API_KEY = "C84895-P3LRCE-AFE97B-54RM"; //NOTE: 1000 req/hour
String URL;
#define HOST  "api.n2yo.com"
#define PORT  443

WifiLocation location (GOOGLE_API_KEY);
WiFiClientSecure client;
HTTPClient https;
DynamicJsonDocument doc(4096);
LiquidCrystal_I2C lcd(0x27, 20, 4);

static time_t now;
int page = 1;
long lastTimeUpdate = 0;
int lastPage = 0;
bool flybyNow;
int lastFlybyNowState = -1;
bool peak = false;
bool apiUpdated = true;
//bool discord = false;
int passesToday;
long lastPassET;
int httpsCode;
int buttons = 0;
long lastPress;

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

Flyby nextFlyby;

void setup() {
  Serial.begin(115200);
  
  pinMode(RED_LED, OUTPUT);
  pinMode(RAINBOW_LED, OUTPUT);
  digitalWrite(RED_LED, LOW);
  digitalWrite(RAINBOW_LED, LOW);


  //###############
  //## LCD Setup ##
  //###############

  lcd.init();
  lcd.clear();
  lcd.backlight();
  lcd.createChar(0, progBar0);
  lcd.createChar(1, progBar1);
  lcd.createChar(2, progBar2);
  lcd.createChar(3, progBar3);
  lcd.createChar(4, progBar4);
  lcd.createChar(5, progBar5);
  lcd.createChar(6, moon);
  lcd.createChar(7, smiley);
  
  //################
  //## WIFI Setup ##
  //################
  int wifi_page = -1;
  while(buttons == 0){
    buttons = digitalRead(B5)<<4 | digitalRead(B4)<<3 | digitalRead(B3)<<2 | digitalRead(B2)<<1 | digitalRead(B1);

    int current_page = (millis() / 2000) % 2;
    if (current_page != wifi_page) {
      wifi_page = current_page;
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Select SSID: ");
      lcd.setCursor(1,1);
      if (wifi_page == 0){
        lcd.print("B1: " + LOC_ARR[0].NAME);
        lcd.setCursor(1,2);
        lcd.print("B2: " + LOC_ARR[1].NAME);
        lcd.setCursor(1,3);
        lcd.print("B3: " + LOC_ARR[2].NAME);

      } else {
        lcd.print("");
        lcd.setCursor(1,2);
        lcd.print("B4: " + LOC_ARR[3].NAME);
        lcd.setCursor(1,3);
        lcd.print("B5: " + LOC_ARR[4].NAME);

      }
    }
    if (millis() > 20000) buttons = B00100;
    delay(10);
  }

  SelectedLocation = LOC_ARR[__builtin_ctz(buttons)];
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SSID Selected:");

  lcd.setCursor(round((20-SelectedLocation.SSID.length())/2), 2);
  lcd.print(SelectedLocation.SSID);
  lcd.setCursor(0, 3);
  lcd.print(SelectedLocation.LAT + ",");
  lcd.setCursor(10, 3);
  lcd.print(SelectedLocation.LONG);
  delay(2000);

  WiFi.mode(WIFI_STA);
  WiFi.begin(SelectedLocation.SSID, SelectedLocation.KEY);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Connecting WiFi: ");
  for(int i=0; WiFi.status() != WL_CONNECTED; i++) printLoadingIcons(18, 0, i);
  lcd.setCursor(18, 0);
  lcd.printByte(7);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  Serial.println("\n\nWiFi Connected");


  //####################
  //## NTP Time Setup ##
  //####################
  lcd.setCursor(0,1);
  lcd.print("NTP Server: ");
  configTime("GMT0", "pool.ntp.org", "time.nist.gov");
  for (time_t now = time (nullptr); now < 8 * 3600 * 2; now = time (nullptr)) delay (500);
  yield();
  delay(500); //NOTE
  lcd.setCursor(18, 1);
  lcd.printByte(7);
  Serial.println("NTP Server Synchronized: " + getFormattedTime((uint32_t)  time (nullptr), false));


  //####################
  //## Location Setup ##
  //####################
  lcd.setCursor(0, 2);
  lcd.print("Location API: ");
//  location_t loc = location.getGeoFromWiFi(); //BUG!!!
//  LAT = String(loc.lat, 7);
//  LONG = String(loc.lon, 7);
  URL = "/rest/v1/satellite/radiopasses/" + ISS_ID + "/" + SelectedLocation.LAT + "/" + SelectedLocation.LONG + "/" + ALT + "/" + DAYS + "/" + MIN_ELE + "/&apiKey=" + API_KEY;
  lcd.setCursor(18, 2);
  lcd.printByte(7);
  Serial.println("WiFiLocation Result: " + location.wlStatusStr (location.getStatus ()) + "    " + SelectedLocation.LAT + ", " + SelectedLocation.LONG); //TODO handle bad status (ie halt int)

  //####################
  //## N2YO API Setup ##
  //####################
  lcd.setCursor(0, 3);
  lcd.print("N2YO API: ");
  https.setReuse(false);
  client.setInsecure();
  sendDiscord("Tracker Connected", 65280);
  lcd.setCursor(18, 3);
  lcd.printByte(7);
  lastPress = time(nullptr);
}

void loop() {
    now = time(nullptr); // + (6*60 + 10)*60;

  //################
  //## API Update ##
  //################
  /*
   * API Updates:
      * if nextFlyby isn't initialized
      * Every 60 seconds except 2 minutes before nextFlyby.startUTC
      * if now > nextFlyby.endUTC
   */
   
  if( !nextFlyby.startUTC  ||  ((uint32_t) now % 60 == 0) && !(nextFlyby.startUTC-120 < (uint32_t) now)  ||  (flybyNow && (uint32_t) now > nextFlyby.endUTC) ){
    bool a = https.begin(client, HOST, PORT, URL);
    httpsCode = https.GET();
    Serial.println("Send NY2O GET Code: " + (String) httpsCode + ", " + https.errorToString(httpsCode));

    if(httpsCode == 200){
      apiUpdated = true;
      deserializeJson(doc, https.getString()); 
      JsonObject nextPass = doc["passes"][0].as<JsonObject>();

      nextFlyby = {
          (double) nextPass["startAz"], (String) nextPass["startAzCompass"], nextPass["startUTC"],
          (double) nextPass["maxAz"], (String) nextPass["maxAzCompass"], (double) nextPass["maxEl"], (unsigned long) nextPass["maxUTC"],
          (double) nextPass["endAz"], (String) nextPass["endAzCompass"], (unsigned long) nextPass["endUTC"]
      };

      if((nextFlyby.endUTC - 2*nextFlyby.maxUTC + nextFlyby.startUTC) > 5){
        Serial.println("StartUTC: " + (String) nextFlyby.startUTC);
        Serial.println("MaxUTC: " + (String) nextFlyby.maxUTC);
        Serial.println("EndUTC: " + (String) nextFlyby.endUTC);
        Serial.println("2*MAX: " + (String) (2*nextFlyby.maxUTC) );

        nextFlyby.startUTC = 2*nextFlyby.maxUTC-nextFlyby.endUTC;

        Serial.println("StartUTC: " + (String) nextFlyby.startUTC);
        Serial.println("NOW: " + (String) (uint32_t) now);

      }

      //For # Passes today
      lastPassET = 0;
      long days = floor(((uint32_t) now + UTC-5)/(24*60*60));
      for(passesToday=0; passesToday<doc["passes"].size(); passesToday++){
        long startET = (long) doc["passes"][passesToday]["startUTC"]+(UTC-5);
        if (floor(startET/(24*60*60))>days) break;
        lastPassET = startET;
      }
//      sendFlybyDiscord(16776960, nextFlyby);
    } else {
      sendErrorDiscord(httpsCode, 16711680);
    }
  }
  
  
  //##############################
  //## Buttons, LEDs, and State ##
  //##############################

  buttons = digitalRead(B5)<<4 | digitalRead(B4)<<3 | digitalRead(B3)<<2 | digitalRead(B2)<<1 | digitalRead(B1);

  if(buttons > 0){
    lastPress = now;
    page = __builtin_ctzl(buttons)+1;
  }
  if(now-lastPress > 60) page=1;


  flybyNow = nextFlyby.startUTC && nextFlyby.startUTC <= (uint32_t) now;
  peak = abs((signed long) nextFlyby.maxUTC - (signed long) now) < (nextFlyby.endUTC - nextFlyby.startUTC)*.25/2;  //Peak at top 25% of traj
  digitalWrite(RAINBOW_LED, flybyNow);
  if(millis() % 250 <= 5) digitalWrite(RED_LED, peak && !digitalRead(RED_LED));


  //######################
  //## Discord Messages ##
  //######################

  if(flybyNow != lastFlybyNowState){
    if(flybyNow) sendFlybyDiscord("Flyby NOW!!!!", 32767, nextFlyby);
    else sendFlybyDiscord("Next Flyby:", 16760576, nextFlyby);
  }
  lastFlybyNowState = flybyNow;

 
  //##################
  //## LCD Printing ##
  //##################
  /*
   * LCD Updates when:
   *      a page changes
   *      the time changes and page 1 is selected
   *      the API updates
   */
  if(page != lastPage || ( (lastTimeUpdate != (uint32_t) now) && page == 1) || apiUpdated){
    
    lcd.clear();
    lcd.setCursor(0,0);
    lastPage = page;
    lastTimeUpdate = (uint32_t) now;
    apiUpdated = false;

    if(!nextFlyby.startUTC){
      lcd.setCursor(0,1);
      lcd.print("     Loading...");
      return;
    }
    
    switch(page){
      case 1: //ISS next pass in

        if(!flybyNow){
          lcd.print("Next ISS Pass in");
          lcd.setCursor(0,1);
          lcd.print(getFormattedTime(nextFlyby.startUTC - (uint32_t) now));
          lcd.setCursor(0,2);

          lcd.setCursor(0,3);
          lcd.print("ISS Time: " + getFormattedTime((uint32_t) now, false));
        } else {
          lcd.print("ISS Passing NOW!");
          lcd.setCursor(0,1);
          if(peak) lcd.print("        PEAK!");
          lcd.setCursor(0,2);
          printProgressBar(20, nextFlyby.startUTC, (uint32_t) now, nextFlyby.endUTC);
          lcd.setCursor(0,3);
          printStartEndAz();
        }
        lcd.setCursor(19,0);
        lcd.printByte(getAwakeStatus(nextFlyby.startUTC)?((getAwakeStatus(nextFlyby.startUTC)-1)?239:7):6); //printAwakeIcon
        break;
      
      case 2:
        lcd.print("At:   " + getFormattedTime(nextFlyby.startUTC + UTC-5, false) + " EST");
        lcd.setCursor(0,1);
        lcd.print("Dur:  " + getFormattedTime(nextFlyby.endUTC - nextFlyby.startUTC));
        lcd.setCursor(0,2);
        lcd.print("MaxE: " + (String) nextFlyby.maxEl + (char)223);
        lcd.setCursor(0,3);
        printStartEndAz();
        break;
              
      case 3:
        lcd.print("Passes Left Today: " + (String) passesToday);
        lcd.setCursor(0,1);
        lcd.print("Last: " + ((lastPassET) ? getFormattedTime(lastPassET, false) + " EST" : "N/A"));
        lcd.setCursor(0,2);        
        break;

      case 4:
        lcd.print("    Frequencies");
        lcd.setCursor(0,1);
        lcd.print("Down: 437.800 MHz");
        lcd.setCursor(0,2);
        lcd.print("Up:   145.990 MHz");
        lcd.setCursor(0,3);
        lcd.print("PL:   67 MHz");
        break;

      case 5:
        lcd.print("WIFI Status: " + ((WiFi.status()==3) ? (String)(char)7 : (String) WiFi.status()));
        lcd.setCursor(0,1);
        lcd.print("API Status:  " + (String) (client.connected()?(char)7:'!') + " (" + (String) httpsCode + ")");
        lcd.setCursor(0,2);
        lcd.print("Lat:   " + SelectedLocation.LAT);
        lcd.setCursor(0,3);
        lcd.print("Long: " + SelectedLocation.LONG);
        break;
    }
  }
}


String getFormattedTime(unsigned long rawTime) {
  return getFormattedTime(rawTime, true);
}

String getFormattedTime(unsigned long rawTime, bool isDur) {
  unsigned long hours = rawTime / 3600;
  if(!isDur) hours %= 24;
  String hoursStr = hours < 10 ? "0" + String(hours) : String(hours);

  unsigned long minutes = (rawTime % 3600) / 60;
  String minuteStr = minutes < 10 ? "0" + String(minutes) : String(minutes);

  unsigned long seconds = rawTime % 60;
  String secondStr = seconds < 10 ? "0" + String(seconds) : String(seconds);

  return isDur ? (hoursStr + "h " + minuteStr + "m " + secondStr + "s") : (hoursStr + ":" + minuteStr + ":" + secondStr);
}


void printProgressBar(int length, long start, long current, long end){
  printProgressBar(length, (float) (current-start)/(end-start));
}

void printProgressBar(int length, float percent){
  lcd.print("(");
  int ticks = (length-2)*5*percent; 
  for(int i = 0; i<length-2; i++){
    int cell = (ticks<5)?ticks:5;
    lcd.printByte(cell);
    ticks -= cell;
  }
  lcd.print(")");
}

void printStartEndAz(){
  String str = (String)(int) round(nextFlyby.startAz) + (char)223 + nextFlyby.startAzCompass + " " +(char)126 + " " + (String)(int) round(nextFlyby.endAz) + (char)223 + nextFlyby.endAzCompass;
  for(int i=0; i<round((20-str.length())/2); i++) lcd.write(' ');
  lcd.print(str);
}


/*
 * GREAT : 6:00-8:30 UTC
 * OK :    8:30-18:30 UTC
 * GREAT : 18:30-20:00 UTC
 * ASLEEP: 20:00-6:00 UTC
 */

int getAwakeStatus(long time){
  const int great_start = 6*60*60;
  const int ok_start = 8.5*60*60;
  const int ok_end = 18.5*60*60;
  const int asleep_start = 20*60*60;

  int daySecs = time % (24*60*60);
     
  if(daySecs < great_start || daySecs > asleep_start) return 0; //SLEEP
  else if(daySecs > ok_start && daySecs < ok_end) return 1;  //OK
  else return 2;  //GREAT
  
}

void printLoadingIcons(int x, int line, int i){
  lcd.setCursor(x, line);
  lcd.print( !(i%3) ? "|" : (i%3==1 ? "/" : "-" ) );
  delay(250);
}

//https://www.instructables.com/Send-a-Message-on-Discord-Using-Esp32-Arduino-MKR1/
//https://github.com/maditnerd/discord_test/blob/master/discord_test_esp8266/discord.h
//https://birdie0.github.io/discord-webhooks-guide/discord_webhook.html
//32767
//16776960
void sendFlybyDiscord(String title, int color, Flyby thisFlyby){
  String fullTitle = "**" + title + "** \\n\\u0000";
  String duration = getFormattedTime(thisFlyby.endUTC - thisFlyby.startUTC);
  String awake = getAwakeStatus(thisFlyby.startUTC)?((getAwakeStatus(thisFlyby.startUTC)-1)?"Awake and free!":"Awake"):"Asleep";
  String startET = getFormattedTime(thisFlyby.startUTC + UTC-5, false) + " EST";
  String maxET = getFormattedTime(thisFlyby.maxUTC + UTC-5, false) + " EST";
  String endET = getFormattedTime(thisFlyby.endUTC + UTC-5, false) + " EST";
  String startCompass = (String)(int)round(thisFlyby.startAz) + "° " + thisFlyby.startAzCompass;
  String maxCompass = (String)(int)round(thisFlyby.maxAz) + "° " + thisFlyby.maxAzCompass;
  String endCompass = (String)(int)round(thisFlyby.endAz) + "° " + thisFlyby.endAzCompass;
  String maxEl = "El: " + (String) thisFlyby.maxEl + "°";


  bool a = https.begin(client, WEBHOOK);
  https.addHeader("Content-Type", "application/json");
  int code = https.POST("{\"content\":\"\",\"embeds\": [{\"title\": \"" + fullTitle + "\", \"color\": " + (String) color + ", \"fields\": [{\"name\": \"__Duration__\",\"value\": \"" + duration + "\\n\\u0000\"}, {\"name\": \"__Sleep Status__\",\"value\": \"" + awake + "\\n\\u0000\"}, {\"name\": \"__Start__\",\"value\": \"" + startET + "\\n" + startCompass + "\\n\\u0000\", \"inline\": true}, {\"name\": \"__Max__\",\"value\": \"" + maxET + "\\n" + maxCompass + "\\n" + maxEl + "\\n\\u0000\", \"inline\": true}, {\"name\": \"__End__\",\"value\": \"" + endET + "\\n" + endCompass + "\", \"inline\": true}] }],\"tts\":false}");
  Serial.println("Send Discord FLYBY POST Code: " + (String) code + ", " + https.errorToString(code));
}

//16711680
void sendErrorDiscord(int errorCode, int color){
  sendDiscord("**ERROR:  **" + (String) errorCode + "   " + https.errorToString(errorCode), color);
}

void sendDiscord(String subContent, int color){
  String content = "";
  bool a = https.begin(client, WEBHOOK);
  https.addHeader("Content-Type", "application/json");
  int code = https.POST("{\"content\":\"" + content + "\",\"embeds\": [{\"color\": " + (String) color + ", \"fields\": [{\"name\": \"" + subContent + "\", \"value\": \"\"\}] }],\"tts\":false}");
  Serial.println("Send Discord POST Code: " + (String) code + ", " + https.errorToString(code));
}
