#include <Arduino.h>
#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include "secrets.h"
#include "dashboard.h"

const int PWM_PIN = 27;
const int TACH_PIN = 26;
const int POT_PIN = 33;
const int DHT_PIN = 14;
const int BTN_PIN = 32;
const int LED_PIN = 25;
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);

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
    if (out > outMax) { out = outMax; integral -= error * dt; }
    if (out < outMin) { out = outMin; integral -= error * dt; }
    *output = out;
  }
};

PID pid(&pidInput, &pidOutput, &setpoint, Kp, Ki, Kd, 0);

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
WebServer server(80);

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

void setup() {
  Serial.begin(115200);
  Serial.println("Fan Control with Tachometer Starting...");

  dht.begin();

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

  WiFi.mode(WIFI_STA);
  WiFi.setHostname("esp32-fan");
  connectWiFi();

  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setKeepAlive(15);

  server.on("/", handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/cmd", HTTP_POST, handleCommand);
  server.begin();

  Serial.println("Fan controller ready!");
  Serial.println("Press button to toggle MANUAL/AUTO mode");
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
    Serial.println();

    tachPulseCount = 0;
    lastTachTime = currentTime;
  }

  if (currentTime - lastMqttTime >= MQTT_INTERVAL) {
    publishTelemetry();
    lastMqttTime = currentTime;
  }

  delay(50);
}

void connectWiFi() {
  lastWifiAttempt = millis();
  wifiConnected = false;
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
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
