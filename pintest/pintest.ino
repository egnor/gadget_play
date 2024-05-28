#include <algorithm>
#include <array>
#include <cctype>

#include <Arduino.h>
// #include <NeoPixelBus.h>
#include <Wire.h>

#if ARDUINO_ARCH_ESP32
#include <esp_log.h>
#endif

// Specific boards
#if defined(ARDUINO_ADAFRUIT_FEATHER_ESP32_V2)
constexpr std::array<int, 23> pins = {
  26, 25, 34, 39, 36, 4, 5, 19, 21, 7, 8, 37,
  13, 12, 27, 33, 15, 32, 14, 20, 22,
  PIN_NEOPIXEL, NEOPIXEL_I2C_POWER,
};
#elif defined(ARDUINO_WT32_ETH01)
constexpr std::array pins = {
  39, 36, 15, 14, 12, 35, 4, 2, 17, 5, 33, 32,
};

// General microcontrollers
#elif defined(ARDUINO_ARCH_ESP32) && NUM_DIGITAL_PINS == 22  // ESP32-C3
constexpr std::array<int, 13> pins = {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 20, 21,
  // Assume 18 and 19 are USB D- and D+
};
#elif defined(ARDUINO_ARCH_RP2040)
constexpr std::array pins = {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
  10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
  20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
};
#else

#error "Unsupported board"
#endif

#if defined(ARDUINO_ARCH_RP2040)
SerialPIO* make_software_serial(int tx, int rx, int baud) {
  auto* pio = new SerialPIO(tx, rx);
  pio->begin(baud);
  return pio;
}
#else
#include <SoftwareSerial.h>
SoftwareSerial* make_software_serial(int tx, int rx, int baud) {
  auto* ss = new SoftwareSerial();
  ss->begin(baud, SWSERIAL_8N1, rx, tx);
  return ss;
}
#endif

static std::array<char, pins.size()> pin_modes;
static int pin_index = -1;
static char pending_char = 0;
static long next_millis = 0;

// using NeoPixelBusType = NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>;
// static NeoPixelBusType* strip = nullptr;
static int strip_index = -1;
static bool strip_spam = false;

static int sda_pin = SDA;
static int scl_pin = SCL;

static int rx_pin = -1;
static int tx_pin = -1;

void run_i2c_scan() {
  Wire.begin();

  int found = 0;
  for (int address = 1; address < 127; ++address) {
    Serial.printf("Scanning 0x%02X...\r", address);
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.printf("Found device at 0x%02X\n", address);
      ++found;
    }
  }
  Serial.printf("I2C scan complete, %d devices found\n\n", found);
  Wire.end();
}

void run_uart_terminal() {
  auto* port = make_software_serial(tx_pin, rx_pin, 9600);

  char last_dat = 0, last_key = 0;
  bool keep_running = true;
  while (keep_running) {
    int const dat_n = std::min(port->available(), Serial.availableForWrite());
    for (int i = 0; i < dat_n; ++i) {
      auto const dat = port->read();
      if (dat < 0) {
        break;  // Sometimes we get spurious available() returns?
      } else if (dat >= 0x20 && dat < 0x7F) {
        Serial.write(dat);
      } else if (dat == '\r' || dat == '\n') {
        if (!(last_dat == '\r' && dat == '\n')) {
          Serial.print("\r\n");  // Turn any CR / LF / CR+LF into CR+LF
        }
      } else {
        Serial.printf("<%02X>", dat);
      }
      last_dat = dat;
    }

    int const key_n = std::min(Serial.available(), port->availableForWrite());
    for (int i = 0; i < key_n; ++i) {
      auto const key = Serial.read();
      if (key == 'U' - 0x40) {
        Serial.print(
           "\n<<< ctrl-U >>> UART terminal menu\n"
           "  [9]600, [1]9200, [3]8400, [5]7600, 11520[0], [2]30400: set baud\n"
           "  u: send literal ctrl-U\n"
           "  q, x: exit UART terminal\n"
           "  enter, escape: never mind\n"
           "\n==> "
        );
      } else if (last_key == 'U' - 0x40) {
        int new_baud = 0;
        switch (toupper(key)) {
          case '9': new_baud = 9600; break;
          case '1': new_baud = 19200; break;
          case '3': new_baud = 38400; break;
          case '5': new_baud = 57600; break;
          case '0': new_baud = 115200; break;
          case '2': new_baud = 230400; break;
          case 'Q':
          case 'X':
            Serial.printf("[%c] exit terminal\n", key);
            keep_running = false;
            break;

          case 'U':
            Serial.printf("[%c] send ctrl-U\n", key);
            port->write('U' - 0x40);
            break;

          case '\r':
          case '\n':
          case '\e':
            Serial.print("Never mind!\n");
            break;

          default:
            if (key > 0 && key < 0x20) {
              Serial.printf("*** Bad key ctrl-%c\n", key + 0x40);
            } else if (key < 0x7F) {
              Serial.printf("*** Bad key [%c]\n", key);
            } else {
              Serial.printf("*** Bad key #%d\n", key);
            }
            break;
        }
       
        if (new_baud > 0) {
          port->end();
          port->begin(new_baud);
          Serial.printf("[%c] set baud to %d\n", key, new_baud);
        }

        Serial.print("\n");
      } else if (key != '\r' && key != '\n') {
        port->write(key);
      } else if (!(last_key == '\r' && key == '\n')) {
        port->print("\r\n");
      }
      last_key = key;
    }

    delay(5);
  }

  port->end();
  delete port;
}

