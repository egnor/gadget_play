#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  Serial.println("SETUP");
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  Serial.println("LOOP");
  delay(250);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(250);
  digitalWrite(LED_BUILTIN, LOW);
}
