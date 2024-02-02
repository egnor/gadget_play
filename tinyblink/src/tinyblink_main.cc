#include <Arduino.h>

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  Serial.printf("Hello, world!\n");
  delay(500);
}
