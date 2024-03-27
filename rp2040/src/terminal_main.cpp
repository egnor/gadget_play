#include <algorithm>

#include <Arduino.h>

long blink_until_millis = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("▶️ STARTING");
  Serial1.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(24, INPUT_PULLUP);
}

void loop() {
  int const host_n = std::min(Serial1.available(), Serial.availableForWrite());
  for (int i = 0; i < host_n; ++i) {
    Serial.write(Serial1.read());
  }

  int const modem_n = std::min(Serial.available(), Serial1.availableForWrite());
  for (int i = 0; i < modem_n; ++i) {
    Serial1.write(Serial.read());
  }

  if (host_n > 0 || modem_n > 0) {
    digitalWrite(LED_BUILTIN, HIGH);
    blink_until_millis = millis() + 20;
  } else if (millis() > blink_until_millis) {
    digitalWrite(LED_BUILTIN, LOW);
  }
}
