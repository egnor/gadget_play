#include <math.h>
#include <stdio.h>

#include <Adafruit_MPR121.h>
#include <FastLED.h>

CRGB *pixel(CRGBArray<256> *leds, uint8_t x, uint8_t y) {
  if ((y % 2) == 0) {
    return &(*leds)[(15 - y) * 16 + 15 - x];
  } else {
    return &(*leds)[(15 - y) * 16 + x];
  }
}

static uint32_t next_millis = 0;
static uint16_t frame = 0;

static CRGBArray<256> leds;

static Adafruit_MPR121 touch;

void setup() {
  Serial.begin(115200);
  Serial.println("=== flexmatrix demo starting! ===");

  FastLED.addLeds<NEOPIXEL, 11>(leds, leds.len);

  if (!touch.begin()) {
    Serial.println("*** MPR121 init failed ***");
    while (true) {}
  }

  next_millis = millis();
}

void loop() {
  const uint16_t touched = touch.touched();

  for (uint8_t y = 0 ; y < 16; ++y) {
    for (uint8_t x = 0 ; x < 16; ++x) {
      const float dx = x - 7.5f, dy = y - 7.5f;
      const float r = sqrtf(dx * dx + dy * dy);
      const float ang = atan2f(dy, dx);
      const uint8_t hue = (ang + frame * 0.1f) * 128 / M_PI;
      const uint8_t sat = 255;  // min(255, r * 256 / 7.0f);
      pixel(&leds, x, y)->setHSV(hue, sat, 128);
    }
  }

  const CRGB off(0, 0, 0), on(255, 255, 255);
  for (uint8_t s = 0; s < 12; ++s) {
    const CRGB &color = touched & (1 << s) ? on : off;
    for (uint8_t y = 3 + (s / 6) * 8; y < 6 + (s / 6) * 8; ++y)
      for (uint8_t x = 2 + (s % 6) * 2; x < 4 + (s % 6) * 2; ++x)
        *pixel(&leds, x, y) = color;
  }

  const CRGB border(32, 32, 32);
  for (uint8_t x = 1; x < 15; ++x) {
    *pixel(&leds, x, 2) = border;
    *pixel(&leds, x, 6) = border;
    *pixel(&leds, x, 10) = border;
    *pixel(&leds, x, 14) = border;
  }
  for (uint8_t y = 3; y < 6; ++y) {
    *pixel(&leds, 1, y) = border;
    *pixel(&leds, 14, y) = border;
    *pixel(&leds, 1, y + 8) = border;
    *pixel(&leds, 14, y + 8) = border;
  }

  // for (uint8_t v = 0; v < 16; ++v) {
  //   pixel(&leds, v, 0)->setRGB(255, 0, 0);
  //   pixel(&leds, 0, v)->setRGB(0, 255, 0);
  // }
  // pixel(&leds, 0, 0)->setRGB(255, 255, 255);

  FastLED.show();

  Serial.print("frame ");
  Serial.print(frame);
  Serial.print(" ");
  for (uint8_t s = 0; s < 12; ++s) {
    Serial.print(touched & (1 << s) ? "*" : ".");
  }
  Serial.println();

  ++frame;
  next_millis += 100;
  while (millis() < next_millis) {}
}
