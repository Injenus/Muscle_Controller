/*
  Протокол отправки:
  данные с датчиков: $<data0>,<data1>,<data2>..<dataN>;
  данные состояния устройства: #<строка_символов>;

  Для управления микроконтроллером отправляются однозначные числа 3..7

  Частота отправки данных не менее 400 Гц
*/
#include <SPI.h>          // библиотека для работы с шиной SPI
#include "nRF24L01.h"     // библиотека радиомодуля
#include "RF24.h"         // ещё библиотека радиомодуля

#include <EEPROM.h>
#include "Wire.h"
#include "Print.h"
#include "MPU6050.h"
#include "mString.h"


#define sendPeriod 1024 // фактический период на 4-12 мкс больше (разрешение таймера 4мкс)
// период больше на ~175мкс
//c 1024 получается 1984..2016 мкс 
#define blinkPeriodSlow 500000 // период работы в два раза больше
#define blinkPeriodFast 10000

#define START_BYTE 1010 //для записи калибровки в еепром
#define BUFFER_SIZE 100

#define pinExtensorSig 16  // A2
#define pinExtensorRaw 20  // A6
#define pinFlexorSig 17  // A3
#define pinFlexorRaw 15  // A1
#define pinIndicator 13   // D13
#define pinBtn 4 // D4

int16_t extensorSig; // переменные для значений ЭМГ
int16_t extensorRaw;
int16_t flexorSig;
int16_t flexorRaw;

RF24 radio(9, 10); // "создать" модуль на пинах 9 и 10 Для Уно
byte address[][6] = {"1Node", "2Node", "3Node", "4Node", "5Node", "6Node"}; //возможные номера труб
String data_str;
byte gotByte, readData;

/*const int MPU_addr = 0x68; // адрес датчика PU-6050
  int16_t mpuData[7]; // [accX, accY, accZ, temp, gyrX, gyrY, gyrZ]*/
MPU6050 mpu;  // SCL to A5, SDA to A4
int16_t ax, ay, az, gx, gy, gz;
long offsets[6]; // для оффсетов для калибровки

bool flagRecord = false;
bool stateIndicator = false;
bool isMpuOk = false;

uint32_t initTime = 0;
uint32_t pauseTime = 0;
uint32_t deltaTime = 0;
uint32_t currentSendTime = 0;
uint32_t prevSendTime = 0;
uint32_t blinkTime = 0;

int16_t toSend [6]; // массив данных для отправки: extenSig, flexSig, extenRaw, flexRaw, accX, accY, accZ, deltaTime(us)

