#include <Arduino.h>
#include <DHT.h>

// Pin definitions
const int PWM_PIN = 27;   // GPIO 27
const int TACH_PIN = 26;  // GPIO 26
const int POT_PIN = 33;  // GPIO 33 (analog)
const int DHT_PIN = 14; // GPIO 14
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);

// Push button and LED
const int BTN_PIN = 32;
const int LED_PIN = 25;

enum FanMode { MANUAL, AUTO };
FanMode fanMode = MANUAL;

bool lastBtnState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY = 50;

const float TEMP_MIN = 30.0;
const float TEMP_MAX = 50.0;

// PWM settings
const int PWM_FREQ = 25000;    // 25 kHz frequency for computer fans
const int PWM_RESOLUTION = 8;  // 8-bit resolution (0-255)
 
// Tachometer variables
volatile unsigned long tachPulseCount = 0;
unsigned long lastTachTime = 0;
const unsigned long TACH_SAMPLE_TIME = 1000;  // Sample period in milliseconds
 
// Interrupt service routine for tachometer
void IRAM_ATTR tachISR() {
  tachPulseCount = tachPulseCount + 1;
}
 
void setup() {
 
  // Start Serial Monitor
  Serial.begin(115200);
  Serial.println("Fan Control with Tachometer Starting...");

  // Initialize DHT22 sensor
  dht.begin();

  // Configure button and LED
  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Configure PWM
  ledcAttach(PWM_PIN, PWM_FREQ, PWM_RESOLUTION);
 
  // Configure tachometer pin with interrupt
  pinMode(TACH_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TACH_PIN), tachISR, FALLING);
 
  // Set initial fan speed to zero
  ledcWrite(PWM_PIN, 0);
 
  // Initialize timing
  lastTachTime = millis();
 
  Serial.println("Fan controller ready!");
  Serial.println("Press button to toggle MANUAL/AUTO mode");
}
 
void loop() {

  // --- Button debounce + mode toggle ---
  bool btnState = digitalRead(BTN_PIN);
  if (btnState == LOW && lastBtnState == HIGH && millis() - lastDebounceTime > DEBOUNCE_DELAY) {
    fanMode = (fanMode == MANUAL) ? AUTO : MANUAL;
    lastDebounceTime = millis();
    Serial.print("Switched to ");
    Serial.println(fanMode == AUTO ? "AUTO mode" : "MANUAL mode");
  }
  lastBtnState = btnState;

  digitalWrite(LED_PIN, fanMode == AUTO ? LOW : HIGH); // LED ON in MANUAL mode, OFF in AUTO mode

  // --- Read DHT22 ---
  float temp = dht.readTemperature();
  float humid = dht.readHumidity();
  bool dhtValid = !isnan(temp) && !isnan(humid);

  // --- Fan speed calculation ---
  int fanSpeed;
  int potValue = analogRead(POT_PIN);

  if (fanMode == AUTO) {
    if (dhtValid) {
      fanSpeed = constrain(map((temp - TEMP_MIN) * 100, 0, (TEMP_MAX - TEMP_MIN) * 100, 0, 255), 0, 255);
    } else {
      fanSpeed = 0;
    }
  } else {
    fanSpeed = map(potValue, 0, 4095, 0, 255);
  }

  ledcWrite(PWM_PIN, fanSpeed);
  int speedPercent = map(fanSpeed, 0, 255, 0, 100);

  // --- Tachometer + serial every 1s ---
  unsigned long currentTime = millis();
  if (currentTime - lastTachTime >= TACH_SAMPLE_TIME) {
    unsigned long rpm = (tachPulseCount * 60000) / (TACH_SAMPLE_TIME * 2);
    unsigned long targetRPM = map(speedPercent, 0, 100, 0, 5000);

    Serial.print(fanMode == AUTO ? "[AUTO] " : "[MANUAL] ");
    Serial.print(fanMode == AUTO ? "Temp: " : "Pot: ");
    Serial.print(fanMode == AUTO ? temp : potValue);
    Serial.print(fanMode == AUTO ? "C | " : " | ");
    Serial.print("PWM: ");
    Serial.print(fanSpeed);
    Serial.print(" | Speed: ");
    Serial.print(speedPercent);
    Serial.print("% | Target: ");
    Serial.print(targetRPM);
    Serial.print(" RPM | Actual: ");
    Serial.print(rpm);
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

  delay(50);
}