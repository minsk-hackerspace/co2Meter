
/*
 * TODO:
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 */

extern "C" { //timer
#include "user_interface.h"
}


#include <ArduinoOTA.h> //https://github.com/esp8266/Arduino/tree/master/libraries/ArduinoOTA

#include <EEPROM.h>
#include <FS.h>

#include <SoftwareSerial.h> //Arduino lib SoftwareUART for MH-Z19

#include <MHZ19.h>  //https://github.com/strange-v/MHZ19
#include <MHZ19PWM.h>


#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson
#include <ESP8266WiFi.h> //https://github.com/esp8266/Arduino

#include <jled.h> //https://github.com/jandelgado/jled

#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager
#include <DNSServer.h> //required for WiFiManager
#include <ESP8266WebServer.h> //required for WiFiManager

#include <PubSubClient.h> // https://github.com/knolleary/pubsubclient


//for display
#include <Wire.h>
#include <SPI.h>
#include "SH1106.h"   //https://github.com/rene-mt/esp8266-oled-sh1106

//for DGT sensor
#include "DHTesp.h" //https://github.com/beegee-tokyo/DHTesp
DHTesp dht;
// comment for disable logging
#define DEBUG

#ifdef DEBUG
 #define DLG(x)  Serial.println(x);//Serial1.println(x);
#else
 #define DLG(x)
#endif

#define min(a,b) ((a)<(b)?(a):(b))

//const int LED_PIN = D0; //HADWARE LED
const int LED_PIN = LED_BUILTIN; //HADWARE LED PIN D0

const int HD_PIN = D3; //HARDWARE FLASH BTN
//const int DHT_PIN = D3; //SHARED wit hardware flash button
const int DHT_PIN = 9; //GPIO9 SDD2 SD2 Not available on some ESP (QIO mode)


const int SOFT_TX_PIN = D4; //tx 
const int SOFT_RX_PIN = D1; //rx

//for conecting with MH-Z19
SoftwareSerial mySerial(SOFT_RX_PIN, SOFT_TX_PIN);

MHZ19 mhz(&mySerial);


// SPI OLED display 128x64

//hadware SPI - to OLED pins
//const int OLED_CLK  = D5; //GPIO14 - CLK (D0)
//const int OLED_MISO = D6; //GPIO12 - not connected
//const int OLED_MOSI = D7; //GPIO13 - MOSI (D1)
const int OLED_CS     = D8; //GPIO15 - Chip select (CS)
const int OLED_DC     = D2; //GPIO4  - Data/Command (DC)
const int OLED_RESET  = 10; //GPIO10 SDD3 - RESET (RST)
SH1106 display(true, OLED_RESET, OLED_DC, OLED_CS); // FOR SPI


// I2C OLED display 128x64
//#define OLED_ADDR   0x3C
//hadware I2C - to OLED pins
//const int OLED_SDA    = D7; //GPIO13 - SDA  (D1)
//const int OLED_SDC    = D5; //GPIO14 - CLK (D0)
//SH1106   display(OLED_ADDR, OLED_SDA, OLED_SDC);    // For I2C

int ppm = 0; //Текущее значение уровня СО2
int temp = 0; //Текущее значение температуры
int acc = 0; //Текущее значение точности

int dht_temp = 0; //last measured Temperature from DHT
int dht_hum = 0; //last measured Humidity from DHT

#define PPMS_L 120
int ppms[PPMS_L]= {};


int avg_ppm = 0; //average СО2 level for period measure_period
int avg_ppm_summ = 0; //measures summ
int avg_measures = 0; //measures count

unsigned int last_measured_time = 0; //last measure timestamp
unsigned int measured_time = 0; //measure time accumulator
const unsigned int measure_period = 1000*60*2; //2 minutes

const int measure_millis = 10000; //period between measurments in milliseconds


/*
 * 
 * SATUS LED
 * 
 */
JLed led = JLed(LED_PIN);//.LowActive();


/*
 * 
 * Main Funcs
 * 
 */
void reboot(void) {
  led.Blink(10,50).Forever();
  DLG("Restarting...");
  delay(2000);
  ESP.restart();
}


/*
 * 
 * OTA 
 * 
 */
bool OTA = false;

void setupOTA() {
    
    DLG("Starting OTA");

    ArduinoOTA.setHostname("CO2 Meter OTA");
    ArduinoOTA.onStart([]() { 
                    Serial.println(" >> OTA onStart");
                    });

    ArduinoOTA.onEnd([]() { 
                        Serial.println(" >> OTA onEnd");
                        });

    ArduinoOTA.onError([](ota_error_t error) { 
                      Serial.println(" >> OTA onError");
                      reboot();
                      });

    led.Breathe(1000).Forever();                      
    OTA = true;
   /* setup the OTA server */
    ArduinoOTA.begin();

    DLG("OTA ready");
  
  }

