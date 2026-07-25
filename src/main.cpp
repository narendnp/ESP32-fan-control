#include <Arduino.h>
#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include "secrets.h"
#include "dashboard.h"
#include "portal.h"
#include <DNSServer.h>
#include <Preferences.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>

const int PWM_PIN = 27;
const int TACH_PIN = 26;
const int POT_PIN = 33;
const int DHT_PIN = 14;
const int BTN_PIN = 32;
const int LED_PIN = 25;
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);
unsigned long lastLcdUpdate = 0;
int lcdScreen = 0;

enum FanMode { MANUAL, AUTO };
FanMode fanMode = MANUAL;
enum AutoMode { PID_MODE, LINEAR_MODE };
AutoMode autoMode = PID_MODE;

bool lastBtnState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY = 50;

const int PWM_FREQ = 25000;
const int PWM_RESOLUTION = 8;

volatile unsigned long tachPulseCount = 0;
volatile unsigned long lastTachMicros = 0;
unsigned long lastTachTime = 0;
unsigned long currentRPM = 0;
const unsigned long TACH_SAMPLE_TIME = 1000;

void IRAM_ATTR tachISR() {
  unsigned long now = micros();
  if (now - lastTachMicros > 2000) {
    tachPulseCount = tachPulseCount + 1;
    lastTachMicros = now;
  }
}

int manualPWMOverride = -1;
int lastPotValue = -1;
const int POT_HYSTERESIS = 50;

#define AUTOMATIC 1
#define DIRECT 0

double setpoint = 24.0;
double pidInput, pidOutput;
double Kp = 30.0, Ki = 0.5, Kd = 8.0;

class PID {
private:
  double* input;
  double* output;
  double* setpoint;
  double Kp, Ki, Kd;
  double lastError, integral;
  double outMin, outMax;
  unsigned long lastTime;
  unsigned long sampleTime;
  bool enabled;

public:
  PID(double* in, double* out, double* sp, double kp, double ki, double kd, int dir) {
    input = in; output = out; setpoint = sp;
    Kp = kp; Ki = ki; Kd = kd;
    outMin = 0; outMax = 255;
    lastError = 0; integral = 0;
    lastTime = 0;
    sampleTime = 2000;
    enabled = false;
  }

  void SetMode(int mode) { enabled = (mode == 1); lastTime = millis(); }
  void SetSampleTime(unsigned long ms) { sampleTime = ms; }
  void SetOutputLimits(double min, double max) { outMin = min; outMax = max; }

  void Compute() {
    if (!enabled) return;
    unsigned long now = millis();
    if (now - lastTime < sampleTime) return;
    double dt = (double)(now - lastTime) / 1000.0;
    if (dt > sampleTime * 2 / 1000.0) dt = (double)sampleTime / 1000.0;
    lastTime = now;

    double error = *input - *setpoint;
    double P = Kp * error;

    integral += error * dt;
    if (integral > 500) integral = 500;
    if (integral < -500) integral = -500;
    double I = Ki * integral;

    double D = Kd * (error - lastError) / dt;
    lastError = error;

    double out = P + I + D;
    if (out > outMax) { out = outMax; if (error > 0) integral -= error * dt; }
    if (out < outMin) { out = outMin; if (error < 0) integral -= error * dt; }
    *output = out;
  }
};

PID pid(&pidInput, &pidOutput, &setpoint, Kp, Ki, Kd, 0);

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
WebServer server(80);
DNSServer dnsServer;
Preferences preferences;

unsigned long lastMqttTime = 0;
const unsigned long MQTT_INTERVAL = 2000;
unsigned long lastWifiAttempt = 0;
bool wifiConnected = false;

void connectWiFi();
void connectMQTT();
void publishTelemetry();
String buildStatusJSON();
void handleStatus();
void handleCommand();
void handleRoot();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void processCommand(const char* json);
String getSavedSSID();
String getSavedPass();
void saveCredentials(const String& ssid, const String& pass);
void clearCredentials();
void handlePortal();
void handleStatusPage();
void handlePortalScan();
void handlePortalConnect();
void handlePortalStatus();
void handleWifiStatus();
void handleWifiSave();
void handleWifiForget();
void handleSystemRestart();
void handleNotFound();
void updateLCD(float temp, float humid, int fanSpeed);

