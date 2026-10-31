#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <DHT_U.h>
#include <Adafruit_Sensor.h>

bool opSerialActiva = false;
unsigned long last1s = 0;
unsigned long last2s = 0;

const byte STX = 0x02;
const byte ETX = 0x03;
const byte ACK = 0x06;
const byte NAK = 0x15;
const int TIMEOUT_MS = 100;

#define CMD_START  0xAA
#define CMD_STOP   0xFF

#pragma pack(1)
struct FechaHora {
  uint16_t year;			// 2 bytes
  uint8_t  month;			// 3
  uint8_t  day;				// 4
  uint8_t  hour;			// 5
  uint8_t  minute;			// 6
  uint8_t  second;			// 7
};
struct SensorData {
  float    temperature;		// 4
  float    humidity;		// 8
  float    heatIndex;		// 12
  float    rtcTemp;			// 16
};
#pragma pack()
  
LiquidCrystal_I2C lcd(0x23, 16, 2);
RTC_DS3231 rtc;
DHT_Unified dht(7, DHT11);

void setup() {
	rtc.begin();
	if (rtc.lostPower()) {
	    rtc.adjust(DateTime( __DATE__, __TIME__));
	}
	lcd.init();
	lcd.backlight();
	dht.begin();
	pinMode(LED_BUILTIN, OUTPUT);
	Serial.begin(115200);
}