/*
 * 
 * WiFiManager 
 * 
 */
void configModeCallback (WiFiManager *wifiManager) {
  DLG("Entered config portal mode");
  led.Blink(400,100).Forever();  
  
  display.clear();
  display.drawString(0, 0, "Could not connect to Wi-Fi");
  display.drawString(0, 10, "Starting access point...");
  display.display();
  String ip = String(WiFi.softAPIP().toString());
  String ap = wifiManager->getConfigPortalSSID();
  
  
  display.drawString(20, 24, "SSID:"+ap);
  display.drawString(20, 34, "IP:"+ip);
  display.display();
  
  DLG("AP:"+ap + " IP:" + ip);
  
}

WiFiClient espClient;

/*
 * 
 * Config 
 * 
 */
 
bool shouldSaveConfig = false;
void saveConfigCallback () {
  DLG("Should save config");
  shouldSaveConfig = true;  
}

String readConfigValue(int addr) {
    String str = String();
    int address = addr;
    while(address < 512) {
      char c = char(EEPROM.read(address));
      if (c != 0 && c != 255) {
        str = str + c;
      } else {
        break;  
      }
      address ++;
    }  
    return str;
}

void writeConfigValue(int addr, String str) {
  for(int i=0; i<str.length(); i++) {
    EEPROM.write(addr+i,str[i]);
  }
  EEPROM.write(addr+str.length(),0);
  EEPROM.commit();
}


/*
 * 
 * MQTT 
 * 
 */

char mqtt_server[40] = "io.adafruit.com";
const int ADDR_mqtt_server = 200;

char mqtt_port[6] = "1883";
const int ADDR_mqtt_port = 250;

char mqtt_user[40] = "user";
const int ADDR_mqtt_user = 300;

char mqtt_token[40] = "token_or_key";
const int ADDR_mqtt_token = 350;

char mqtt_topic[110] = "topic/feed/name/";
const int ADDR_mqtt_topic = 400;

PubSubClient mqtt(espClient);

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  DLG("MQTT : callback to " + String(topic) );
}

bool mqttConnect(int retries) {

  if(WiFi.status() != WL_CONNECTED) {
    DLG("MQTT : Skip - No Wi-Fi");
    return false;
  }

  if (mqtt.connected()) {
    
    //DLG("MQTT : Connected");
    return true;
    
  } else {

    DLG("MQTT : Connect , WIFI : " + String(WiFi.status()) );

    led.Blink(20,200).Forever();
    
    String serv = String(mqtt_server);
    
    int port = strtol(mqtt_port, NULL, 10);

    mqtt.setCallback(mqttCallback);
    mqtt.setServer(serv.c_str(), port);
  
    DLG("MQTT : Connecting to :" + serv + ":" + String(port));
    
    int attempt = 0;
    
    while (!mqtt.connected() && attempt < retries ) {
      
      // Create a random client ID
      String clientId = "CO2_Meter_";
      clientId += String(random(0xffffff), HEX); 

      String username = String(mqtt_user);
      String key = String(mqtt_token);
      
      DLG("MQTT : Attempting connection using id:" + clientId + " username:" + username + " key:" + key);
      
      // Attempt to connect
      if (mqtt.connect(clientId.c_str() ,username.c_str() , key.c_str() ) ) {
        DLG("MQTT : Connected : " + String(mqtt.state()) + " with retry " + String(attempt) );
      } else {
        attempt++;
        DLG("MQTT : Not connected :"+ String(mqtt.state()) +" try to reconnect " + String(attempt) );
        delay(100);
      }
    }
  }

  return mqtt.connected();
}

void mqttPost(const char * stream, int value, int retries) {

    if (mqttConnect(retries)) {
      String topic = String(mqtt_topic) + String(stream);
      String payload = String(value);
      bool ok = mqtt.publish(topic.c_str(), payload.c_str()); 
      DLG("MQTT : Publish "+ topic + " | " + payload + "  OK:" + String(ok));
    }
  }

/*
 * 
 * Routine Timer
 * 
 */
os_timer_t myTimer;
 
void timerCallback(void *pArg) {
  led.Update();
} // End of timerCallback

void timerInit(void) {
      os_timer_setfn(&myTimer, timerCallback, NULL);
      os_timer_arm(&myTimer, 20, true);
} // End of timerInit


/*
 * 
 * SETUP 
 * 
 */
