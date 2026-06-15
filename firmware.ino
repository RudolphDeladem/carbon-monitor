#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <SparkFun_SCD30_Arduino_Library.h>

// ─── Config ─────────────────────────────────────────────────────
const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* SERVER_URL    = "https://your-app.onrender.com/data"; // replace after deploy

// ─── Pins & calibration ─────────────────────────────────────────
#define MQ7_PIN      34
#define RL           10.0
#define RO_CLEAN     10.0
#define VOLTAGE_IN   5.0
#define TEMP_OFFSET  8.0
#define SEND_INTERVAL 10000  // send every 10 seconds

SCD30 airSensor;
unsigned long lastSend = 0;

// ─── MQ-7 ppm ───────────────────────────────────────────────────
float getMQ7ppm() {
  int   raw     = analogRead(MQ7_PIN);
  float voltage = raw * (3.3 / 4095.0);
  if (voltage <= 0.01) return 0;
  float rs    = ((VOLTAGE_IN / voltage) - 1.0) * RL;
  float ratio = rs / RO_CLEAN;
  return 99.042 * pow(ratio, -1.518);
}

// ─── WiFi connect ───────────────────────────────────────────────
void connectWiFi() {
  Serial.printf("Connecting to %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 30) {
    delay(500);
    Serial.print(".");
    tries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected → " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi failed — will retry");
  }
}

// ─── Send to server ─────────────────────────────────────────────
void sendData(float co2, float temp, float hum, float co) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi down — reconnecting...");
    connectWiFi();
    return;
  }

  HTTPClient http;
  http.begin(SERVER_URL);
  http.addHeader("Content-Type", "application/json");

  String body = "{";
  body += "\"co2\":"  + String(co2,  1) + ",";
  body += "\"co\":"   + String(co,   1) + ",";
  body += "\"temp\":" + String(temp, 1) + ",";
  body += "\"hum\":"  + String(hum,  1);
  body += "}";

  int code = http.POST(body);
  if (code == 200) {
    Serial.println("Sent OK → " + body);
  } else {
    Serial.printf("Send failed (HTTP %d)\n", code);
  }
  http.end();
}

// ─── Setup ──────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  if (!airSensor.begin()) {
    Serial.println("ERROR: SCD30 not found! Check wiring.");
    while (1);
  }
  airSensor.setMeasurementInterval(5);
  airSensor.setTemperatureOffset(TEMP_OFFSET);
  Serial.println("SCD30 ready");

  // MQ-7 preheat
  Serial.println("MQ-7 preheating (60s)...");
  unsigned long t = millis();
  while (millis() - t < 60000) {
    int left = (60000 - (millis() - t)) / 1000;
    Serial.printf("  %ds remaining\n", left);
    delay(5000);
  }
  Serial.println("MQ-7 ready");

  connectWiFi();
}

// ─── Loop ───────────────────────────────────────────────────────
void loop() {
  if (millis() - lastSend >= SEND_INTERVAL) {
    float co2 = 0, temp = 0, hum = 0;

    if (airSensor.dataAvailable()) {
      co2  = airSensor.getCO2();
      temp = airSensor.getTemperature();
      hum  = airSensor.getHumidity();
    }

    float co = getMQ7ppm();

    // Print to serial too (useful for debugging)
    Serial.printf("CO2: %.0f ppm | CO: %.1f ppm | Temp: %.1f C | RH: %.1f%%\n",
                  co2, co, temp, hum);

    sendData(co2, temp, hum, co);
    lastSend = millis();
  }
}
