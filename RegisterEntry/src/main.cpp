#include <Arduino.h>

constexpr int irTransmitter = 15;
constexpr int sensorOutPin = 27;
constexpr int ch = 0;
constexpr int freq = 38000;
constexpr int res = 8;
constexpr int duty = 128;

constexpr uint32_t burst_ms = 200;

void sendIRBurst(uint32_t ms) {
  ledcWrite(ch, duty);
  delay(ms);
  ledcWrite(ch, 0);
}

void setup() {
  Serial.begin(115200);
  delay(50);

  ledcSetup(ch, freq, res);
  ledcAttachPin(irTransmitter, ch);
  ledcWrite(ch, 0);

  auto cause = esp_sleep_get_wakeup_cause();

  if (cause == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("Sensor wake -> sending IR");
    sendIRBurst(burst_ms);
  }
  else  Serial.println("Power on / reset");

  pinMode(sensorOutPin, INPUT);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)sensorOutPin, 0);

  Serial.println("Sleeping...");
  esp_deep_sleep_start();
}

void loop() {}