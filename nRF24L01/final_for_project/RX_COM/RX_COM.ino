#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"

RF24 radio(9, 10);  // "создать" модуль на пинах 9 и 10 Для Уно
byte address[][6] = {"1Node", "2Node", "3Node", "4Node", "5Node", "6Node"}; //возможные номера труб
uint8_t  pipe;

String first_data, second_data, rec_data;
bool isFirst, isSecond;

void setup() {
  Serial.begin(1000000);         // открываем порт для связи с ПК
  Serial.println("wedfg");
  radio.begin();              // активировать модуль
  radio.setAutoAck(0);        // режим подтверждения приёма, 1 вкл 0 выкл
  radio.setRetries(0, 15);    // (время между попыткой достучаться, число попыток)
  radio.enableAckPayload();   // разрешить отсылку данных в ответ на входящий сигнал
  radio.setPayloadSize(32);   // размер пакета, в байтах

  radio.openReadingPipe(1, address[0]);   // хотим слушать трубу 0
  radio.openWritingPipe(address[1]); // теперь мы - труба 
  radio.setChannel(0x6a);     // выбираем канал (в котором нет шумов!)
  radio.setPALevel (RF24_PA_MAX);   // уровень мощности передатчика. На выбор RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH, RF24_PA_MAX
  radio.setDataRate (RF24_2MBPS); // скорость обмена. На выбор RF24_2MBPS, RF24_1MBPS, RF24_250KBPS
  //должна быть одинакова на приёмнике и передатчике!
  //при самой низкой скорости имеем самую высокую чувствительность и дальность!!

  radio.powerUp();        // начать работу
  radio.startListening(); // начинаем слушать эфир, мы приёмный модуль

  isFirst = false;
  isSecond = false;
}

void loop() {
  from_EMG_to_COM();
  check_Serial();
}

String receive_data() {
  String receive_data;
  if (radio.available(&pipe)) { // Если есть данные для приема, узнаём от кого
    char charBuf[32]; // Буфер для хранения принятых данных.
    radio.read(&charBuf, sizeof(charBuf)); // Чтение данных из радиоканала.
    receive_data = String(charBuf); // Преобразование принятых данных в строку.
    //    Serial.print(pipe);
    //    Serial.print(": ");
    //    Serial.println("Data received: " + receive_data);

  }
  else {
    receive_data = "NONE";
  }
  return receive_data;
}

void from_EMG_to_COM() {
  rec_data = receive_data();
  if (rec_data != "NONE") {
    if (rec_data[rec_data.length() - 1] == '%') {
      first_data = rec_data;
      isFirst = true;
    }
    else if (rec_data[0] == '%') {
      second_data = rec_data;
      isSecond = true;
    }
    else if (rec_data[0] == '#') {
      Serial.println(rec_data);
      isFirst = false;
      isSecond = false;
    }
    else if (rec_data[0] == '$') {
      Serial.println(rec_data);
      isFirst = false;
      isSecond = false;
    }
    else {
      Serial.println("RADIO_DATA: " + rec_data);
      //Serial.println(rec_data);
    }
  }
  if (isFirst && isSecond) {
    Serial.println(first_data.substring(0, first_data.length() - 1) + second_data.substring(1));
    isFirst = false;
    isSecond = false;
  }
}
void check_Serial() {
  if (Serial.available()) {
    int readSerial = Serial.read() - '0'; //превращаем символ в число
    radio.stopListening();
    //    radio.openWritingPipe(address[0]); // теперь мы - труба 0
    radio.write(&readSerial, sizeof(readSerial));
    //Serial.println('s');
    //radio.openReadingPipe(1, address[0]); //мы снова слушаем трубу 0
    radio.startListening();
  }
}
