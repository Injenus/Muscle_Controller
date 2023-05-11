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

#define sendPeriod 2492 // фактический период на 4-12 мкс больше (разрешение таймера 4мкс) !!!!ИЗМЕНИТЬ!!!
#define blinkPeriodSlow 500000 // период работы в два раза больше
#define blinkPeriodFast 100000

#define START_BYTE 1010 //для записи калибровки в еепром
#define BUFFER_SIZE 100

#define pinExtensorSig 16  // A2
#define pinExtensorRaw 20  // A6
#define pinFlexorSig 17  // A3
#define pinFlexorRaw 15  // A1
#define pinIndicator 13   // D13

int16_t extensorSig; // переменные для значений ЭМГ
int16_t extensorRaw;
int16_t flexorSig;
int16_t flexorRaw;

RF24 radio(9, 10); // "создать" модуль на пинах 9 и 10 Для Уно
byte address[][6] = {"1Node", "2Node", "3Node", "4Node", "5Node", "6Node"}; //возможные номера труб

String string;

/*const int MPU_addr = 0x68; // адрес датчика PU-6050
  int16_t mpuData[7]; // [accX, accY, accZ, temp, gyrX, gyrY, gyrZ]*/
MPU6050 mpu;  // SCL to A5, SDA to A4
int16_t ax, ay, az, gx, gy, gz;
long offsets[6]; // для оффсетов для калибровки

bool flagRecord = false;
bool stateIndicator = false;
bool isMpuOk = false;

uint32_t initTime;
uint32_t pauseTime;
uint32_t deltaTime = 0;
uint32_t currentSendTime = 0;
uint32_t prevSendTime = 0;
uint32_t blinkTime;

int16_t toSend [8]; // массив данных для отправки: extenSig, flexSig, extenRaw, flexRaw, accX, accY, accZ, deltaTime(us)

void setup() {  
  //инициализация UART
  Serial.begin(1000000);
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

  // иницализация пинов ЭМГ
  pinMode(pinExtensorSig, INPUT);
  pinMode(pinExtensorRaw, INPUT);
  pinMode(pinFlexorSig, INPUT);
  pinMode(pinFlexorRaw, INPUT);
  
  if (mpu.testConnection()) {
    mpu.getAcceleration(&ax, &ay, &az);
    if (ax == 0 && ay == 0 && az == 0) {
      string="#ERROR DATA: CHECK MPU6050;";
      transmit_text(string);
      //Serial.println("#ERROR DATA: CHECK MPU6050;");
      isMpuOk = false;
    }
    else {
      string="#All SYSTEMS ONLINE;";
      transmit_text(string);
      //Serial.println("#All SYSTEMS ONLINE;");
      isMpuOk = true;
    }
  }
  else {
    string="#ERROR CONNECTION: CHECK MPU6050;";
    transmit_text(string);
    //Serial.println("#ERROR CONNECTION: CHECK MPU6050;");
    isMpuOk = false;
  }

}

void loop() {
  transmit_text(string);
  delay(800);
  if (isMpuOk) {
    if (Serial.available()) {
      int readData = Serial.read() - '0'; //превращаем символ в число
      switch (readData) {
        case 3: //3 51
          for (byte i = 0; i < 3; i++) {
            digitalWriteFast(pinIndicator, 1);
            delay(40);
            digitalWriteFast(pinIndicator, 0);
            delay(20);
          }
          Serial.println("#READY;");
          break;
        case 4: //4 52
          flagRecord = true;
          initTime = micros();
          currentSendTime = initTime;
          prevSendTime = currentSendTime;
          deltaTime = 0;
          Serial.println("#START;");
          break;
        case 6: //6 54
          flagRecord = true;
          deltaTime = deltaTime + micros() - pauseTime;
          Serial.println("#CONTINUE;");
          break;
        case 5: //5 53
          flagRecord = false;
          pauseTime = micros();
          digitalWriteFast(pinIndicator, 1);
          Serial.println("#PAUSE;");
          break;
        case 7: //7 55
          flagRecord = false;
          digitalWriteFast(pinIndicator, 0);
          Serial.println("#STOP;");
          break;
        case 8: //8 56
          if (!flagRecord) {
            calibration(); // функция калибровки
          }
          break;
      }
    }
  }
  else {
    blinkIndicator(blinkPeriodFast >> 2);
  }

  if (micros() - currentSendTime >= sendPeriod) {
    currentSendTime = micros();
    toSend[7] = currentSendTime - prevSendTime;
    prevSendTime = currentSendTime;

    extensorSig = analogReadFast(pinExtensorSig - 14); //нумерация с А0 (или с 14);
    toSend[0] = extensorSig;
    flexorSig = analogReadFast(pinFlexorSig - 14);
    toSend[1] = flexorSig;

    extensorRaw = analogReadFast(pinExtensorRaw - 14);
    toSend[2] = extensorRaw;
    flexorRaw = analogReadFast(pinFlexorRaw - 14);
    toSend[3] = flexorRaw;

    /*getMpuData();
      for (byte i = 0; i < 3; i++) {
      toSend[i + 4] = mpuData[i];
      }*/
    mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
    toSend[4] = ax;
    toSend[5] = ay;
    toSend[6] = az;

    if (flagRecord) {
      blinkIndicator(blinkPeriodSlow);
      Serial.print('$');
      for (byte i = 0; i < 8; i++) {
        Serial.print(toSend[i]);
        if (i < 7) {
          Serial.print(',');
        } else {
          /*Serial.print(',');
            Serial.print(gx);
            Serial.print(',');
            Serial.print(gy);
            Serial.print(',');
            Serial.print(gz);*/
          Serial.println(';');
        }
      }
    }
  }

  if (micros() > 3694967296) { //предупреждение за 10 минут до переполнения 3694967296 // через 10 мин работы 600000000
    blinkIndicator(blinkPeriodFast);
  }
}


void transmit_text(String text){
  char charBuf[text.length() + 1];
  text.toCharArray(charBuf, sizeof(charBuf));
  // Отправка данных по радиоканалу.
  radio.write(&charBuf, sizeof(charBuf));
  Serial.println("Data sent: " + text); // Вывод в консоль информации о передаче.
}
/*void getMpuData() {
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x3B);  // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_addr, 14, true); // request a total of 14 registers
  for (byte i = 0; i < 7; i++) {
    mpuData[i] = Wire.read() << 8 | Wire.read();
  }
  }*/

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
  Serial.println("#CALIBRATION...;");
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
  Serial.println("#CALIBRATED SUCCESSFULLY;");
}
