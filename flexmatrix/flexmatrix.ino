#include <stdio.h>

#include <FastLED.h>

static uint32_t next_millis = 0;
static uint16_t frame = 0;

CRGBArray<256> leds;

void setup() {
  Serial.begin(115200);
  Serial.println("=== flexmatrix demo starting ===");
  next_millis = millis();

  FastLED.addLeds<NEOPIXEL, 11>(leds, leds.len);
}

void loop() {
  for (uint16_t i = 0 ; i < leds.len; ++i) {
    uint16_t p = i + frame;
    leds[i].setRGB(16 * (p % 16), p % 256, (p % 4096) / 16);
  }
  FastLED.show();

  Serial.print("frame ");
  Serial.println(frame);

  ++frame;
  next_millis += 100;
  while (millis() < next_millis) {}
}
