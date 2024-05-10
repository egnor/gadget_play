#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  Serial.println("SETUP");
}

void loop() {
  Serial.println("LOOP");
  delay(500);
}
