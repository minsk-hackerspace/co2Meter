
extern "C" { //Это нужно для работы таймера
#include "user_interface.h"
}


#include <SoftwareSerial.h> //Программный UART для MH-Z19

const int TX1_PIN = D4; //serial1

const int HD_PIN = D3; //A0
const int SOFTTX_PIN = 12; //tx  12
const int SOFTRX_PIN = 14; //rx  14


SoftwareSerial mySerial(SOFTRX_PIN, SOFTTX_PIN);

unsigned char response[9]; //Сюда пишем ответ MH-Z19
byte cmd[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79}; // команда запроса данных у MH-Z19
unsigned int ppm = 0; //Текущее значение уровня СО2

LEDDisplay *led;

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600);
  mySerial.begin(9600);

  Serial.println("CO2: meter setup");
  Serial1.println("CO2: meter setup");
  
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(HD_PIN, OUTPUT);
  
  /*
  digitalWrite(HD_PIN, LOW);
  
  Serial.println("Calibration");
  
  for (int i = 0; i<10; i++) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(500);
      digitalWrite(LED_BUILTIN, LOW);
      delay(500);
      Serial.println("...");
  }
  digitalWrite(HD_PIN, HIGH);
  */
  Serial.println("Calibration Done");
  
}

// the loop function runs over and over again forever
void loop() {
  
  delay(2000);
  
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
  
    Serial.println("CRC error: " + String(crc) + " / " + String(response[8]));
    ESP.restart();

  }
  unsigned int responseHigh = (unsigned int) response[2];
  unsigned int responseLow = (unsigned int) response[3];

  ppm = (256 * responseHigh) + responseLow;
  Serial.println("CO2: " + String(ppm) + " ppm\t"); //Выводим данные на UART для отладки
  Serial1.println("CO2: " + String(ppm) + " ppm\t");
  
}
