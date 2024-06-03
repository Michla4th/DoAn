#include <FS.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#define relay_pin 12 // D5 
#define led_pin 2    // D4 

#define WIFI_SSID "WIFI_AP"
#define WIFI_PASSWORD "WIFI_PASSWORD"

// AP mode
const char *ssid_AP = "ESP8266";
const char *password_AP = "12345678";

ESP8266WebServer server(80);
bool connect = 0;

// Fire base
#include <Firebase_ESP_Client.h>
// Provide the token generation process info.
#include "addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"
// Insert Firebase project API Key
#define API_KEY "API_KEY"
// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "URL"

// Pzem 004t
#include <PZEM004Tv30.h>
#include <SoftwareSerial.h>

#if !defined(PZEM_RX_PIN) && !defined(PZEM_TX_PIN)
#define PZEM_RX_PIN 14 // D6
#define PZEM_TX_PIN 16 // D7
#endif

SoftwareSerial pzemSWSerial(PZEM_RX_PIN, PZEM_TX_PIN);
PZEM004Tv30 pzem(pzemSWSerial);

// OLED
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// pin connect oled
// #define SDA_PIN D2
// #define SCL_PIN D1

Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
byte change=0;
byte i=0;

#include <time.h>
#include <ezTime.h>
Timezone myTZ;

// const char* ntpServer = "pool.ntp.org";
// const long  gmtOffset_sec = 7*60*60;
// const int   daylightOffset_sec = 0;

// Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool signupOK = false;

// Timer variables (send new readings every three minutes)
unsigned long sendDataPrevMillis = 0;
unsigned long sendDataPrevMinute = 0;
unsigned long timeCutRelay = 0;

FirebaseJson json;
FirebaseJson jsonSetting;
FirebaseJson jsonDataBase;
String parentPath;

// Room information
String room = "4";

float voltage = 0;
float current = 0;
float power = 0;
float energy = 0;
float frequency = 0;
float pf = 0;
// user set up
int OV = 270;     
int UV = 170;
int OC = 40;
int OP = 3000;
bool relay = true;
int Error = false;
bool alarm = false;

//========================== Function ==========================//
//==============================================================//


//========================== TimerESP ==========================//
String TimeNow()
{
  String now = String(myTZ.year()) +"/"+String(myTZ.month())+"/" + String(myTZ.day())+"/"+String(myTZ.hour())+"/"+String(myTZ.minute());
  return now;
}

//========================== Read Pzem =========================//
void Read_pzem(bool print=false)
{
  timeCutRelay = millis();
  voltage = pzem.voltage();
  current = pzem.current();
  power = pzem.power();
  energy = pzem.energy();
  frequency = pzem.frequency();
  pf =  pzem.pf();

  if (isnan(voltage))     voltage = 0;
  if (isnan(current))     current=0;
  if (isnan(power))       power=0;
  if (isnan(energy))      energy=0;
  if (isnan(frequency))   frequency=0;
  if (isnan(pf))          pf=0;
  
  Error_event();
  // Convert to JsonData
  // database
    jsonDataBase.set("U", voltage);
    jsonDataBase.set("I", current);
  // realtime
    json.set("Voltage", voltage);
    json.set("Current", current);
    json.set("Power", power);
    json.set("Energy", energy);
    json.set("Frequency", frequency);
    json.set("PowerFactor", pf);
  // setting
    jsonSetting.set("relay", relay);
    jsonSetting.set("error", Error);
    jsonSetting.set("alarm", alarm);

    jsonSetting.set("OV", OV);
    jsonSetting.set("UV", UV);
    jsonSetting.set("OC", OC);
    jsonSetting.set("OP", OP);
  
  // print value
  if(print)
  {
    Serial.print("Voltage: ");      Serial.print(voltage);      Serial.println("V");
    Serial.print("Current: ");      Serial.print(current);      Serial.println("A");
    Serial.print("Power: ");        Serial.print(power);        Serial.println("W");
    Serial.print("Energy: ");       Serial.print(energy,3);     Serial.println("kWh");
    Serial.print("Frequency: ");    Serial.print(frequency, 1); Serial.println("Hz");
    Serial.print("PF: ");           Serial.println(pf);
    Serial.println();
  }
}

void Reset_pzem()
{
  if(myTZ.day()==15 && myTZ.hour()==0 && myTZ.minute()==0 && myTZ.second()==0)
  {
    pzem.resetEnergy();
    Serial.println("Reset energy");
    Serial.println();
  } 
}

