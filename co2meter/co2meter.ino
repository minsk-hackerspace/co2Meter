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
#include <FS.h>
#include <SoftwareSerial.h> //Arduino lib SoftwareUART for MH-Z19

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

#include <jled.h> //https://github.com/jandelgado/jled

#include <WiFiManager.h> https://github.com/tzapu/WiFiManager
#include <DNSServer.h> //required for WiFiManager
#include <ESP8266WebServer.h> //required for WiFiManager

#include <PubSubClient.h> // https://github.com/knolleary/pubsubclient


// comment for disable logging
#define DEBUG

#ifdef DEBUG
 #define DLG(x)  Serial.println(x);Serial1.println(x);
#else
 #define DLG(x)
#endif

const int TX1_PIN = D4; //serial1

const int HD_PIN = D3; //A0
const int SOFTTX_PIN = 12; //tx  12
const int SOFTRX_PIN = 14; //rx  14


SoftwareSerial mySerial(SOFTRX_PIN, SOFTTX_PIN);

unsigned char response[9]; //Сюда пишем ответ MH-Z19
byte cmd[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79}; // команда запроса данных у MH-Z19
unsigned int ppm = 0; //Текущее значение уровня СО2


/*
 * 
 * SATUS LED
 * 
 */
 
os_timer_t myTimer;
JLed led = JLed(LED_BUILTIN).LowActive();

void timerCallback(void *pArg) {
  led.Update();
} // End of timerCallback

void timerInit(void) {
      os_timer_setfn(&myTimer, timerCallback, NULL);
      os_timer_arm(&myTimer, 20, true);
} // End of timerInit


void reboot(void) {
  led.Blink(10,50).Forever();
  DLG("Restarting...");
  delay(2000);
  ESP.restart();
}

/*
 * 
 * WiFiManager 
 * 
 */

void configModeCallback (WiFiManager *wifiManager) {
  DLG("Entered config mode");
  led.Blink(200,20).Forever();  
  DLG(WiFi.softAPIP());
  DLG(wifiManager->getConfigPortalSSID());
}
bool shouldSaveConfig = false;
void saveConfigCallback () {
  DLG("Should save config");
  shouldSaveConfig = true;  
}

/*
 * 
 * MQTT 
 * 
 */

char mqtt_server[40] = "178.172.172.103";
char mqtt_port[6] = "1883";
char mqtt_token[34] = "TOKEN";


WiFiClient espClient;
PubSubClient mqtt(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;


void mqtt_reconnect() {
  // Loop until we're reconnected
  int retries = 0;
  while (!mqtt.connected()) {
    DLG("MQTT : Attempting connection...");
    
    // Create a random client ID
    String clientId = "CO2Meter";
    
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (mqtt.connect(clientId.c_str())) {
      DLG("MQTT : Connected");
      mqtt.publish("co2/value", "0");
    } else {
      DLG("MQTT : "+ String(mqtt.state()) +" try to reconnect...");
      delay(3000);
      retries++;
    }

    if (retries == 10) {
      reboot();
    }
  }
}



/*
 * 
 * SETUP 
 * 
 */
void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);
  mySerial.begin(9600);
  analogWriteRange(255);
  timerInit();
  
  DLG("CO2: meter setup");
  pinMode(LED_BUILTIN, OUTPUT);
  led.Breathe(300).Forever();
  
  pinMode(HD_PIN, INPUT_PULLUP);
  delay(1200);
  bool goConfig = (digitalRead(HD_PIN)==LOW); //will go config portal in case of Flash pressed in first second
  pinMode(HD_PIN, OUTPUT);
  digitalWrite(HD_PIN, LOW);
  
  //add wifimanager
  WiFiManager wifiManager;
  //add all your parameters here
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_token("token", "mqtt token", mqtt_token, 34);
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_token);
  wifiManager.setMinimumSignalQuality(10);
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setConfigPortalTimeout(120);

  led.Blink(20,200).Forever();
  if (goConfig) {
    DLG("Wifi reset and go config");
    wifiManager.resetSettings();
    wifiManager.startConfigPortal("CO2 Meter");
  } else {
    DLG("Wifi auto connect");
    wifiManager.autoConnect("CO2 Meter");
  }
  
  DLG("Wifi Connected local IP:"+ WiFi.localIP());
  
  
  //save MQTT
  led.Blink(200,200).Forever();
  
  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_token, custom_mqtt_token.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    DLG("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["mqtt_token"] = mqtt_token;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      DLG("failed to open config file for writing");
    }

    json.printTo(Serial1);
    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  delay(1000);

  
  //connect mqtt
  //TODO:
  
  delay(1000);
  digitalWrite(HD_PIN, HIGH);
  led.Breathe(3000).Forever();
  DLG("Calibration Done");
  
  
}

// the loop function runs over and over again forever
void loop() {
  
  delay(10000);
  
  mySerial.write(cmd, 9); //Запрашиваем данные у MH-Z19
  memset(response, 0, 9); //Чистим переменную от предыдущих значений
  mySerial.readBytes(response, 9); //Записываем свежий ответ от MH-Z19
  
  unsigned int i;
  byte crc = 0;//Ниже магия контрольной суммы
  for (i = 1; i < 8; i++) crc += response[i];
  crc = 255 - crc;
  crc++;
  
  //Проверяем контрольную сумму и если она не сходится - перезагружаем модуль
  
  if ( !(response[0] == 0xFF && response[1] == 0x86 && response[8] == crc) ) {
  
    DLG("CRC error: " + String(crc) + " / " + String(response[8]));
    reboot();

  }
  unsigned int responseHigh = (unsigned int) response[2];
  unsigned int responseLow = (unsigned int) response[3];

  ppm = (256 * responseHigh) + responseLow;
  DLG("CO2: " + String(ppm) + " ppm\t");
  
}