void setup() {
  Serial.begin(115200);
  Serial.println("Fan Control with Tachometer Starting...");
  Serial.print("Reset reason: ");
  Serial.println(esp_reset_reason());
  Serial.print("Free heap: ");
  Serial.print(ESP.getFreeHeap());
  Serial.print(" | Max alloc: ");
  Serial.println(ESP.getMaxAllocHeap());

  dht.begin();

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Fan Controller");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");


  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  ledcAttach(PWM_PIN, PWM_FREQ, PWM_RESOLUTION);
  pinMode(TACH_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TACH_PIN), tachISR, FALLING);
  ledcWrite(PWM_PIN, 0);
  lastTachTime = millis();

  pid.SetMode(AUTOMATIC);
  pid.SetSampleTime(2000);
  pid.SetOutputLimits(0, 255);

  WiFi.mode(WIFI_AP_STA);
  WiFi.setHostname("esp32-fan");
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
  IPAddress apIP = WiFi.softAPIP();
  Serial.print("AP IP: ");
  Serial.println(apIP);

  dnsServer.start(53, "*", apIP);

  connectWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    configTime(28800, 0, "pool.ntp.org", "time.nist.gov");
  }

  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setKeepAlive(15);

  server.on("/", handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/cmd", HTTP_POST, handleCommand);
  server.on("/portal", HTTP_GET, handlePortal);
  server.on("/status", HTTP_GET, handleStatusPage);
  server.on("/api/portal/scan", HTTP_POST, handlePortalScan);
  server.on("/api/portal/connect", HTTP_POST, handlePortalConnect);
  server.on("/api/portal/status", HTTP_GET, handlePortalStatus);
  server.on("/api/wifi/status", HTTP_GET, handleWifiStatus);
  server.on("/api/wifi/save", HTTP_POST, handleWifiSave);
  server.on("/api/wifi/forget", HTTP_POST, handleWifiForget);
  server.on("/api/system/restart", HTTP_POST, handleSystemRestart);
  server.onNotFound(handleNotFound);
  server.begin();

  Serial.println("Fan controller ready!");
  Serial.println("Press button to toggle MANUAL/AUTO mode");
}

