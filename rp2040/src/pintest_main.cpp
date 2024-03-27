#include <algorithm>
#include <array>

#include <Arduino.h>
#include <NeoPixelBus.h>

#if ARDUINO_ARCH_ESP32
#include <esp_log.h>
#endif

#include "pg_cradle.h"

using namespace pg_cradle_io;

#if CONFIG_IDF_TARGET_ESP32
// Assume WT32-ETH01 board
constexpr std::array pins = {
  39, 36, 15, 14, 12, 35, 4, 2,
  CRADLE_X0, CRADLE_X1, CRADLE_X2, CRADLE_X3, CRADLE_X4, CRADLE_X5, CRADLE_X6,
  17, 5, 33, 32,
};
#elif CONFIG_IDF_TARGET_ESP32C3
constexpr std::array pins = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
#elif ARDUINO_ARCH_RP2040
constexpr std::array pins = {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
  10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
  20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
};
#endif

static std::array<char, pins.size()> pin_modes;
static std::array<bool, 4> leds;
static int pin_index = -1;
static char pending_char = 0;
static long next_millis = 0;

using NeoPixelBusType = NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>;
static NeoPixelBusType* strip = nullptr;
static int strip_index = -1;
static bool strip_spam = false;

void loop() {
  std::array<char[8], pins.size()> pin_text = {};

  for (size_t i = 0; i < pins.size(); ++i) {
    if (pins[i] - CRADLE_X0 >= pg_cradle.extra_gpio_count) {
      continue;  // Cradle pin that isn't present
    }

    auto const read = pg_cradle_io::digitalRead(pins[i]);
    char const* emoji;
    switch (pin_modes[i] | 0x20) {
      case 'i': emoji = read ? "🔼" : "⬇️"; break;
      case 'u': emoji = read ? "🎈" : "⤵️"; break;
      case 'd': emoji = read ? "⏫" : "💧"; break;
      case 'h': emoji = read ? "⚡" : "💀"; break;
      case 'l': emoji = read ? "💥" : "🇴"; break;
      case 'r': emoji = "🔴"; break;
      case 'g': emoji = "🟢"; break;
      case 'b': emoji = "🔵"; break;
      case 'w': emoji = "⚪"; break;
      case 'c': emoji = "🟦"; break;
      case 'm': emoji = "🟪"; break;
      case 'y': emoji = "🟨"; break;
      case 'k': emoji = "⚫"; break;
      case 'a': emoji = "🌈"; break;
      case 'z': emoji = "🦓"; break;
      default: emoji = "???"; break;
    }

    char pin_name[4];
    if (pins[i] >= CRADLE_X0) sprintf(pin_name, "X%d", pins[i] - CRADLE_X0);
    if (pins[i] < CRADLE_X0) sprintf(pin_name, "%d", pins[i]);
    sprintf(pin_text[i], "%s%c%d", pin_name, pin_modes[i], read);

    if (i > 0) Serial.print(" ");
    if (pin_index == i) Serial.print("[");
    Serial.printf("%s%s", pin_name, emoji);
    if (pin_index == i) Serial.print("]");
  }

  if (pg_cradle.status_led_count > 0) {
    Serial.print(" S");
    for (int i = 0; i < pg_cradle.status_led_count && i < leds.size(); ++i) {
      if (leds[i]) {
        switch (i) {
          case ORANGE_CRADLE_LED: Serial.print("🟠"); break;
          case GREEN_CRADLE_LED: Serial.print("🟢"); break;
          case BLUE_CRADLE_LED: Serial.print("🔵"); break;
          case WHITE_CRADLE_LED: Serial.print("⚪"); break;
          default: Serial.print("💡"); break;
        }
      } else {
        Serial.print("▪️");
      }
    }
  }

  if (pg_cradle.button_count > 0) {
    Serial.print(" B");
    for (int i = 0; i < pg_cradle.button_count; ++i) {
      Serial.print(pg_cradle_button(i) ? "🔘" : "⚫");
    }
  }

  if (pending_char > 0) {
    Serial.printf(" [%c]", pending_char);
  }

  Serial.print("\n");

  if (pg_cradle_screen) {
    for (size_t i = 0; i < pins.size(); i += 2) {
      if (i + 1 < pins.size()) {
        pg_cradle_printf((i / 2), 6, "%s %s", pin_text[i], pin_text[i + 1]);
      } else {
        pg_cradle_printf((i / 2), 6, "%s", pin_text[i]);
      }
    }
  }

  for (;;) {
    if (millis() > next_millis) {
      next_millis += 200;
      break;
    }

    if (Serial.available() > 0) {
      int const ch = Serial.read();
      if (ch == '\n') {
        if (!(pending_char >= '0' && pending_char <= '9')) {
          Serial.print("\n");  // Echo newline for use in marking a spot
          continue;
        }
      } else if (ch == '\r') {
        continue;  // Ignore CR which often comes with newline
      } else if (ch < 32 || ch >= 127) {
        Serial.printf("*** unknown char #%d\n\n", ch);
        pending_char = 0;
        continue;
      }

      // This has to come before the general pin-number 0-9 handling
      if (pending_char == 's') {
        pending_char = 0;
        int led;
        switch (ch | 0x20) {
          case 'o': case '0': led = ORANGE_CRADLE_LED; break;
          case 'g': case '1': led = GREEN_CRADLE_LED; break;
          case 'b': case '2': led = BLUE_CRADLE_LED; break;
          case 'w': case '3': led = WHITE_CRADLE_LED; break;
          default:
            Serial.printf("*** [S%c]: unknown status LED\n\n", ch);
            continue;
        }
        leds[led] = !leds[led];
        pg_set_cradle_led(led, leds[led] ? 100 : 0);
        break;
      }

      int input_pin = -1;
      if (ch >= '0' && ch <= '9') {
        if (pending_char == 'x') {
          input_pin = CRADLE_X0 + (ch - '0');
          pending_char = 0;
        } else if (pending_char >= '0' && pending_char <= '9') {
          input_pin = 10 * (pending_char - '0') + (ch - '0');
          pending_char = 0;
        } else {
          pending_char = ch;
          continue;
        }
      } else if (pending_char >= '0' && pending_char <= '9') {
        // Finish numeric input with any non-numeric key ('i', newline, etc)
        input_pin = pending_char - '0';
        pending_char = 0;
      }

      if (input_pin >= 0) {
        auto const found = std::find(pins.begin(), pins.end(), input_pin);
        if (input_pin >= CRADLE_X0 + pg_cradle.extra_gpio_count) {
          Serial.printf("*** [X%d]: no such pin\n\n", input_pin - CRADLE_X0);
          pin_index = -1;
        } else if (found == pins.end()) {
          Serial.printf("*** [%d]: no such pin\n\n", input_pin);
          pin_index = -1;
        } else {
          pin_index = found - pins.begin();
        }
        // Allow numbers to be terminated with another command (eg. "3i")
        if ((ch >= '0' && ch <= '9') || ch == '\n') break;
      }

      if ((ch | 0x20) == 'x') {
        pending_char = 'x';
        break;
      }

      if ((ch | 0x20) == 's') {
        pending_char = 's';
        break;
      }

      // All multichar sequences are handled above
      pending_char = 0;

      if (ch == '?' || ch == '/') {
        Serial.print(
           "\n<<< ? >>> help:\n"
           "  00-nn: select pin\n"
           "  X + 0-n: select cradle extra pin\n"
           "  i: set pin to INPUT\n"
           "  u: set pin to INPUT_PULLUP\n"
           "  d: set pin to INPUT_PULLDOWN\n"
           "  h: set pin to OUTPUT and HIGH\n"
           "  l: set pin to OUTPUT and LOW\n"
           "  [rgbw/cmyk/az]: drive LED strip (lower=once, UPPER=repeat)\n"
           "  S + [0123/ogbw]: toggle cradle status LED\n"
           "\n"
        );
        break;
      }

      int rgb = -1;
      switch (ch | 0x20) {
        case 'r': rgb = 0xFF0000; break;
        case 'g': rgb = 0x00FF00; break;
        case 'b': rgb = 0x0000FF; break;
        case 'w': rgb = 0xFFFFFF; break;
        case 'c': rgb = 0x00FFFF; break;
        case 'm': rgb = 0xFF00FF; break;
        case 'y': rgb = 0xFFFF00; break;
        case 'k': rgb = 0x000000; break;
        case 'a': rgb = 0x000000; break;
        case 'z': rgb = 0x000000; break;
      }

      if (rgb >= 0) {
        if (pin_index < 0) {
          Serial.printf("*** [%c]: no pin selected for LED strip\n\n", ch);
          continue;
        }
        if (pins[pin_index] >= CRADLE_X0) {
          Serial.printf("*** [%c]: can't use XGPIO pin for LED strips\n\n", ch);
          continue;
        }

        if (strip_index != pin_index) {
          if (strip != nullptr) {
            delete strip;
            pin_modes[strip_index] = 'i';
            pg_cradle_io::pinMode(pins[strip_index], INPUT);
          }
          strip_index = pin_index;
          strip = new NeoPixelBusType(768, pins[pin_index]);
          strip->Begin();
        }

        if (ch == 'a' || ch == 'A') {
          int const pixels = strip->PixelCount();
          for (int p = 0; p < pixels; ++p) {
            int const c = (p * 10) % 768;
            if (c < 256) {
              strip->SetPixelColor(p, RgbColor(c, 255 - c, 0));
            } else if (c < 512) {
              strip->SetPixelColor(p, RgbColor(0, c - 256, 511 - c));
            } else {
              strip->SetPixelColor(p, RgbColor(767 - c, 0, c - 512));
            }
          }
        } else if (ch == 'z' || ch == 'Z') {
          int const pixels = strip->PixelCount();
          for (int p = 0; p < pixels; ++p) {
            if (p % 32 < 16) {
              strip->SetPixelColor(p, RgbColor(255, 255, 255));
            } else {
              strip->SetPixelColor(p, RgbColor(0, 0, 0));
            }
          }
        } else {
          strip->ClearTo(HtmlColor(rgb));
        }

        strip->Show();
        strip_spam = (ch < 'a');
        pin_modes[pin_index] = ch;
        break;
      }

      PinMode new_mode;
      switch (ch | 0x20) {
        case 'i': new_mode = INPUT; break;
        case 'u': new_mode = INPUT_PULLUP; break;
        case 'd': new_mode = INPUT_PULLDOWN; break;
        case 'h': new_mode = OUTPUT; break;
        case 'l': new_mode = OUTPUT; break;
        default:
          Serial.printf("*** unknown key [%c]\n\n", ch);
          continue;
      }
      if (pin_index < 0) {
        Serial.printf("*** [%c]: no pin selected\n\n", ch);
        continue;
      }

      if (strip_index == pin_index) {
        delete strip;
        strip = nullptr;
        strip_index = -1;
        strip_spam = false;
      }

      pin_modes[pin_index] = ch | 0x20;
      pg_cradle_io::pinMode(pins[pin_index], new_mode);
      if (new_mode == OUTPUT) {
        auto const new_status = (ch | 0x20) == 'h' ? HIGH : LOW;
        pg_cradle_io::digitalWrite(pins[pin_index], new_status);
      }
      break;
    }

    if (strip_spam && strip != nullptr && strip->CanShow()) {
      // Always rotate the buffer (though it only matters for the rainbow)
      uint8_t* const buf = strip->Pixels();
      int const buf_size = strip->PixelCount() * 3;
      uint8_t temp[3];
      memcpy(temp, buf + buf_size - 3, 3);
      memmove(buf + 3, buf, buf_size - 3);
      memcpy(buf, temp, 3);
      strip->Dirty();
      strip->Show();
    }

    delay(10);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  while (!Serial) delay(1);
  Serial.print("\n\n### PIN TEST START 🔌 ###\n");
#ifdef ESP_LOG_VERBOSE
  esp_log_level_set("*", ESP_LOG_VERBOSE);
#endif
  Serial.print("🧺 CRADLE SETUP\n");
  Serial.flush();
  pg_detect_cradle();
  pg_cradle_printf(0, 8 + CRADLE_TEXT_BOLD, "PinTest");
  pg_cradle_printf(1, 6, "Starting...");

  for (size_t i = 0; i < pins.size(); ++i) {
    pin_modes[i] = 'i';
    pg_cradle_io::pinMode(pins[i], INPUT);
  }

  next_millis = millis();
}