// relay event
void ValueSetting()
{
  jsonSetting.set("OV", OV);
  jsonSetting.set("UV", UV);
  jsonSetting.set("OC", OC);
  jsonSetting.set("OP", OP);

  jsonSetting.set("relay", relay);
  jsonSetting.set("error", Error);
  jsonSetting.set("alarm", alarm);

  if(checkWifi() && Firebase.ready()) sendSetting(); 
}

void Error_event()
{
  // OV
  if(Error==0)
  {
    if(voltage > OV)
    {
      delay(0);
      digitalWrite(relay_pin, LOW);
      Serial.print("Time cut relay: "); Serial.print(millis() - timeCutRelay); Serial.println(" ms");
      Error = 1;
      relay = false;
      Serial.println("An overvoltage error has occurred");
      Serial.println();
      ValueSetting();
    }
    // UV
    if(voltage < UV)
    {
      delay(0);
      digitalWrite(relay_pin, LOW);
      Serial.print("Time cut relay: "); Serial.print(millis() - timeCutRelay); Serial.println(" ms");
      Error = 2;
      relay = false;
      Serial.println("A low voltage error has occurred");
      Serial.println();
      ValueSetting();
    }
    // OC
    if(current > OC)
    {
      delay(0);
      digitalWrite(relay_pin, LOW);
      Serial.print("Time cut relay: "); Serial.print(millis() - timeCutRelay); Serial.println(" ms");
      Error = 3;
      relay = false;
      Serial.println("An overcurrent error has occurred");
      Serial.println();
      ValueSetting();
    }
    // op
    if(power > OP)
    {
      delay(0);
      Serial.print("Time cut relay: "); Serial.print(millis() - timeCutRelay); Serial.println(" ms");
      Error = 4;
      Serial.println("An overpower error has occurred");
      Serial.println();
      ValueSetting();
    }  
  }
  if(UV < voltage < OV & current < OC)
  {
    if(Error) // check if had error befor 
    {
      delay(2000);
      if(pzem.voltage()>UV && pzem.voltage()<OV && pzem.current()<OC)
      {
        digitalWrite(relay_pin, HIGH);
        Error = 0;
        relay = true;
        Serial.println("The electrical problem is over");
        Serial.println();
        ValueSetting();
      }
    }
  }
}
//==============================================================//


//======================== OLED Display ========================//
void clock_display()
{
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.printf("%02d:%02d:%02d", myTZ.hour(), myTZ.minute(), myTZ.second());
  oled.display();
}

void wifi_icon(int x=2, int y=51)
{
  oled.drawPixel(x+0, y+2, SSD1306_WHITE);
  oled.drawPixel(x+1, y+1, SSD1306_WHITE);
  oled.drawLine(x+2,y+0,x+8,y+0, SSD1306_WHITE);
  oled.drawPixel(x+9, y+1, SSD1306_WHITE);
  oled.drawPixel(x+10, y+2, SSD1306_WHITE);

  oled.drawPixel(x+2, y+5, SSD1306_WHITE);
  oled.drawPixel(x+3, y+4, SSD1306_WHITE);
  oled.drawLine(x+4,y+3,x+6,y+3, SSD1306_WHITE);
  oled.drawPixel(x+7, y+4, SSD1306_WHITE);
  oled.drawPixel(x+8, y+5, SSD1306_WHITE);

  oled.drawCircle(x+5, y+8, 1, SSD1306_WHITE);

  oled.display();
}

void display_oled()
{
    oled.clearDisplay();
    clock_display();
    if(checkWifi()) wifi_icon();
    oled.setCursor(89, 0);
    oled.setTextSize(2); oled.println("kWh");
    oled.setTextSize(3); oled.printf("%7.2f\n", energy); 
   
    oled.setTextSize(2);
    oled.setCursor(4, 45);
    if(change==0) {oled.printf("%9.2fV", voltage);}
    if(change==1) {oled.printf("%9.3fA", current);}
    if(change==2) {oled.printf("%9.2fW", power);}
    if(change==3) {oled.printf("%8.1fHz", frequency);}
    if(change==4) {oled.printf("%10.2f", pf);}
    oled.display();
    change++; if(change>4) change=0;
}
//==============================================================//


//========================= Fire Base ==========================//
void sendData()
{
  parentPath = "rooms/" + room + "/data";
  Serial.printf("Set jsonData... %s\n", Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());
  Serial.println();
}

void sendSetting()
{
  parentPath = "rooms/" + room + "/setup";
  Serial.printf("Set jsonSetting... %s\n", Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &jsonSetting) ? "ok" : fbdo.errorReason().c_str());
  Serial.println();
}

void sendDataBase()
{
  parentPath = "rooms/" + room + "/database/" + TimeNow();
  Serial.printf("Set jsonDataBase... %s\n", Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &jsonDataBase) ? "ok" : fbdo.errorReason().c_str());
  Serial.println();
}

