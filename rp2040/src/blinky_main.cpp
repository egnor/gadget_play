#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  Serial.println("STARTING");
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(24, INPUT_PULLUP);
}

void loop() {
  Serial.println("Blinking ON");
  digitalWrite(LED_BUILTIN, HIGH);
  delay(500);
  Serial.println("Blinking OFF");
  digitalWrite(LED_BUILTIN, LOW);
  delay(500);
}