void setup() {
  //иниц радио-модуля
  radio.begin();              // активировать модуль
  radio.setAutoAck(0);        // режим подтверждения приёма, 1 вкл 0 выкл
  radio.setRetries(15, 15);    // (время между попыткой достучаться, число попыток)
  radio.enableAckPayload();   // разрешить отсылку данных в ответ на входящий сигнал
  radio.setPayloadSize(32);   // размер пакета, в байтах

  radio.openWritingPipe(address[0]);  // мы - труба 0, открываем канал для передачи данных
  radio.openReadingPipe(1, address[1]);   // хотим слушать трубу 1
  radio.setChannel(0x6a);             // выбираем канал (в котором нет шумов!)

  radio.setPALevel (RF24_PA_MAX);   // уровень мощности передатчика. На выбор RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH, RF24_PA_MAX
  radio.setDataRate (RF24_2MBPS); // скорость обмена. На выбор RF24_2MBPS, RF24_1MBPS, RF24_250KBPS
  //должна быть одинакова на приёмнике и передатчике!
  //при самой низкой скорости имеем самую высокую чувствительность и дальность!!
  radio.powerUp();        // начать работу
  radio.stopListening();  // не слушаем радиоэфир, мы передатчик

  //иниц UART
  Serial.begin(1000000);         // открываем порт для связи с ПК
  Serial.setTimeout(50);
  Serial.flush(); //очищаем буфер
  digitalWriteFast(pinIndicator, HIGH);
  delay(300);
  digitalWriteFast(pinIndicator, LOW);

  // инициализация для MPU-6050
  Wire.begin();
  Wire.setClock(400000);  // скорость I2C в Гц (бит/с)
  /*Wire.beginTransmission(MPU_addr);
    Wire.write(0x6B);  // PWR_MGMT_1 register
    Wire.write(0);     // set to zero (wakes up the MPU-6050)
    Wire.endTransmission(true);*/
  mpu.initialize();
  EEPROM.get(START_BYTE, offsets);//вспоминаем оффсеты
  // ставим оффсеты из памяти
  mpu.setXAccelOffset(offsets[0]);
  mpu.setYAccelOffset(offsets[1]);
  mpu.setZAccelOffset(offsets[2]);
  mpu.setXGyroOffset(offsets[3]);
  mpu.setYGyroOffset(offsets[4]);
  mpu.setZGyroOffset(offsets[5]);
  /*Serial.print(mpu.getXAccelOffset()); Serial.print(", ");
    Serial.print(mpu.getYAccelOffset()); Serial.print(", ");
    Serial.print(mpu.getZAccelOffset()); Serial.print(", ");
    Serial.print(mpu.getXGyroOffset()); Serial.print(", ");
    Serial.print(mpu.getYGyroOffset()); Serial.print(", ");
    Serial.print(mpu.getZGyroOffset()); Serial.println(" ");
    Serial.println(" ");*/

  // иницализация пинов
  pinMode(pinExtensorSig, INPUT);
  pinMode(pinExtensorRaw, INPUT);
  pinMode(pinFlexorSig, INPUT);
  pinMode(pinFlexorRaw, INPUT);
  pinMode(pinBtn, INPUT_PULLUP);

  if (mpu.testConnection()) {
    mpu.getAcceleration(&ax, &ay, &az);
    if (ax == 0 && ay == 0 && az == 0) {
      data_str = F("#ERROR DATA: CHECK MPU6050;");
      send_data();
      //Serial.println("#ERROR DATA: CHECK MPU6050;");
      isMpuOk = false;
    }
    else {
      data_str = F("#All SYSTEMS ONLINE;");
      send_data();
      //Serial.println("#All SYSTEMS ONLINE;");
      isMpuOk = true;
    }
  }
  else {
    data_str = F("#ERROR CONNECTION: CHECK MPU6050;");
    send_data();
    //Serial.println("#ERROR CONNECTION: CHECK MPU6050;");
    isMpuOk = false;
  }
}

