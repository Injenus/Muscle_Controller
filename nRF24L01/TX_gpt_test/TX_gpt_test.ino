#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

RF24 radio(9, 10); // создаем объект радио с указанием пинов CE и CSN
const byte address[6] = "00001"; // адрес для передачи

void setup() {
  Serial.begin(115200);
  radio.begin(); // инициализируем радио
  radio.openWritingPipe(address); // устанавливаем адрес для передачи
  radio.setChannel(0x6a);
}

void loop() {
  int number = 12345; // число для передачи
  char character = 'A'; // символ для передачи
  String message = "Hello world!"; // текст для передачи

  radio.write(&number, sizeof(number)); // передаем число
  Serial.print("Sent number: ");
  Serial.println(number);

  radio.write(reinterpret_cast<const uint8_t*>(&character), sizeof(character)); // передаем символ
  Serial.print("Sent character: ");
  Serial.println(character);

  char charMessage[32];
  message.toCharArray(charMessage, sizeof(charMessage)); // преобразуем текст в массив символов
  radio.write(reinterpret_cast<const uint8_t*>(&charMessage), sizeof(charMessage)); // передаем текст
  Serial.print("Sent message: ");
  Serial.println(message);

  delay(1000); // задержка между передачами
}