void setup() {
  Serial.begin(115200);
  analogWriteRange(255);
  timerInit();

  display.init();
  display.resetDisplay();
  //display.flipScreenVertically();
  display.clear();

  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 0, "CO2 Meter initializing...");
  display.display();
  
  DLG("CO2: meter setup");
  pinMode(LED_BUILTIN, OUTPUT);

  led.On();
  pinMode(HD_PIN, INPUT_PULLUP);
  
  bool goConfig = false;  //will go config portal in case of Flash pressed in first 1.5 second
  for (int i=0; i<15; i++) {
    delay(100);
    if (digitalRead(HD_PIN)==LOW) {
      goConfig = true;
      DLG("Wifi will go to config portal");
      break;
    }
  }
  
  bool goOTA = true; //will start OTA in case of Flash pressed and hold for 3 sec 
  led.Blink(10,500).Forever();  
  for (int k=0; k<40; k++) {
    delay(100);
    if (digitalRead(HD_PIN)==HIGH) {
      goOTA = false;
      break;
    }
  }
  
  led.Off();
  
  if (goOTA) {
      led.On();
      goConfig = false;
      DLG("Wifi start OTA");
  }

  DLG("Read config");

  EEPROM.begin(512);  
  
  String saved_mqtt_server = readConfigValue(ADDR_mqtt_server);
  String saved_mqtt_port = readConfigValue(ADDR_mqtt_port);
  String saved_mqtt_user = readConfigValue(ADDR_mqtt_user);
  String saved_mqtt_token = readConfigValue(ADDR_mqtt_token);
  String saved_mqtt_topic = readConfigValue(ADDR_mqtt_topic);

  DLG("MQTT : Serv:" + saved_mqtt_server + ":" + saved_mqtt_port 
  + " username:" + saved_mqtt_user + " key:" + saved_mqtt_token + " topic:" + saved_mqtt_topic);

  if (saved_mqtt_server.length()) {
    strcpy(mqtt_server, saved_mqtt_server.c_str());
  }

  if (saved_mqtt_port.length()) {
    strcpy(mqtt_port, saved_mqtt_port.c_str());  
  }
  
  if (saved_mqtt_user.length()) {
    strcpy(mqtt_user, saved_mqtt_user.c_str());
  }

  if (saved_mqtt_token.length()) {
    strcpy(mqtt_token, saved_mqtt_token.c_str());
  }
  
  if (saved_mqtt_topic.length()) {
    strcpy(mqtt_topic, saved_mqtt_topic.c_str());
  }
  
  
  //add wifimanager
  WiFiManager wifiManager;
  //add all your parameters here
  WiFiManagerParameter custom_mqtt_server("mqtt_server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("mqtt_port", "mqtt port", mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_user("mqtt_user", "user", mqtt_user, 40);  
  WiFiManagerParameter custom_mqtt_token("mqtt_token", "token_or_key", mqtt_token, 40);  
  WiFiManagerParameter custom_mqtt_topic("mqtt_topic", "topic/feed/name/", mqtt_topic, 110);
  
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);  
  wifiManager.addParameter(&custom_mqtt_token);
  wifiManager.addParameter(&custom_mqtt_topic);  
  
  wifiManager.setMinimumSignalQuality(10);
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setConfigPortalTimeout(60);

  led.Blink(10,100).Forever();
  
  if (goConfig) {
    DLG("Wifi reset and go config");
    //wifiManager.resetSettings();
    wifiManager.startConfigPortal("CO2 Meter");
  } else {
    DLG("Wifi auto connect");
    display.drawString(0, 10, "Connecting to Wi-Fi...");
    display.display();
    wifiManager.autoConnect("CO2 Meter");
  }


  //save Config Portal Settings
  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_token, custom_mqtt_token.getValue());
  strcpy(mqtt_topic, custom_mqtt_topic.getValue());
  
  //save the custom parameters to FS
  if (shouldSaveConfig) {
    DLG("saving config");
    
    String new_mqtt_server = String(mqtt_server);
    String new_mqtt_port   = String(mqtt_port);
    String new_mqtt_user   = String(mqtt_user);
    String new_mqtt_token  = String(mqtt_token);
    String new_mqtt_topic  = String(mqtt_topic);

    if (new_mqtt_server.length()) {
        writeConfigValue(ADDR_mqtt_server, new_mqtt_server);
    }
    if (new_mqtt_port.length()) {
        writeConfigValue(ADDR_mqtt_port, new_mqtt_port);
    }
    if (new_mqtt_user.length()) {
        writeConfigValue(ADDR_mqtt_user, new_mqtt_user);
    }
    if (new_mqtt_token.length()) {
        writeConfigValue(ADDR_mqtt_token, new_mqtt_token);
    }
    if (new_mqtt_topic.length()) {
        writeConfigValue(ADDR_mqtt_topic, new_mqtt_topic);
    }

    //save
  }

  display.clear();
  display.display();
  DLG("Wifi Connected ? ");
  String msg = " + IP: " + WiFi.localIP().toString() + " ";
  DLG(msg);

  if(WiFi.status() == WL_CONNECTED) {

      DLG("Wifi Connected !");
      
      display.drawString(0, 0, "Wi-Fi conected");
      display.drawString(0, 10, msg);
      display.display();

      if (goOTA) {
        //starting OTA
        delay(100);
        display.drawString(0, 20, "Starting OTA ...");
        display.display();
        delay(500);
        setupOTA();
      } else {

        //connect MQTT
        delay(100);
        display.drawString(0, 20, "Connecting to MQTT server");
        display.display();
        delay(100);
        
        bool ok = mqttConnect(5);

        if (ok) {
          display.drawString(0, 30, " + connected");
        } else {
          display.drawString(0, 30, " - fail");
        }
        
        display.display();
        delay(1000);
    
      }

      
    } else {

      DLG("Wifi offline mode !");
      //offline mode
      WiFi.mode(WIFI_OFF);
      
      delay(100);
      display.drawString(0, 0, "Wi-Fi NOT Conected");
      display.display();
      delay(1000);
      display.drawString(0, 10, "Starting Offile mode");
      display.display();
      delay(100);
      display.drawString(0, 20, "     .      ");
      display.display();
      delay(100);
      display.drawString(0, 20, "     ..     ");
      display.display();
      delay(100);
      display.drawString(0, 20, "     ...    ");
      display.display();
      delay(100);
      
    }

  mySerial.begin(9600);
  dht.setup(DHT_PIN, DHTesp::DHT11);
  
  delay(1000);
  
  display.clear();
  display.display();