void updateLCD(float temp, float humid, int fanSpeed) {
  unsigned long now = millis();
  if (now - lastLcdUpdate < 3000) return;
  lastLcdUpdate = now;

  struct tm tm;
  bool timeValid = getLocalTime(&tm, 2000);

  lcd.clear();
  lcd.setCursor(0, 0);

  switch (lcdScreen) {
    case 0: {
      if (timeValid) {
        char buf[17];
        snprintf(buf, sizeof(buf), "Date:%02d/%02d/%02d", tm.tm_mday, tm.tm_mon + 1, (tm.tm_year + 1900) % 100);
        lcd.print(buf);
        lcd.setCursor(0, 1);
        snprintf(buf, sizeof(buf), "Time:%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
        lcd.print(buf);
      } else {
        lcd.print("Date: --/--/--");
        lcd.setCursor(0, 1);
        lcd.print("Time: --:--:--");
      }
      break;
    }
    case 1: {
      char line1[17], line2[17];
      if (!isnan(temp) && !isnan(humid)) {
        snprintf(line1, sizeof(line1), "T:%2.1fC   H:%2.0f%%", temp, humid);
      } else {
        snprintf(line1, sizeof(line1), "T:--.-C   H:--%%");
      }
      int spd = map(fanSpeed, 0, 255, 0, 100);
      unsigned long rpm = (tachPulseCount * 60000) / (TACH_SAMPLE_TIME * 2);
      snprintf(line2, sizeof(line2), "RPM:%-4lu SPD:%d%%", rpm, spd);
      lcd.print(line1);
      lcd.setCursor(0, 1);
      lcd.print(line2);
      break;
    }
    case 2: {
      lcd.print("IP:");
      if (WiFi.status() == WL_CONNECTED) {
        lcd.print(WiFi.localIP().toString());
      } else {
        lcd.print("N/A");
      }
      lcd.setCursor(0, 1);
      lcd.print("Mode:");
      if (fanMode == AUTO) {
        lcd.print(autoMode == PID_MODE ? "AUTO PID   " : "AUTO LINEAR");
      } else {
        lcd.print("MANUAL   ");
      }
      break;
    }
  }

  lcdScreen = (lcdScreen + 1) % 3;
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWifiAttempt > 30000) {
      connectWiFi();
    }
  } else {
    if (!wifiConnected) {
      wifiConnected = true;
      Serial.print("WiFi connected. IP: ");
      Serial.println(WiFi.localIP());
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (!mqtt.connected()) {
      connectMQTT();
    }
    mqtt.loop();
  }

  server.handleClient();
  dnsServer.processNextRequest();

  bool btnState = digitalRead(BTN_PIN);
  if (btnState == LOW && lastBtnState == HIGH && millis() - lastDebounceTime > DEBOUNCE_DELAY) {
    fanMode = (fanMode == MANUAL) ? AUTO : MANUAL;
    lastDebounceTime = millis();
    Serial.print("Switched to ");
    Serial.println(fanMode == AUTO ? "AUTO mode" : "MANUAL mode");
    publishTelemetry();
  }
  lastBtnState = btnState;

  digitalWrite(LED_PIN, fanMode == MANUAL ? HIGH : LOW);

  float temp = dht.readTemperature();
  float humid = dht.readHumidity();
  bool dhtValid = !isnan(temp) && !isnan(humid);

  int fanSpeed;
  int potValue = analogRead(POT_PIN);

  if (fanMode == AUTO) {
    if (dhtValid) {
      if (autoMode == LINEAR_MODE) {
        fanSpeed = constrain(map((int)(temp * 10), 200, 300, 0, 255), 0, 255);
      } else {
        pidInput = temp;
        pid.Compute();
        if (temp < 20.0) {
          fanSpeed = 0;
        } else if (temp > 30.0) {
          fanSpeed = 255;
        } else {
          fanSpeed = constrain((int)pidOutput, 0, 255);
        }
      }
    } else {
      fanSpeed = 0;
    }
  } else {
    if (manualPWMOverride >= 0) {
      fanSpeed = manualPWMOverride;
      if (abs(potValue - lastPotValue) > POT_HYSTERESIS) {
        manualPWMOverride = -1;
        fanSpeed = map(potValue, 0, 4095, 0, 255);
      }
    } else {
      fanSpeed = map(potValue, 0, 4095, 0, 255);
    }
    lastPotValue = potValue;
  }

  ledcWrite(PWM_PIN, fanSpeed);
  int speedPercent = map(fanSpeed, 0, 255, 0, 100);

  unsigned long currentTime = millis();

  if (currentTime - lastTachTime >= TACH_SAMPLE_TIME) {
    currentRPM = (tachPulseCount * 60000) / (TACH_SAMPLE_TIME * 2);
    unsigned long targetRPM = map(speedPercent, 0, 100, 0, 5000);

    Serial.print(fanMode == AUTO ? "[AUTO " : "[MANUAL] ");
    if (fanMode == AUTO) {
      Serial.print(autoMode == PID_MODE ? "PID] " : "LIN] ");
      Serial.print("Set: ");
      Serial.print(setpoint, 1);
      Serial.print("C | Temp: ");
      Serial.print(temp, 1);
      Serial.print("C | ");
    } else {
      Serial.print(potValue);
      Serial.print(" | ");
    }
    Serial.print("PWM: ");
    Serial.print(fanSpeed);
    Serial.print(" | Speed: ");
    Serial.print(speedPercent);
    Serial.print("% | Target: ");
    Serial.print(targetRPM);
    Serial.print(" RPM | Actual: ");
    Serial.print(currentRPM);
    Serial.print(" RPM");
    if (dhtValid) {
      Serial.print(" | Humidity: ");
      Serial.print(humid, 1);
      Serial.print("%");
    } else {
      Serial.print(" | DHT: ERROR");
    }
    Serial.print(" | Heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.print(" (max alloc ");
    Serial.print(ESP.getMaxAllocHeap());
    Serial.print(")");
    Serial.println();

    tachPulseCount = 0;
    lastTachTime = currentTime;
  }

  updateLCD(temp, humid, fanSpeed);

  if (currentTime - lastMqttTime >= MQTT_INTERVAL) {
    publishTelemetry();
    lastMqttTime = currentTime;
  }

  delay(50);
}

void connectWiFi() {
  lastWifiAttempt = millis();
  wifiConnected = false;

  String ssid = getSavedSSID();
  String pass = getSavedPass();

  if (ssid.length() == 0) {
    ssid = WIFI_SSID;
    pass = WIFI_PASS;
  }

  if (ssid.length() == 0 || ssid == "your_ssid") {
    Serial.println("No valid WiFi credentials (offline mode)");
    return;
  }

  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid.c_str(), pass.c_str());
  int timeout = 100;
  while (WiFi.status() != WL_CONNECTED && timeout > 0) {
    delay(100);
    Serial.print(".");
    timeout--;
  }
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.print(" OK (");
    Serial.print(WiFi.localIP());
    Serial.println(")");
  } else {
    Serial.println(" FAILED (offline mode)");
  }
}

