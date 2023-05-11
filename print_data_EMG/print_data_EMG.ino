#include <GParser.h>

#define PERIOD 2

int extensorDataPin = 14;
int flexorDataPin = 15;
int led = 13;
int extensorDataEMG;
int flexorDataEMG;
byte angA=31;
byte angH=150;
bool flagRecord=false;

long initTime;
long pauseTime;
long deltaTime = 0;
long sendTime = 0;

void setup() {
  for (int i=2;i<=21;i++){
    pinMode(i,OUTPUT);
  }
  pinMode(extensorDataPin,INPUT);
  pinMode(flexorDataPin,INPUT);
  Serial.begin(115200);
  Serial.setTimeout(50);
  Serial.flush(); //очищаем буфер
  digitalWrite(13, HIGH);
  delay(100);
  digitalWrite(13, LOW);
  Serial.println("READY (setup)");
  //Serial.println();
  //Serial.println("sig,min,max");
}

void loop() {
  if (Serial.available()) {
    char txs[100];
    int amount = Serial.readBytesUntil(';', txs, 100);
    txs[amount] = NULL;
    GParser data(txs, ',');
    int arrData[data.amount()];
    int am = data.parseInts(arrData);

    switch (arrData[0]) {
      case 7:
        digitalWrite(13, 1);
        delay(50);
        digitalWrite(13, 0);
        Serial.println("READY");
        break;
      case 4:
        flagRecord=true;
        initTime = millis();
        deltaTime = 0;
        //Serial.println("START");
        break;
      case 6:
        flagRecord=true;
        deltaTime = deltaTime + millis()- pauseTime;
        //Serial.println("CONTINUE");
        break;
      case 5:
        flagRecord=false;
        pauseTime = millis();
        //Serial.println("PAUSE");
        break;
      case 8:
        flagRecord=false;
        //Serial.println("STOP");
        break;            
    }
  }
    extensorDataEMG=analogRead(extensorDataPin);
    flexorDataEMG=analogRead(flexorDataPin);
    
    if (flagRecord){
      if (millis()-sendTime > PERIOD or true){
        sendTime = millis();    
        Serial.print(extensorDataEMG);
        Serial.print(',');
        //Serial.print(flexorDataEMG);
        //Serial.print(',');
        Serial.print(angA);
        Serial.print(',');
        Serial.print(angH);
        Serial.print(',');
        //Serial.println(millis() - initTime - deltaTime);
        
        //Serial.print(0);
        //Serial.print(',');
        Serial.println(1100);
      }
    }
}