// initial setup values
  
  memset(ppms, 0, PPMS_L);
  
  DLG("RUN !");
  
}


void displayDrawIcons() {

  //anten
  display.fillRect(111, 0, 1, 7);
  display.fillRect(108, 0, 7, 1);  
  display.fillRect(109, 1, 5, 1);  
  display.fillRect(110, 2, 3, 1);
  
  if (WiFi.status() == WL_CONNECTED) { //connected wi-fi
    //palki
    display.fillRect(119, 1, 1, 6);
    display.fillRect(117, 3, 1, 4);
    display.fillRect(115, 5, 1, 2);

    if (mqtt.connected()) { //mqtt
        display.fillRect(124, 4, 3, 1);
        display.fillRect(125, 3, 1, 3);        
    } else {
        display.fillRect(124, 4, 3, 1);
    }
  } else {
    display.setPixel(117, 4); 
    display.setPixel(116, 5);
    display.setPixel(116, 3);    
    display.setPixel(118, 5);
    display.setPixel(118, 3);    

    display.setPixel(125, 4);
  }

}

void displayPrintData() {
    // draw on display

  
  String co2level = "CO  level:" + String(ppm) + " ";
  String co2levelAvg = "Avg:" + String(avg_ppm) + " ppm ";
  String templevel = "" + String(dht_temp) + "ºC " + String(dht_hum) + "%.";
      
  display.setTextAlignment(TEXT_ALIGN_LEFT);  
  
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 2, co2level);
  display.setFont(ArialMT_Plain_10);
  display.drawString(24, 8, "2");
  display.drawString(0, 20, co2levelAvg);
  display.setTextAlignment(TEXT_ALIGN_RIGHT);  
  display.drawString(128, 20, templevel);
  display.drawString(128, 7, "ppm");
  display.drawString(128, 32, "2");
  display.drawString(128, 42, "1");
  display.drawString(128, 52, "0");
  //draw graph
  
  for (int x = 0; x <= 120; x++) {
 
       if (x%30 == 0) {
          display.setPixel(x, 63);          
       }
       if (x%5 == 0) {
          display.setPixel(x, 62);
       }
       
  }

  display.fillRect(0, 61, 121, 1);
  display.fillRect(0, 51, 121, 1);
  display.fillRect(0, 41, 121, 1);  

  display.setColor(INVERSE);
  for (int i = 0; i<PPMS_L ; i++) {
    unsigned int l_ppm = ppms[i];
    int y = (int)((int)l_ppm)/100;
    if (l_ppm != 0) {
      display.fillRect(120-i, 61-y, 1, y);
    } else {
      display.setPixel(0, 61); 
    }
  }

}