void connectMQTT() {
  Serial.print("Connecting to MQTT...");
  String clientId = "esp32-fan-" + String(random(0xffff), HEX);
  if (mqtt.connect(clientId.c_str(), MQTT_USER, MQTT_PASS, "fan/status", 0, true, "{\"online\":0}")) {
    Serial.println(" OK");
    mqtt.publish("fan/status", "{\"online\":1}", true);
    mqtt.subscribe("fan/cmd");
  } else {
    Serial.print(" FAILED (rc=");
    Serial.print(mqtt.state());
    Serial.println(")");
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char buf[length + 1];
  memcpy(buf, payload, length);
  buf[length] = 0;
  processCommand(buf);
}

void publishTelemetry() {
  if (!mqtt.connected()) return;

  String json = buildStatusJSON();
  mqtt.publish("fan/telemetry", json.c_str(), false);
}

String buildStatusJSON() {
  float temp = dht.readTemperature();
  float humid = dht.readHumidity();
  bool dhtValid = !isnan(temp) && !isnan(humid);
  int fanSpeed = ledcRead(PWM_PIN);
  int speedPercent = map(fanSpeed, 0, 255, 0, 100);

  JsonDocument doc;
  doc["t"] = dhtValid ? temp : -1;
  doc["h"] = dhtValid ? humid : -1;
  doc["p"] = fanSpeed;
  doc["s"] = speedPercent;
  doc["r"] = currentRPM;
  doc["m"] = (fanMode == AUTO) ? 1 : 0;
  doc["mq"] = mqtt.connected() ? 1 : 0;
  doc["sp"] = setpoint;
  doc["pid"] = (int)pidOutput;
  doc["am"] = (autoMode == LINEAR_MODE) ? 1 : 0;
  doc["ut"] = millis() / 1000;
  doc["fh"] = ESP.getFreeHeap();

  String json;
  serializeJson(doc, json);
  return json;
}

void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleStatus() {
  server.send(200, "application/json", buildStatusJSON());
}

void handleCommand() {
  String body = server.arg("plain");
  processCommand(body.c_str());
  server.send(200, "application/json", "{\"ok\":1}");
}

void processCommand(const char* json) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) return;

  const char* cmd = doc["cmd"];

  if (strcmp(cmd, "mode_auto") == 0) {
    fanMode = AUTO;
    digitalWrite(LED_PIN, LOW);
    Serial.println("Remote mode: AUTO");
    publishTelemetry();
  }

  if (strcmp(cmd, "mode_manual") == 0) {
    fanMode = MANUAL;
    digitalWrite(LED_PIN, HIGH);
    Serial.println("Remote mode: MANUAL");
    publishTelemetry();
  }

  if (strcmp(cmd, "pwm") == 0 && fanMode == MANUAL) {
    int pct = doc["v"] | -1;
    if (pct >= 0 && pct <= 100) {
      manualPWMOverride = map(pct, 0, 100, 0, 255);
      ledcWrite(PWM_PIN, manualPWMOverride);
      Serial.printf("Remote PWM set: %d%%\n", pct);
    }
  }

  if (strcmp(cmd, "setpoint") == 0) {
    double sp = doc["v"] | 0.0;
    if (sp >= 20.0 && sp <= 30.0) {
      setpoint = sp;
      Serial.printf("Setpoint changed to %.1fC\n", setpoint);
    }
  }

  if (strcmp(cmd, "automode") == 0) {
    const char* v = doc["v"];
    if (strcmp(v, "pid") == 0) {
      autoMode = PID_MODE;
      Serial.println("Auto mode: PID");
    } else if (strcmp(v, "linear") == 0) {
      autoMode = LINEAR_MODE;
      Serial.println("Auto mode: Linear ramp-up");
    }
  }
}

void handlePortal() {
  server.send_P(200, "text/html", PORTAL_HTML);
}

void handleStatusPage() {
  String pass = server.arg("pass");
  if (pass != WIFI_ADMIN_PASS) {
    server.send(200, "text/html", "<!DOCTYPE html><html><body style=\"background:#0f1117;color:#e1e4e8;font-family:sans-serif;padding:40px;text-align:center\"><h1 style=\"color:#58a6ff\">Access Denied</h1><p style=\"color:#8b949e\">Invalid or missing admin password.</p><p style=\"color:#8b949e;margin-top:12px\">Add <code>?pass=YOUR_PASSWORD</code> to the URL.</p></body></html>");
    return;
  }
  String html = String(reinterpret_cast<const __FlashStringHelper*>(STATUS_HTML));
  html.replace("__MQTT_SERVER__", MQTT_SERVER);
  html.replace("__MQTT_PORT__", String(MQTT_PORT));
  html.replace("__MQTT_USER__", MQTT_USER);
  html.replace("__MQTT_PASS__", MQTT_PASS);
  server.send(200, "text/html", html);
}