void loop() {
  while (!digitalRead(pinBtn)) {
    //    radio.openReadingPipe(1, address[0]);   // хотим слушать трубу 0
    radio.startListening(); // начинаем слушать эфир, мы приёмный модуль
    byte pipe;
    if (radio.available(&pipe)) {
      radio.read(&gotByte, sizeof(gotByte));  // чиатем входящий сигнал
      if (gotByte < 10) {
        readData = gotByte;
        radio.stopListening();
        //Serial.print("Recieved: ");
        //Serial.println(gotByte);
        if (isMpuOk) {
          switch (readData) {
            case 3: //3 51
              for (byte i = 0; i < 3; i++) {
                digitalWriteFast(pinIndicator, 1);
                delay(40);
                digitalWriteFast(pinIndicator, 0);
                delay(20);
              }
              //Serial.println("#READY;");
              //radio.openWritingPipe(address[0]);
              radio.stopListening();
              data_str = F("#READY;");
              delay(10);
              send_data();
              break;
            case 4: //4 52
              flagRecord = true;
              initTime = micros();
              currentSendTime = initTime;
              prevSendTime = currentSendTime;
              deltaTime = 0;

              //Serial.println("#START;");
              //radio.openWritingPipe(address[0]);
              radio.stopListening();
              data_str = F("#START;");
              delay(10);
              send_data();

              break;
            case 6: //6 54
              flagRecord = true;
              deltaTime = deltaTime + micros() - pauseTime;
              //Serial.println("#CONTINUE;");
              //radio.openWritingPipe(address[0]);
              radio.stopListening();
              data_str = F("#CONTINUE;");
              delay(10);
              send_data();
              break;
            case 5: //5 53
              flagRecord = false;
              pauseTime = micros();
              digitalWriteFast(pinIndicator, 1);
              //Serial.println("#PAUSE;");
              //radio.openWritingPipe(address[0]);
              radio.stopListening();
              data_str = F("#PAUSE;");
              delay(10);
              send_data();
              break;
            case 7: //7 55
              flagRecord = false;
              digitalWriteFast(pinIndicator, 0);
              //Serial.println("#STOP;");
              //radio.openWritingPipe(address[0]);
              radio.stopListening();
              data_str = F("#STOP;");
              delay(10);
              send_data();
              break;
            case 8: //8 56
              if (!flagRecord) {
                calibration(); // функция калибровки
              }
              break;
          }
        }
        else {
          blinkIndicator(blinkPeriodFast >> 2);
        }
      }
    }
  }
  radio.stopListening();


  if (micros() - currentSendTime >= sendPeriod) {
    currentSendTime = micros();
    toSend[5] = currentSendTime - prevSendTime;
    if (toSend[5] > 9999 or toSend[5] < -999) {
      toSend[5] = 0;
    }
    prevSendTime = currentSendTime;

    data_str = F("");

    extensorSig = analogReadFast(pinExtensorSig - 14); //нумерация с А0 (или с 14);
    toSend[0] = extensorSig;
    flexorSig = analogReadFast(pinFlexorSig - 14);
    toSend[1] = flexorSig;

    //    extensorRaw = analogReadFast(pinExtensorRaw - 14);
    //    toSend[6] = extensorRaw;
    //    flexorRaw = analogReadFast(pinFlexorRaw - 14);
    //    toSend[7] = flexorRaw;

    /*getMpuData();
      for (byte i = 0; i < 3; i++) {
      toSend[i + 4] = mpuData[i];
      }*/
    mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
    toSend[2] = map(ax, -32768, 32767, -999, 999);
    toSend[3] = map(ay, -32768, 32767, -999, 999);
    toSend[4] = map(az, -32768, 32767, -999, 999);

    if (flagRecord) {
      blinkIndicator(blinkPeriodSlow);
      //Serial.print('$');
      data_str += '$';
      //data_str[0]='$';
      for (byte i = 0; i < 6; i++) {
        //Serial.print(toSend[i]);
        data_str += toSend[i];
        //itoa(toSend[i], data_str+strlen(data_str), DEC);
        if (i < 5) {
          //Serial.print(',');
          data_str += ',';
          //strcat(data_str, ',');
        } else {
          /*Serial.print(',');
            Serial.print(gx);
            Serial.print(',');
            Serial.print(gy);
            Serial.print(',');
            Serial.print(gz);*/
          //Serial.println(';');
          data_str += ';';
          //strcat(data_str, ';');
          send_data();
        }
      }
    }
  }

  if (micros() > 3694967296) { //предупреждение за 10 минут до переполнения 3694967296 // через 10 мин работы 600000000
    blinkIndicator(blinkPeriodFast);
  }
}

void send_data() {
  // Отправка данных по радиоканалу.
  //radio.write(&data_str.buf, sizeof(data_str.buf));

  char charBuf[data_str.length() + 1];
  data_str.toCharArray(charBuf, sizeof(charBuf));
  radio.write(&charBuf, sizeof(charBuf));
  //Serial.print("Data sent: ");
  //  Serial.println(data_str.buf); // Вывод в консоль информации о передаче.
  //Serial.println(data_str);
}

//опорное установлено DEFAULT
uint16_t analogReadFast(uint8_t pin) {
  pin = ((pin < 8) ? pin : pin - 14);    // analogRead(2) = analogRead(A2)
  ADMUX = (DEFAULT << 6) | pin;   // Set analog MUX & reference
  bitSet(ADCSRA, ADSC);            // Start
  while (ADCSRA & (1 << ADSC));        // Wait
  return ADC;                // Return result
}