void displayRefresh()  {

  display.clear();
  display.setColor(WHITE);

  displayDrawIcons();
  displayPrintData();
  
  display.display();

}

void measureDataMHZ() {
  
  MHZ19_RESULT response = mhz.retrieveData();
  
  if (response == MHZ19_RESULT_OK) {
    ppm = mhz.getCO2();
    temp = mhz.getTemperature();
    acc = mhz.getAccuracy();
  } else {
    ppm = 0;
    temp = 0;
    acc = -1;
  }
  
  if (avg_ppm == 0 && ppm !=0) {
    avg_ppm = ppm;
  }


  avg_ppm_summ = avg_ppm_summ + ppm;
  avg_measures ++;

  String msg = ">> CO2 level:" + String(ppm) + " \t AVG:" + String(avg_ppm) + "\t temp:" + String(temp) +"\t acc:"+String(acc);
  DLG(msg);
}

void measureDataDHT() {

  float humidity = dht.getHumidity();
  float temperature = dht.getTemperature();

  if (dht.getStatus() == 0) {

    dht_temp = (int) temperature;
    dht_hum = (int) humidity;
  }

  String msg = ">> DHT: " + String(dht.getStatusString()) + "\t temp:" + String(temperature) + "\t humn:" +  String(humidity);
  DLG(msg);
  
}

  
void postDataToMQTT() {
    
  for (int i = 0 ; i<10 ; i++) {
      mqtt.loop();
      delay(5);
  }
  mqttPost("co2ppm", ppm, 3);

  for (int i = 0 ; i<10 ; i++) {
      mqtt.loop();
      delay(5);
  }
  mqttPost("co2temperature", temp, 0);
  
  for (int i = 0 ; i<10 ; i++) {
      mqtt.loop();
      delay(5);
  }
  mqttPost("co2accuracy", acc, 0);
  
  for (int i = 0 ; i<10 ; i++) {
      mqtt.loop();
      delay(5);
  }

  mqttPost("temperature", dht_temp, 0);
  
  for (int i = 0 ; i<10 ; i++) {
      mqtt.loop();
      delay(5);
  }

  mqttPost("humidity", dht_hum, 0);
  
  for (int i = 0 ; i<10 ; i++) {
      mqtt.loop();
      delay(5);
  }

  
}

// the loop function runs over and over again forever
void loop() {

  //TODO : IRQ
  unsigned int before_loop_time = millis();  

  if (OTA) {
      ArduinoOTA.handle();
      
      display.clear();
      display.setFont(ArialMT_Plain_10);
      display.setTextAlignment(TEXT_ALIGN_LEFT);
      display.drawString(10, 10, "CO2 Meter OTA...");
      String ip = " IP: " + WiFi.localIP().toString();
      display.drawString(10, 20, ip);
      display.display();

      return;
  }


  measureDataMHZ();
  measureDataDHT();

  displayRefresh();
    
  //status led
  int bval = 5000 - min(avg_ppm*2, 4400);
  led.Breathe(bval).Forever();

  postDataToMQTT();

  //time measure
  unsigned int cur_measure_time = millis();
  unsigned int measure_time_diff = 0;
  if (last_measured_time == 0 || cur_measure_time < last_measured_time) {
    measure_time_diff = measure_millis;//should be near this value
  } else {
    measure_time_diff = cur_measure_time - last_measured_time;
  }
  last_measured_time = cur_measure_time;
  measured_time = measured_time + measure_time_diff;
  
  if (measured_time > measure_period) { 
    //DLG("period");
    measured_time = measured_time - measure_period;
    avg_ppm = avg_ppm_summ / avg_measures;
    //write avg to buffer;
    for (int i = PPMS_L-1 ; i>0 ; i--) {
       ppms[i] = ppms[i-1];
    }
    ppms[0] = avg_ppm;

    avg_ppm_summ = 0;
    avg_measures = 0;
    
  } else {
    
    unsigned int last_avg_ppm = ppms[0];
    if (last_avg_ppm) {
      avg_ppm = last_avg_ppm;
    } else {
      avg_ppm = avg_ppm_summ / avg_measures;    
    }

  }


  //timings 
  unsigned int diff = 0;
  unsigned int cur_loop_time = millis();
  
  if (cur_loop_time > before_loop_time) {
    // OK
    diff = cur_loop_time - before_loop_time;
  } else {
    //overflow what to do??? should be near to average measure time;
    diff = 1190; //average time for measurment 
  }

  //DLG(millis());
  //DLG(diff);    
  delay(measure_millis-diff);
  //DLG(millis());
  
}