void getData()
{
  String path = "rooms/" + room + "/setup";
  // Serial.printf("Get jsonSetting... %s\n", Firebase.RTDB.getJSON(&fbdo, path) ? fbdo.to<FirebaseJson>().raw() : fbdo.errorReason().c_str());
  if(Firebase.RTDB.getJSON(&fbdo, path))
  {
    Serial.printf("Get jsonSetting... ok\n");
    Serial.println();
    String oject = fbdo.to<FirebaseJson>().raw();
    
    JsonDocument doc;
    deserializeJson(doc, oject);

    OV = int(doc["OV"]);
    UV = int(doc["UV"]);
    OC = int(doc["OC"]);
    OP = int(doc["OP"]);    
    relay = bool(doc["relay"]);
  }
  else {Serial.printf("Get jsonSetting... %s\n", fbdo.errorReason().c_str()); Serial.println();}
  Read_pzem();
  digitalWrite(relay_pin, relay);
}
//==============================================================//


//======================= Flash DataBase =======================//
bool saveData(const char* filename, String content)
{
  SPIFFS.begin();
  File f = SPIFFS.open(String("/") + filename, "w");
  if(!f) {f.close(); return false;}
  else
  {
    f.print(content);
    f.close(); 
    return true;
  }
}

String readData(const char* filename)
{
  SPIFFS.begin();
  File f = SPIFFS.open(String("/") + filename, "r");
  String ret = f.readString();
  f.close();
  return ret;
}

void save_data_flash()
{
  if(myTZ.year() > 1900)
  {
    String Path = String("/database/") + TimeNow() + ".txt";
    String data = String("{\"path\":\"") + TimeNow() + "\",\"U\":" + String(voltage) + ",\"I\":" + String(current) + ",\"P\":" + String(power) + "}";
    // save to flash
    SPIFFS.begin();
    File f = SPIFFS.open(Path, "w");
    f.print(data);
    f.close(); 
    Serial.println("Save data to flash");
    Serial.println();
  }
}

void send_data_flash()
{
  Dir dir = SPIFFS.openDir("/database");
  while (dir.next()) 
  {
    Serial.print(dir.fileName());
    if(dir.fileSize()) 
    {
      Serial.println("have data in flash");
      File f = dir.openFile("r");
      String s = f.readString();
      Serial.print("file content: ");
      Serial.print(s);
      Serial.print(" ");
      Serial.print("file size: ");
      Serial.println(f.size());

      JsonDocument doc;
      deserializeJson(doc, s);

      String parentPath = "rooms/" + room + "/database/" + String(doc["path"]);

      jsonDataBase.set("U", float(doc["U"]));
      jsonDataBase.set("I", float(doc["I"]));
      jsonDataBase.set("P", float(doc["P"]));

      json.toString(Serial, true);
      Serial.printf("Dend database form flash... %s\n", Firebase.RTDB.setJSON(&fbdo, parentPath, &jsonDataBase) ? "ok" : fbdo.errorReason().c_str());
      Serial.println();
      f.close();
      SPIFFS.remove(dir.fileName());
    }
    else Serial.println("None data in flash database");
  }
}
//==============================================================//


//==================== realtime and database ===================// 
bool checkWifi()
{
  if(WiFi.status() == WL_CONNECTED) return 1;
  else return 0;
}

void realtime()
{
  if ((millis() - sendDataPrevMillis > 1000 || sendDataPrevMillis == 0))
  {
    sendDataPrevMillis = millis();
    Read_pzem(true);
    if (checkWifi() && Firebase.ready())
    {
      getData();
      send_data_flash();
      sendData();
      sendDataBase();
    }
    else {Serial.println("WiFi disconnected or lost internet"); Serial.println();}
  }
}
  // send database
void database()
{
  if ((millis() - sendDataPrevMinute > 60000 || sendDataPrevMinute == 0))
  {
    sendDataPrevMinute = millis();
    Read_pzem();
    if (!checkWifi() && !Firebase.ready())
    {
      Serial.println("Save data to flash");
      save_data_flash();
    }
    else
    {
      // if(!Error_event()) getData();
      // sendSetting(); 
      // sendDataBase();
    }
  }
}
//==============================================================//


//========================= Void setup =========================//  
void setup()
{
  // Set up pin in out
  pinMode(led_pin, OUTPUT);
  pinMode(relay_pin, OUTPUT);
  digitalWrite(led_pin, LOW);
  digitalWrite(relay_pin, HIGH);
  // Default setup
  Read_pzem();
  Serial.begin(115200);
  delay(2000);

//========================= OLED setup =========================//   
  if (!oled.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)){
    Serial.println(F("SSD1306 Allocation Failed"));
    for(;;);
  }
  else Serial.println(F("SSD1306 Allocation Successed"));
  oled.display();
  delay(2000);
  oled.clearDisplay();
  // set text
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);
//==============================================================//  