void digitalWriteFast(uint8_t pin, bool x) {
  switch (pin) {
    case 3: bitClear(TCCR2A, COM2B1);
      break;
    case 5: bitClear(TCCR0A, COM0B1);
      break;
    case 6: bitClear(TCCR0A, COM0A1);
      break;
    case 9: bitClear(TCCR1A, COM1A1);
      break;
    case 10: bitClear(TCCR1A, COM1B1);
      break;
    case 11: bitClear(TCCR2A, COM2A1);   // PWM disable
      break;
  }
  if (pin < 8) {
    bitWrite(PORTD, pin, x);
  } else if (pin < 14) {
    bitWrite(PORTB, (pin - 8), x);
  } else if (pin < 20) {
    bitWrite(PORTC, (pin - 14), x);    // Set pin to HIGH / LOW
  }
}

void blinkIndicator(uint32_t blinkPeriod) {
  if (micros() - blinkTime > blinkPeriod) {
    blinkTime = micros();
    digitalWriteFast(pinIndicator, stateIndicator);
    stateIndicator = !stateIndicator;
  }
}

// =======  ФУНКЦИЯ КАЛИБРОВКИ И ЗАПИСИ В ЕЕПРОМ =======
void calibration() {

  //radio.openWritingPipe(address[0]);
  radio.stopListening();
  //Serial.println("#CALIBRATION...;");
  data_str = F("#CALIBRATION...;");
  delay(10);
  send_data();

  long offsetsNew[6];
  long offsetsNewOld[6];
  int16_t mpuGet[6];
  // используем стандартную точность
  mpu.setFullScaleAccelRange(0);
  mpu.setFullScaleGyroRange(0);
  // обнуляем оффсеты
  mpu.setXAccelOffset(0);
  mpu.setYAccelOffset(0);
  mpu.setZAccelOffset(0);
  mpu.setXGyroOffset(0);
  mpu.setYGyroOffset(0);
  mpu.setZGyroOffset(0);
  delay(5);

  for (byte n = 0; n < 10; n++) {     // 10 итераций калибровки
    for (byte j = 0; j < 6; j++) {    // обнуляем калибровочный массив
      offsetsNew[j] = 0;
    }
    for (byte i = 0; i < 100 + BUFFER_SIZE; i++) {
      // делаем BUFFER_SIZE измерений для усреднения
      mpu.getMotion6(&mpuGet[0], &mpuGet[1], &mpuGet[2], &mpuGet[3], &mpuGet[4], &mpuGet[5]);

      if (i >= 99) {                         // пропускаем первые 99 измерений
        for (byte j = 0; j < 6; j++) {
          offsetsNew[j] += (long)mpuGet[j];   // записываем в калибровочный массив
        }
      }
    }

    for (byte i = 0; i < 6; i++) {
      offsetsNew[i] = offsetsNewOld[i] - ((long)offsetsNew[i] / BUFFER_SIZE); // учитываем предыдущую калибровку
      if (i == 2) offsetsNew[i] += 16384;                               // если ось Z, калибруем в 16384
      offsetsNewOld[i] = offsetsNew[i];
    }

    // ставим новые оффсеты
    mpu.setXAccelOffset(offsetsNew[0] / 8);
    mpu.setYAccelOffset(offsetsNew[1] / 8);
    mpu.setZAccelOffset(offsetsNew[2] / 8);
    mpu.setXGyroOffset(offsetsNew[3] / 4);
    mpu.setYGyroOffset(offsetsNew[4] / 4);
    mpu.setZGyroOffset(offsetsNew[5] / 4);
    delay(2);
  }
  // пересчитываем для хранения
  for (byte i = 0; i < 6; i++) {
    if (i < 3) offsetsNew[i] /= 8;
    else offsetsNew[i] /= 4;
  }
  // запись в память
  EEPROM.put(START_BYTE, offsetsNew);
  data_str = F("#CALIBRATED SUCCESSFULLY;");
  delay(10);
  send_data();
  //Serial.println("#CALIBRATED SUCCESSFULLY;");
}
