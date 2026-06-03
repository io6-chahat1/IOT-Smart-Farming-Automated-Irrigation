#define BLYNK_TEMPLATE_ID "TMPL3cV2g4wS8"
#define BLYNK_TEMPLATE_NAME "IOT Smart Farming and Automated Irrigation system"
#define BLYNK_AUTH_TOKEN "SQ8eH7BJGZsWMD2s_DtYYAK16ZEnwoKq"
#define BLYNK_PRINT Serial

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <DHT.h>

char ssid[] = "ESPTEST";
char pass[] = "12345678";

// ===== PINS =====
#define DHTPIN      D5
#define DHTTYPE     DHT22
#define SOIL_PIN    A0
#define RELAY_PIN   D2    // D1 se D2 change kiya
#define BUTTON_PIN  D7
#define BUZZER_PIN  D6

// ===== BLYNK VIRTUAL PINS =====
#define VPIN_TEMP    V0
#define VPIN_HUM     V1
#define VPIN_SOIL    V2
#define VPIN_PUMP    V3 
#define VPIN_AUTO    V4
#define VPIN_BUZZER  V6

DHT dht(DHTPIN, DHTTYPE);
BlynkTimer timer;

// ===== STATE =====
bool pumpState   = false;
bool autoMode    = true;
bool buzzerState = false;
int  lastButtonState = HIGH;
int  soilPercent = 0;

// ===== CALIBRATION =====
int soilDry = 850;
int soilWet = 350;

// ===== THRESHOLD =====
int moistureThreshold = 80;

// ===== BUZZER TIMER =====
bool          alertActive  = false;
unsigned long buzzerStart  = 0;
const unsigned long buzzerDuration = 3000;

// ===== RELAY MODE =====
// Agar pump ON nahi ho raha to neeche RELAY_ACTIVE_LOW = false karo
#define RELAY_ACTIVE_LOW true

// ================================================
//  PUMP
// ================================================

void setPump(bool state) {
  pumpState = state;

  if (RELAY_ACTIVE_LOW) {
    digitalWrite(RELAY_PIN, state ? LOW : HIGH);
  } else {
    digitalWrite(RELAY_PIN, state ? HIGH : LOW);
  }

  Blynk.virtualWrite(VPIN_PUMP, state ? 1 : 0);
  Serial.print("[PUMP] ");
  Serial.println(state ? "ON" : "OFF");
}

// ================================================
//  BUZZER
// ================================================

void setBuzzer(bool state) {
  buzzerState = state;
  digitalWrite(BUZZER_PIN, state ? HIGH : LOW);
  Blynk.virtualWrite(VPIN_BUZZER, state ? 1 : 0);
  Serial.print("[BUZZER] ");
  Serial.println(state ? "ON" : "OFF");
}

// ================================================
//  DHT22
// ================================================

void readDHTData() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("[DHT] Failed!");
    return;
  }

  Blynk.virtualWrite(VPIN_TEMP, t);
  Blynk.virtualWrite(VPIN_HUM,  h);

  Serial.print("[DHT] Temp: ");
  Serial.print(t);
  Serial.print(" C  Hum: ");
  Serial.print(h);
  Serial.println(" %");
}

// ================================================
//  SOIL
// ================================================

void readSoilData() {
  int raw = analogRead(SOIL_PIN);
  soilPercent = map(raw, soilDry, soilWet, 0, 100);
  soilPercent = constrain(soilPercent, 0, 100);

  Blynk.virtualWrite(VPIN_SOIL, soilPercent);

  Serial.print("[SOIL] Raw: ");
  Serial.print(raw);
  Serial.print("  Moisture: ");
  Serial.print(soilPercent);
  Serial.println("%");
}

// ================================================
//  AUTO CONTROL
// ================================================

void autoControl() {
  if (!autoMode) return;

  if (soilPercent < moistureThreshold) {
    // DRY → Pump ON
    if (!pumpState)  setPump(true);
    if (buzzerState) setBuzzer(false);
    alertActive = false;
    Serial.println("[AUTO] Dry → Pump ON");

  } else {
    // WET → Pump OFF + Buzzer beep
    if (pumpState) setPump(false);

    if (!alertActive) {
      setBuzzer(true);
      alertActive = true;
      buzzerStart = millis();
      Serial.println("[AUTO] Wet → Pump OFF, Buzzer ON");
    }
  }
}

// ================================================
//  BUZZER AUTO OFF
// ================================================

void handleBuzzerTimer() {
  if (alertActive && (millis() - buzzerStart >= buzzerDuration)) {
    setBuzzer(false);
    alertActive = false;
    Serial.println("[BUZZER] Auto OFF");
  }
}

// ================================================
//  PHYSICAL BUTTON (D7)
// ================================================

void checkButton() {
  int currentState = digitalRead(BUTTON_PIN);

  if (currentState == LOW && lastButtonState == HIGH) {
    delay(50);
    if (digitalRead(BUTTON_PIN) == LOW) {

      autoMode = false;
      Blynk.virtualWrite(VPIN_AUTO, 0);

      bool newPump = !pumpState;
      setPump(newPump);

      // Short beep feedback
      digitalWrite(BUZZER_PIN, HIGH);
      delay(100);
      digitalWrite(BUZZER_PIN, LOW);

      Serial.print("[BUTTON] Manual → Pump ");
      Serial.println(newPump ? "ON" : "OFF");

      while (digitalRead(BUTTON_PIN) == LOW) delay(10);
    }
  }

  lastButtonState = currentState;
}

// ================================================
//  SEND DATA (every 3 sec)
// ================================================

void sendData() {
  readDHTData();
  readSoilData();
  autoControl();
  handleBuzzerTimer();
  Serial.println("----------------------------");
}

// ================================================
//  BLYNK CALLBACKS
// ================================================

BLYNK_CONNECTED() {
  Blynk.virtualWrite(VPIN_AUTO,   autoMode    ? 1 : 0);
  Blynk.virtualWrite(VPIN_PUMP,   pumpState   ? 1 : 0);
  Blynk.virtualWrite(VPIN_BUZZER, buzzerState ? 1 : 0);
  Serial.println("[BLYNK] Connected & Synced");
}

BLYNK_WRITE(VPIN_PUMP) {
  if (!autoMode) {
    setPump(param.asInt());
  } else {
    Blynk.virtualWrite(VPIN_PUMP, pumpState ? 1 : 0);
  }
}

BLYNK_WRITE(VPIN_AUTO) {
  autoMode = param.asInt();
  Serial.print("[MODE] Auto: ");
  Serial.println(autoMode ? "ON" : "OFF");

  if (!autoMode) {
    setBuzzer(false);
    alertActive = false;
  }
}

BLYNK_WRITE(VPIN_BUZZER) {
  if (!autoMode) {
    setBuzzer(param.asInt());
  } else {
    Blynk.virtualWrite(VPIN_BUZZER, buzzerState ? 1 : 0);
  }
}

// ================================================
//  SETUP
// ================================================

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Smart Farming Boot ===");

  pinMode(RELAY_PIN,  OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(RELAY_PIN,  HIGH);  // Relay OFF
  digitalWrite(BUZZER_PIN, LOW);   // Buzzer OFF

  dht.begin();
  delay(2000);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  timer.setInterval(3000L, sendData);
  timer.setInterval(100L,  checkButton);
}

// ================================================
//  LOOP
// ================================================

void loop() {
  Blynk.run();
  timer.run();
}
