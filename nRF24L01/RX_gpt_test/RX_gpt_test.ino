#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

RF24 radio(9, 10); // создаем объект радио с указанием пинов CE и CSN
const byte address[6] = "00001"; // адрес для приема

void setup() {
  Serial.begin(115200);
  radio.begin(); // инициализируем радио
  radio.openReadingPipe(0, address); // устанавливаем адрес для приема
  radio.setChannel(0x6a);
  radio.startListening(); // переводим радио в режим приема
}

void loop() {
  if (radio.available()) { // если есть данные для приема
    // получаем число
    int number;
    radio.read(&number, sizeof(number));
    Serial.print("Received number: ");
    Serial.println(number);

    // получаем символ
    char character;
    radio.read(&character, sizeof(character));
    Serial.print("Received character: ");
    Serial.println(character);

    // получаем текст
    char charMessage[32];
    radio.read(&charMessage, sizeof(charMessage));
    String message = "";
    for(int i = 0; i < sizeof(charMessage); i++){
      if(charMessage[i] != '\0') message += charMessage[i];
    }
    Serial.print("Received message: ");
    Serial.println(message);
  }
}
