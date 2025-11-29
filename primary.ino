#include <SoftwareSerial.h>
#include <TinyGPSPlus.h>

// GPS: TX -> 4, RX -> 3
SoftwareSerial gpsSerial(4, 3);
TinyGPSPlus gps;

// GSM A7670C: TXD -> 7, RXD -> 8
SoftwareSerial gsm(7, 8);

const char PHONE_NUMBER[] = "+917022587305";
bool smsSent = false;

unsigned long lastPrint = 0;

// Sensors
const int PULSE_PIN = A0;     // Heartbeat
const int TEMP_PIN  = A1;     // LM35 temperature
const int PULSE_THRESHOLD = 550;
const int SAMPLE_TIME = 10;   // seconds

// -------- BUZZER PIN --------
const int BUZZER_PIN = 9;

void setup() {
  Serial.begin(9600);

  gpsSerial.begin(9600);
  gsm.begin(115200);
  gpsSerial.listen();

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.println("GPS + GSM + BPM + Temp SMS System");
  delay(2000);

  // GSM init
  gsm.listen();
  gsm.println("AT");
  delay(500);
  gsm.println("AT+CMGF=1");
  delay(1000);
  gpsSerial.listen();
}

void loop() {
  gpsSerial.listen();

  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }

  // Print GPS data every 2 sec
  if (millis() - lastPrint > 2000) {
    lastPrint = millis();

    Serial.println("---- GPS STATUS ----");
    if (gps.location.isValid()) {
      Serial.print("Lat: ");
      Serial.println(gps.location.lat(), 6);
      Serial.print("Lng: ");
      Serial.println(gps.location.lng(), 6);
    } else {
      Serial.println("Location: NO FIX");
    }
    Serial.println("--------------------");
  }

  if (!smsSent && gps.location.isValid() && gps.satellites.value() >= 3) {
    Serial.println("GPS Fix OK → Reading Sensors");

    int bpm = getBPM();
    float tempC = readTemperature();

    sendSMS(bpm, tempC);

    smsSent = true;
    Serial.println("SMS SENT SUCCESSFULLY!");

    // ---------- BUZZER SOUND ----------
    beepBuzzer();
  }
}

// ====== BUZZER FUNCTION ======
void beepBuzzer() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);
  }
}

// ====== HEARTBEAT BPM FUNCTION ======
int getBPM() {
  Serial.println("Sampling Heartbeat...");

  int beats = 0;
  bool prevHigh = false;
  unsigned long start = millis();

  while (millis() - start < SAMPLE_TIME * 1000UL) {
    int val = analogRead(PULSE_PIN);

    if (val > PULSE_THRESHOLD && !prevHigh) {
      beats++;
      prevHigh = true;
    }
    if (val < PULSE_THRESHOLD - 50) {
      prevHigh = false;
    }

    delay(10);
  }

  int bpm = beats * (60 / SAMPLE_TIME);
  Serial.print("BPM = ");
  Serial.println(bpm);
  return bpm;
}

// ====== LM35 TEMP FUNCTION ======
float readTemperature() {
  int raw = analogRead(TEMP_PIN);
  float mv = raw * (5000.0 / 1023.0);  // mV
  float tempC = mv / 10.0;            // LM35: 10mV = 1°C

  Serial.print("Temp: ");
  Serial.println(tempC);

  return tempC;
}

// ====== SEND SMS FUNCTION ======
void sendSMS(int bpm, float tempC) {
  double lat = gps.location.lat();
  double lng = gps.location.lng();

  String msg = "Soldier Health Report:\n";
  msg += "BPM: " + String(bpm) + "\n";
  msg += "Temp: " + String(tempC, 1) + " C\n\n";
  msg += "Location:\nhttps://www.google.com/maps/?q=";
  msg += String(lat, 6) + "," + String(lng, 6);

  gsm.listen();
  delay(200);

  gsm.print("AT+CMGS=\"");
  gsm.print(PHONE_NUMBER);
  gsm.print("\"\r");
  delay(1500);

  gsm.print(msg);
  gsm.write(26);  // CTRL+Z
  delay(4000);

  gpsSerial.listen();
}
