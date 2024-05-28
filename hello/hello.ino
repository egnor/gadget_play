#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  Serial.println("SETUP");
}

void loop() {
  static int l = 0;
  Serial.print("LOOP ");
  Serial.println(l++);
  delay(500);
}