void loop(){
	if (Serial.available() >= 1) {
		byte buf[1];
		int r = receiveFrame(buf, 1);
		if (r == 0) {
			sendFrame((const byte *)&ACK, sizeof(ACK));
			if (buf[0] == CMD_START){
				sensor_t sensor;
	  			dht.temperature().getSensor(&sensor);
	  			sendFrame((const byte *)&sensor, sizeof(sensor));
				dht.humidity().getSensor(&sensor);
				sendFrame((const byte *)&sensor, sizeof(sensor));
				FechaHora inifh;
				DateTime inidt = rtc.now();
		    	inifh.year        = inidt.year();
		    	inifh.month       = inidt.month();
		    	inifh.day         = inidt.day();
		    	inifh.hour        = inidt.hour();
		    	inifh.minute      = inidt.minute();
		    	inifh.second      = inidt.second();
				sendFrame((const byte *)&inifh, sizeof(inifh));
				opSerialActiva = true;
			}
			else if (buf[0] == CMD_STOP){
				FechaHora inifh;
				DateTime inidt = rtc.now();
		    	inifh.year        = inidt.year();
		    	inifh.month       = inidt.month();
		    	inifh.day         = inidt.day();
		    	inifh.hour        = inidt.hour();
		    	inifh.minute      = inidt.minute();
		    	inifh.second      = inidt.second();
				sendFrame((const byte *)&inifh, sizeof(inifh));
				opSerialActiva = false;
			}
		}
	    else if (r == -3 || r == -4) {
	    	sendFrame((const byte *)&NAK, sizeof(NAK));
		}
	}
	unsigned long actual = millis();
	if(actual - last1s >= 1000) {
		last1s = actual;
	    DateTime dt1s = rtc.now();
		lcdDate(dt1s);
	}
	if(actual - last2s >= 2000){
	  	last2s = actual;
	    float trtc = rtc.getTemperature();
   	    lcd.setCursor(0,0);
	    sensors_event_t tEvent; sensors_event_t hEvent;
	    float t=0.0, h=0.0, hic=0.0;
	    dht.temperature().getEvent(&tEvent);
	    dht.humidity().getEvent(&hEvent);
	    if (isnan(tEvent.temperature) || isnan(hEvent.relative_humidity)) {
	    	lcd.print("Err HT11");
			if (opSerialActiva) {
		        byte err = 0xFE;
		        sendFrame(&err, 1);   // notifica el error
			}
      		return;
    	}
    	else{
    		t = tEvent.temperature;
    		h = hEvent.relative_humidity;
	    	lcd.print((int)t);  lcd.print("C ");
	    	lcd.print((int)h);  lcd.print("%");
	    	lcd.setCursor(0,1);
	    	hic = calcHic(t, h);
			lcd.print(hic);
			lcd.print("ST ");
		}
   	    if (opSerialActiva) {
	    	SensorData data;
	    	data.temperature = t;
	    	data.humidity    = h;
		   	data.heatIndex   = hic;
    		data.rtcTemp     = trtc;
    		sendFrame((const byte *)&data, sizeof(data));
		}
	}
}
// --- Funcion calcHic ---------------------------------------------------------
float calcHic(float t, float h){
  float hi;
  t = t * 1.8 + 32;                 // °C → °F
  hi = 0.5 * (t + 61.0 + ((t - 68.0) * 1.2) + (h * 0.094));
  if (hi > 79) {
    hi = -42.379 + 2.04901523 * t + 10.14333127 * h +
         -0.22475541 * t * h +
         -0.00683783 * pow(t, 2) +
         -0.05481717 * pow(h, 2) +
         0.00122874 * pow(t, 2) * h +
         0.00085282 * t * pow(h, 2) +
         -0.00000199 * pow(t, 2) * pow(h, 2);

    if ((h < 13) && (t >= 80.0) && (t <= 112.0))
      hi -= ((13.0 - h) * 0.25) * sqrt((17.0 - abs(t - 95.0)) * 0.05882);

    else if ((h > 85.0) && (t >= 80.0) && (t <= 87.0))
      hi += ((h - 85.0) * 0.1) * ((87.0 - t) * 0.2);
  }
  hi = (hi - 32.0f) *  0.55555;      // °F → °C
  return hi;
}
// --- Funcion lcdDate ---------------------------------------------------------
void lcdDate(DateTime dt){
    char fecha[9];
    char hora[9];
 	snprintf(fecha, sizeof(fecha), "%02d/%02d/%02d", dt.day(), dt.month(), dt.year() % 100);
	lcd.setCursor(8,0);
	lcd.print(fecha);
	snprintf(hora, sizeof(hora), "%02d:%02d:%02d", dt.hour(), dt.minute(), dt.second());
	lcd.setCursor(8,1);
	lcd.print(hora);
}
// --- Envío (Arduino → PC) ----------------------------------------------------
void sendFrame(const byte *data, uint8_t len) {
  Serial.write(STX);
  Serial.write(data, len);
  // Checksum simple (suma de bytes)
  uint8_t sum = 0;
  for (uint8_t i = 0; i < len; i++) sum += data[i];
  Serial.write(sum);
  Serial.write(ETX);
}
// --- Recepción (PC → Arduino) ---
// El PC envía: STX | 0xAA (START) | sum | ETX  → Arduino responde ACK
// El PC envía: STX | 0xFF (STOP)  | sum | ETX  → Arduino responde ACK
int receiveFrame(byte *data, uint8_t expectedLen) {
  unsigned long start = millis();
  // Esperar STX
  while (Serial.available() == 0) {
    if (millis() - start > TIMEOUT_MS) return -1; // timeout
  }
  if (Serial.read() != STX) return -2;		// no es STX
  // Leer datos + checksum
  for (uint8_t i = 0; i < expectedLen; i++) {
    while (Serial.available() == 0) {
      if (millis() - start > TIMEOUT_MS) return -1;
    }
    data[i] = Serial.read();
  }
  uint8_t sum = 0;
  for (uint8_t i = 0; i < expectedLen; i++) sum += data[i];

  while (Serial.available() == 0) {
    if (millis() - start > TIMEOUT_MS) return -1;
  }
  if (Serial.read() != sum) return -3; // checksum error

  while (Serial.available() == 0) {
    if (millis() - start > TIMEOUT_MS) return -1;
  }
  if (Serial.read() != ETX) return -4;	// no es ETX

  return 0; // OK
}   