//======================== Connect Wifi ========================//
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(readData("wifi.txt"), readData("pass.txt"));
  if(readData("wifi.txt") && readData("pass.txt"))
  {
    WiFi.begin(readData("wifi.txt"), readData("pass.txt"));
    Serial.println("");
    Serial.print("Connecting to Wifi: ");
    Serial.println(readData("wifi.txt"));  
    Serial.print("Password: ");
    Serial.println(readData("pass.txt"));             oled.print(readData("wifi.txt")); oled.display();
    int timer=0;
    while (WiFi.status() != WL_CONNECTED) 
    {
      delay(500);
      Serial.print(".");                              oled.print("."); oled.display();
      timer++;
      if(timer > 40)
      {
        Serial.println();                             oled.println(); oled.display();
        Serial.println("Connection failed");          oled.println("Failed"); oled.display();
        connect = 0;
        WiFi.disconnect();
        break;
      } 
      connect = 1;
    }
    if(connect)
    {
      Serial.println();                               oled.println();
      Serial.println("Connected successfully");       oled.println("Connected");
      Serial.print("IP address: ");                   oled.print("IP: ");
      Serial.println(WiFi.localIP());                 oled.println(WiFi.localIP()); oled.display();
      WiFi.setAutoReconnect(true);
      WiFi.persistent(true);
    }
  }

  // WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  // Serial.print("Connecting to Wi-Fi: ");          
  // Serial.print(WIFI_SSID);                        oled.print(WIFI_SSID); oled.display();
  // while (WiFi.status() != WL_CONNECTED){
  //   Serial.print(".");                            oled.print("."); oled.display();
  //   delay(300); 
  // }
  // Serial.println();                               oled.println();
  // Serial.println("Connected successfully");       oled.println("Connected");
  // Serial.print("IP address: ");                   oled.print("IP: ");
  // Serial.println(WiFi.localIP());                 oled.println(WiFi.localIP()); oled.display();
  // Serial.println();
  // WiFi.setAutoReconnect(true);
  // WiFi.persistent(true);
//==============================================================//


//========================== Webserver =========================//
  server.on("/", []() {
    String html = "<form action='/update' method='POST'>"
                  "SSID: <input type='text' name='ssid'><br>"
                  "Password: <input type='password' name='password'><br>"
                  "<input type='submit' value='Update'>"
                  "</form>";
    server.send(200, "text/html", html);
  });

  server.on("/update", []() {
    String new_ssid = server.arg("ssid");
    String new_password = server.arg("password");
    if(saveData("wifi.txt", new_ssid) & saveData("pass.txt", new_password)) Serial.println("Successfully changed wifi");
    if (new_ssid.length() > 0 && new_password.length() > 0) {
      server.send(200, "text/html", "Updating WiFi credentials. Reboot the device.");
      delay(1000);
      ESP.restart();
    } else {
      server.send(400, "text/html", "Invalid input. Please try again.");
    }
  });

  server.begin();
//==============================================================//


//========================== Fire Base =========================//
  if(connect)
  {
  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Sign up */
  Serial.print("Connecting to FireBase... ");     oled.print("FireBase..."); oled.display();
  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("ok");                         oled.println("ok"); oled.display();
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
    oled.printf("%s\n", config.signer.signupError.message.c_str()); 
    oled.display();
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
  }
//==============================================================//

  
//==============================================================//
  if(connect)
  {
    // send setting default
    // jsonSetting.set("relay", true);
    // jsonSetting.set("error", false);
    // jsonSetting.set("alarm", false);

    // jsonSetting.set("OV", 270);
    // jsonSetting.set("UV", 170);
    // jsonSetting.set("OC", 40);
    // jsonSetting.set("OP", 3000);

    // sendSetting();
    getData();

    waitForSync();
    myTZ.setLocation(F("Asia/Ho_Chi_Minh"));
  }
  else myTZ.setLocation(F("Asia/Ho_Chi_Minh"));

  Serial.print(F("Ho Chi Minh:     "));
	Serial.println(myTZ.dateTime());
  oled.print(" ");
  oled.println(myTZ.dateTime());
  oled.display();
  delay(5000);
}
  
//========================== Void Loop =========================//  
void loop()
{   
  server.handleClient();
  events();
  display_oled();
  Read_pzem();
  realtime();
  database();
}







   
