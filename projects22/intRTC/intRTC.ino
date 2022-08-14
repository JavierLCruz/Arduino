#include <RTClib.h>

RTC_DS3231 rtc;
const int pinalarm=2;

void setup() {
  Serial.begin(9600);
  if(! rtc.begin()){
    Serial.println("RTC no encontrado");
    Serial.flush();
    abort();
  }
  if(rtc.lostPower()){
    Serial.println("Lost power RTC")
  }
  rtc.disable32k();
  pinMode(pinalarm, INPUT_PULLUP);
}

void loop() {
  // put your main code here, to run repeatedly:

}
