#include <math.h>
#include <stdio.h>

#define FASTLED_INTERNAL  // https://github.com/FastLED/FastLED/issues/851

#include <Adafruit_MPR121.h>
#include <FastLED.h>

CRGB *pixel(CRGBArray<256> *leds, uint8_t x, uint8_t y) {
  if ((y % 2) == 0) {
    return &(*leds)[(15 - y) * 16 + 15 - x];
  } else {
    return &(*leds)[(15 - y) * 16 + x];
  }
}

void setup_touch(Adafruit_MPR121 *touch) {
  touch->writeRegister(0x5E, 0x00);  // Electrode Configuration: STOP

  touch->writeRegister(0x2B, 1);     // Rising: Maximum Half Delta
  touch->writeRegister(0x2C, 1);     // Rising: Noise Half Delta
  touch->writeRegister(0x2D, 0);     // Rising: Noise Count Limit
  touch->writeRegister(0x2E, 0);     // Rising: Filter Delay Count

  touch->writeRegister(0x2F, 1);     // Falling: Maximum Half Delta
  touch->writeRegister(0x30, 1);     // Falling: Noise Half Delta
  touch->writeRegister(0x31, 15);    // Falling: Noise Count Limit
  touch->writeRegister(0x32, 255);   // Falling: Filter Delay Count

  touch->writeRegister(0x33, 1);     // Touched: Maximum Half Delta
  touch->writeRegister(0x34, 1);     // Touched: Noise Half Delta
  touch->writeRegister(0x35, 63);    // Touched: Noise Count Limit
  touch->writeRegister(0x36, 255);   // Touched: Filter Delay Count

  touch->writeRegister(0x5B, 0x00);  // Touch and Release Debounce: No debounce
  touch->writeRegister(0x5C, 0xD0);  // AFE1: 34x averaging, 16uA current
  touch->writeRegister(0x5D, 0x20);  // AFE2: 0.5uS, 4x averaging, 1ms period
  // 4 samples * 1ms period = 4ms

  touch->writeRegister(0x7B, 0xCF);  // Autoconfig0: Enable & set baseline
  touch->writeRegister(0x7C, 0x00);  // Autoconfig1: No interrupts
  touch->writeRegister(0x7D, 200);   // Autoconfig Upper Side Limit
  touch->writeRegister(0x7E, 130);   // Autoconfig Lower Side Limit
  touch->writeRegister(0x7F, 180);   // Autoconfig Target Level

  for (uint8_t s = 0; s < 12; ++s) {
    touch->writeRegister(0x41 + s * 2, 5);  // Touch Threshold = 5
    touch->writeRegister(0x42 + s * 2, 2);  // Release Threshold = 2
  }

  touch->writeRegister(0x5E, 0xCC); // Electrode Configuration: Run 12 sensors
}

static uint32_t next_millis = 0;
static uint16_t frame = 0;
static int16_t last_century = -1;

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
  setup_touch(&touch);

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
    for (uint8_t y = 3 + (s / 6) * 7; y < 6 + (s / 6) * 7; ++y)
      for (uint8_t x = 2 + (s % 6) * 2; x < 4 + (s % 6) * 2; ++x)
        *pixel(&leds, x, y) = color;
  }

  const CRGB border(0, 0, 0);
  for (uint8_t x = 1; x < 15; ++x) {
    *pixel(&leds, x, 2) = border;
    *pixel(&leds, x, 6) = border;
    *pixel(&leds, x, 9) = border;
    *pixel(&leds, x, 13) = border;
  }
  for (uint8_t y = 3; y < 6; ++y) {
    *pixel(&leds, 1, y) = border;
    *pixel(&leds, 14, y) = border;
    *pixel(&leds, 1, 15 - y) = border;
    *pixel(&leds, 14, 15 - y) = border;
  }

  // for (uint8_t v = 0; v < 16; ++v) {
  //   pixel(&leds, v, 0)->setRGB(255, 0, 0);
  //   pixel(&leds, 0, v)->setRGB(0, 255, 0);
  // }
  // pixel(&leds, 0, 0)->setRGB(255, 255, 255);

  FastLED.show();

  const uint8_t watch_sensor = 0;
  const uint16_t base_value = touch.baselineData(watch_sensor);
  const uint16_t live_value = touch.filteredData(watch_sensor);
  const uint16_t century = base_value / 100 * 100;

  char out[100];
  if (century != last_century && last_century >= 0) {
    sprintf(out, "                 === %+d => %d ===",
            century - last_century, century);
    Serial.println(out);
  }
  last_century = century;

  sprintf(out, "#%1d %3d %+3d %+3d   ", watch_sensor, century,
          base_value - century, live_value - base_value);

  const char touch_ch = (touched & (1 << watch_sensor)) ? 'T' : ':';
  for (uint8_t p = 0; p < 50; ++p) {
    const uint16_t v = century + p * 2;
    const char ch =
        (v < base_value && v < live_value) ? ' ' :
        (v < base_value && v >= live_value) ? '-' :
        (v == base_value || v + 1 == base_value) ? touch_ch :
        (v > base_value && v < live_value) ? '+' :
        (v > base_value && v >= live_value) ? ' ' : '?';
    out[17 + p] = ch;
  }
  out[17 - 1] = '[';
  strcpy(out + 17 + 50, "]");
  Serial.println(out);

  ++frame;
  next_millis += 50;
  while (millis() < next_millis) {}
}