void handlePortalScan() {
  int n = WiFi.scanNetworks();
  JsonDocument doc;
  JsonArray nets = doc["networks"].to<JsonArray>();
  for (int i = 0; i < n; i++) {
    JsonObject net = nets.add<JsonObject>();
    net["ssid"] = WiFi.SSID(i);
    net["rssi"] = WiFi.RSSI(i);
    net["secured"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN) ? 1 : 0;
  }
  WiFi.scanDelete();
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handlePortalConnect() {
  String body = server.arg("plain");
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) { server.send(400, "application/json", "{\"ok\":0,\"message\":\"Invalid JSON\"}"); return; }
  const char* ssid = doc["ssid"] | "";
  const char* pass = doc["pass"] | "";
  if (strlen(ssid) == 0) { server.send(400, "application/json", "{\"ok\":0,\"message\":\"SSID required\"}"); return; }
  saveCredentials(ssid, pass);
  WiFi.disconnect();
  WiFi.begin(ssid, pass);
  server.send(200, "application/json", "{\"ok\":1,\"message\":\"Connecting...\"}");
}

void handlePortalStatus() {
  JsonDocument doc;
  doc["connected"] = (WiFi.status() == WL_CONNECTED) ? 1 : 0;
  doc["ssid"] = WiFi.SSID();
  doc["ip"] = WiFi.localIP().toString();
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleWifiStatus() {
  String pass = server.arg("pass");
  if (pass != WIFI_ADMIN_PASS) { server.send(403, "application/json", "{\"ok\":0}"); return; }
  String savedSSID = getSavedSSID();
  JsonDocument doc;
  doc["connected"] = (WiFi.status() == WL_CONNECTED) ? 1 : 0;
  doc["ssid"] = WiFi.SSID();
  doc["ip"] = WiFi.localIP().toString();
  doc["savedSsid"] = savedSSID;
  doc["signal"] = WiFi.RSSI();
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleWifiSave() {
  String body = server.arg("plain");
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) { server.send(400, "application/json", "{\"ok\":0}"); return; }
  if (String(doc["adminPass"] | "") != WIFI_ADMIN_PASS) { server.send(403, "application/json", "{\"ok\":0}"); return; }
  const char* ssid = doc["ssid"] | "";
  const char* pass = doc["pass"] | "";
  if (strlen(ssid) == 0) { server.send(400, "application/json", "{\"ok\":0}"); return; }
  saveCredentials(ssid, pass);
  WiFi.disconnect();
  WiFi.begin(ssid, pass);
  server.send(200, "application/json", "{\"ok\":1}");
}

void handleWifiForget() {
  String body = server.arg("plain");
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) { server.send(400, "application/json", "{\"ok\":0}"); return; }
  if (String(doc["adminPass"] | "") != WIFI_ADMIN_PASS) { server.send(403, "application/json", "{\"ok\":0}"); return; }
  clearCredentials();
  WiFi.disconnect();
  server.send(200, "application/json", "{\"ok\":1}");
}

void handleSystemRestart() {
  String body = server.arg("plain");
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) { server.send(400, "application/json", "{\"ok\":0}"); return; }
  if (String(doc["adminPass"] | "") != WIFI_ADMIN_PASS) { server.send(403, "application/json", "{\"ok\":0}"); return; }
  server.send(200, "application/json", "{\"ok\":1,\"message\":\"Restarting...\"}");
  delay(500);
  ESP.restart();
}

void handleNotFound() {
  server.sendHeader("Location", "/portal", true);
  server.send(302, "text/plain", "");
}

String getSavedSSID() {
  preferences.begin("wifi", true);
  String val = preferences.getString("ssid", "");
  preferences.end();
  return val;
}

String getSavedPass() {
  preferences.begin("wifi", true);
  String val = preferences.getString("pass", "");
  preferences.end();
  return val;
}

void saveCredentials(const String& ssid, const String& pass) {
  preferences.begin("wifi", false);
  preferences.putString("ssid", ssid);
  preferences.putString("pass", pass);
  preferences.end();
  Serial.println("WiFi credentials saved to NVS");
}

void clearCredentials() {
  preferences.begin("wifi", false);
  preferences.remove("ssid");
  preferences.remove("pass");
  preferences.end();
  Serial.println("WiFi credentials cleared from NVS");
}