void loop() {
  std::array<char[8], pins.size()> pin_text = {};

  for (size_t i = 0; i < pins.size(); ++i) {
    auto const read = digitalRead(pins[i]);
    char const* emoji;
    switch (toupper(pin_modes[i])) {
      case 'I': emoji = read ? "🔼" : "⬇️"; break;
      case 'U': emoji = read ? "🎈" : "⤵️"; break;
      case 'D': emoji = read ? "⏫" : "💧"; break;
      case 'H': emoji = read ? "⚡" : "💀"; break;
      case 'L': emoji = read ? "💥" : "🇴"; break;
      case 'R': emoji = "🔴"; break;
      case 'G': emoji = "🟢"; break;
      case 'B': emoji = "🔵"; break;
      case 'W': emoji = "⚪"; break;
      case 'C': emoji = "🟦"; break;
      case 'M': emoji = "🟪"; break;
      case 'Y': emoji = "🟨"; break;
      case 'K': emoji = "⚫"; break;
      case 'A': emoji = "🌈"; break;
      case 'Z': emoji = "🦓"; break;
      default: emoji = "???"; break;
    }

    char pin_name[4];
    sprintf(pin_name, "%d", pins[i]);
    sprintf(pin_text[i], "%s%c%d", pin_name, pin_modes[i], read);

    if (i > 0) Serial.print(" ");
    if (pin_index == i) Serial.print("[");
    Serial.printf("%s%s", pin_name, emoji);
    if (scl_pin == pins[i]) Serial.print("⏱️");
    if (sda_pin == pins[i]) Serial.print("📊");
    if (rx_pin == pins[i]) Serial.print("🗨️");
    if (tx_pin == pins[i]) Serial.print("💬");
    if (pin_index == i) Serial.print("]");
  }

  if (pending_char > 0) {
    Serial.printf(" [%c]", pending_char);
  }

  Serial.print("\n");

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
      } else if (ch <= 0 || ch >= 127) {
        Serial.printf("*** Bad char #%d (? for help)\n\n", ch);
        pending_char = 0;
        continue;
      }

      int input_pin = -1;
      if (ch >= '0' && ch <= '9') {
        if (pending_char >= '0' && pending_char <= '9') {
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
        if (found == pins.end()) {
          Serial.printf("*** [%d]: no such pin\n\n", input_pin);
          pin_index = -1;
        } else {
          pin_index = found - pins.begin();
        }
        // Allow numbers to be terminated with another command (eg. "3i")
        if ((ch >= '0' && ch <= '9') || ch == '\n') break;
      }

      // All multichar sequences are handled above
      pending_char = 0;

      if (ch == '?' || ch == '/') {
        Serial.print(
           "\n<<< ? >>> help:\n"
           "  00-nn: select pin\n"
           "  i: set pin to INPUT (🔼/⬇️ for high/low)\n"
           "  u: set pin to INPUT_PULLUP (🎈, ⤵️ if driven low)\n"
           "  d: set pin to INPUT_PULLDOWN (💧, ⏫ if driven high)\n"
           "  h: set pin to OUTPUT and HIGH (⚡, 💀 on conflict)\n"
           "  l: set pin to OUTPUT and LOW (🇴, 💥 on conflict)\n"
           "  [rgbw/cmyk/az]: drive LED strip (lower=once, UPPER=repeat)\n"
           "  ctrl-C: use selected pin for I2C SCL (⏱️)\n"
           "  ctrl-D: use selected pin for I2C SDA (📊)\n"
           "  ctrl-I: run I2C scanner\n"
           "  ctrl-R: use selected pin for UART RX (⬅️)\n"
           "  ctrl-X: use selected pin for UART TX (➡️)\n"
           "  ctrl-U: run UART terminal\n"
           "\n"
        );
        break;
      }

      if (ch < 0x20) {
        auto const ctrl = ch + 0x40;
        switch (ctrl) {
          case 'C':
            if (pin_index < 0) {
              Serial.printf("*** ctrl-C (set I2C SCL): select a pin first\n\n");
            } else {
              scl_pin = pins[pin_index];
            }
            break;

          case 'D':
            if (pin_index < 0) {
              Serial.printf("*** ctrl-D (set I2C SDA): select a pin first\n\n");
            } else {
              sda_pin = pins[pin_index];
            }
            break;

          case 'I':
            if (sda_pin < 0 || scl_pin < 0) {
              Serial.printf("*** ctrl-I (I2C scan): need SDA/SCL pins\n\n");
            } else if (sda_pin == scl_pin) {
              Serial.printf("*** ctrl-I (I2C scan): SDA must != SCL\n\n");
            } else if (
#if defined(ARDUINO_ARCH_RP2040)
                !Wire.setSDA(sda_pin) || !Wire.setSCL(scl_pin)
#else
                !Wire.setPins(sda_pin, scl_pin)
#endif
            ) {
              Serial.printf(
                "*** ctrl-I (I2C scan): bad SDA/SCL p%d/p%d\n\n",
                sda_pin, scl_pin
              );
            } else {
              Serial.printf("<<< ctrl-I >>> I2C scan\n");
              run_i2c_scan();
              next_millis = millis();
            }
            break;

          case 'R':
            if (pin_index < 0) {
              Serial.printf("*** ctrl-R (set UART RX): select a pin first\n\n");
            } else {
              rx_pin = pins[pin_index];
            }
            break;

          case 'X':
            if (pin_index < 0) {
              Serial.printf("*** ctrl-X (set UART TX): select a pin first\n\n");
            } else {
              tx_pin = pins[pin_index];
            }
            break;

          case 'U':
            if (rx_pin < 0 || tx_pin < 0) {
              Serial.printf("*** ctrl-U (UART terminal): need RX/TX pins\n\n");
            } else if (rx_pin == tx_pin) {
              Serial.printf("*** ctrl-U (UART terminal): RX must != TX\n\n");
            } else {
              Serial.printf("<<< ctrl-U >>> UART terminal (ctrl-U for menu)\n");
              run_uart_terminal();
              next_millis = millis();
            }
            break;

          default:
            Serial.printf("*** Bad key ctrl-%c (? for help)\n\n", ctrl);
            break;
        }
        break;
      }

      int rgb = -1;
      switch (toupper(ch)) {
        case 'R': rgb = 0xFF0000; break;
        case 'G': rgb = 0x00FF00; break;
        case 'B': rgb = 0x0000FF; break;
        case 'W': rgb = 0xFFFFFF; break;
        case 'C': rgb = 0x00FFFF; break;
        case 'M': rgb = 0xFF00FF; break;
        case 'Y': rgb = 0xFFFF00; break;
        case 'K': rgb = 0x000000; break;
        case 'A': rgb = 0x000000; break;
        case 'Z': rgb = 0x000000; break;
      }

      if (rgb >= 0) {
        if (pin_index < 0) {
          Serial.printf("*** [%c]: no pin selected for LED strip\n\n", ch);
          continue;
        }

/*
        if (strip_index != pin_index) {
          if (strip != nullptr) {
            delete strip;
            pin_modes[strip_index] = 'i';
            pinMode(pins[strip_index], INPUT);
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
*/
        strip_spam = (ch < 'a');
        pin_modes[pin_index] = ch;
        break;
      }

      int new_mode;
      switch (toupper(ch)) {
        case 'I': new_mode = INPUT; break;
        case 'U': new_mode = INPUT_PULLUP; break;
        case 'D': new_mode = INPUT_PULLDOWN; break;
        case 'H': new_mode = OUTPUT; break;
        case 'L': new_mode = OUTPUT; break;
        default:
          Serial.printf("*** Bad key [%c] (? for help)\n\n", ch);
          continue;
      }
      if (pin_index < 0) {
        Serial.printf("*** [%c]: no pin selected\n\n", ch);
        continue;
      }

/*
      if (strip_index == pin_index) {
        delete strip;
        strip = nullptr;
        strip_index = -1;
        strip_spam = false;
      }
*/

      // Avoid glitching with unnecessary pinMode() calls
      auto const new_mode_ch = toupper(ch);
      if (pin_modes[pin_index] != new_mode_ch) {
        pin_modes[pin_index] = new_mode_ch;
        pinMode(pins[pin_index], new_mode);
      }
      if (new_mode == OUTPUT) {
        auto const new_status = toupper(ch) == 'H' ? HIGH : LOW;
        digitalWrite(pins[pin_index], new_status);
      }
      break;
    }

/*
    if (strip_spam && strip != nullptr && strip->CanShow()) {
      // Always rotate the LED buffer (though it only matters for the rainbow)
      uint8_t* const buf = strip->Pixels();
      int const buf_size = strip->PixelCount() * 3;
      uint8_t temp[3];
      memcpy(temp, buf + buf_size - 3, 3);
      memmove(buf + 3, buf, buf_size - 3);
      memcpy(buf, temp, 3);
      strip->Dirty();
      strip->Show();
    }
*/

    delay(10);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  while (!Serial) delay(1);
  Serial.print("\n\n### PIN TEST START 🔌 ###\n");
  delay(500);
#ifdef ESP_LOG_VERBOSE
  esp_log_level_set("*", ESP_LOG_VERBOSE);
#endif
  Serial.flush();

  for (size_t i = 0; i < pins.size(); ++i) {
    Serial.printf("📍 Pin %d => INPUT\n", pins[i]);
    Serial.flush();
    delay(100);
    pin_modes[i] = 'i';
    pinMode(pins[i], INPUT);
  }

  next_millis = millis();
}
