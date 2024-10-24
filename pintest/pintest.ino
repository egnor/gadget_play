#include <algorithm>
#include <array>
#include <cctype>

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <Wire.h>

#if ARDUINO_ARCH_ESP32
#include <esp_log.h>
#endif

// Specific boards
#if defined(ARDUINO_ADAFRUIT_FEATHER_ESP32_V2)
static int sda_pin = SDA, scl_pin = SCL, rx_pin = RX, tx_pin = TX;
constexpr std::array<int, 24> pins = {
  26, 25, 34, 39, 36, 4, 5, 19, 21, 7, 8, 37,
  13, 12, 27, 33, 15, 32, 14, 20, 22,
  PIN_NEOPIXEL, NEOPIXEL_I2C_POWER, BUTTON,
};

#elif defined(ARDUINO_QUALIA_S3_RGB666)
static int sda_pin = SDA, scl_pin = SCL, rx_pin = -1, tx_pin = -1;
constexpr std::array pins = {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 21,
  38, 39, 40, 41, 42, 45, 46, 47, 48,
};

#elif defined(ARDUINO_WT32_ETH01)
static int sda_pin = SDA, scl_pin = SCL, rx_pin = -1, tx_pin = -1;
constexpr std::array pins = {
  39, 36, 15, 14, 12, 35, 4, 2, 17, 5, 33, 32,
};

// General microcontrollers
#elif CONFIG_IDF_TARGET_ESP32
static int sda_pin = -1, scl_pin = -1, rx_pin = -1, tx_pin = -1;
constexpr std::array pins = {
  // pins 1 and 3 are UART0 TX and RX; don't step on the console
  // pins 6-11 are internal flash; using them will crash
  0, 2, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19,
  21, 22, 23, 25, 26, 27, 32, 33, 34, 35, 36, 39
};

#elif CONFIG_IDF_TARGET_ESP32C3
static int sda_pin = SDA, scl_pin = SCL, rx_pin = -1, tx_pin = -1;
constexpr std::array<int, 13> pins = {
  // Assume 18 and 19 are USB D- and D+
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 20, 21,
};

#elif CONFIG_IDF_TARGET_ESP32S2
static int sda_pin = SDA, scl_pin = SCL, rx_pin = -1, tx_pin = -1;
constexpr std::array<int, 34> pins = {
  // 19 and 20 are USB D- and D+
  // 26 is used by internal PSRAM
   0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
  10, 11, 12, 13, 14, 15, 16, 17, 18,
      21,
              33, 34, 35, 36, 37, 38, 39,
  40, 41, 42, 43, 44, 45, 46,
};

#elif CONFIG_IDF_TARGET_ESP32S3
static int sda_pin = SDA, scl_pin = SCL, rx_pin = -1, tx_pin = -1;
constexpr std::array pins = {
  // 19 and 20 are USB D- and D+
   0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
  10, 11, 12, 13, 14, 15, 16, 17, 18,
      21,                         38, 39,
  40, 41, 42, 43, 44, 45, 46, 47, 48,
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

static std::array<char, pins.size()> pin_chars;
static int pin_index = -1;
static char pending_char = 0;
static long next_millis = 0;

static Adafruit_NeoPixel* leds = nullptr;
static bool led_spam = false;
static int led_pin = -1;

void run_i2c_scan() {
  Wire.begin();

#if defined(ARDUINO_ARCH_ESP32)
  // turn on internal pullups in case it helps!
  gpio_set_pull_mode((gpio_num_t) sda_pin, GPIO_PULLUP_ONLY);
  gpio_set_pull_mode((gpio_num_t) scl_pin, GPIO_PULLUP_ONLY);
#endif

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

    // SoftwareSerial.availableForWrite() doesn't work; just let it block
    int const key_n = Serial.available();
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
#if defined(ARDUINO_ARCH_ESP32)
    // workaround for https://github.com/espressif/arduino-esp32/issues/10370
    auto const read = gpio_get_level((gpio_num_t) pins[i]);
#else
    auto const read = digitalRead(pins[i]);
#endif
    char const* emoji;
    switch (toupper(pin_chars[i])) {
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
      case 'O': emoji = "🟠"; break;
      case 'A': emoji = "🌈"; break;
      case 'Z': emoji = "🦓"; break;
      default: emoji = "???"; break;
    }

    char pin_name[4];
    sprintf(pin_name, "%d", pins[i]);
    sprintf(pin_text[i], "%s%c%d", pin_name, pin_chars[i], read);

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
           "  ctrl-R: use selected pin for UART RX (🗨️)\n"
           "  ctrl-X: use selected pin for UART TX (💬)\n"
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
              for (size_t i = 0; i < pins.size(); ++i) {
                if (pins[i] == sda_pin || pins[i] == scl_pin) {
                  pin_chars[i] = 'i';
                  pinMode(pins[i], INPUT);
                }
              }
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
        case 'O': rgb = 0xFF4400; break;
        case 'A': rgb = 0x000000; break;
        case 'Z': rgb = 0x000000; break;
      }

      if (rgb >= 0) {
        if (pin_index < 0) {
          Serial.printf("*** [%c]: no pin selected for LED strip\n\n", ch);
          continue;
        }

        if (led_pin != pins[pin_index] || leds == nullptr) {
          if (leds != nullptr) {
            Serial.printf("=== p%d LED strip driver stopping\n", led_pin);
            delete leds;
            for (size_t i = 0; i < pins.size(); ++i) {
              if (pins[i] == led_pin) {
                pin_chars[i] = 'i';
                pinMode(pins[i], INPUT);
              }
            }
          }

          Serial.printf("=== p%d LED strip driver starting\n", pins[pin_index]);
          led_pin = pins[pin_index];;
          leds = new Adafruit_NeoPixel(32, led_pin, NEO_GRB + NEO_KHZ800);
          leds->begin();
        }

        if (ch == 'a' || ch == 'A') {
          Serial.printf("<<< A >>> p%d: LED rainbow\n", pins[pin_index]);

          int const pixels = leds->numPixels();
          for (int p = 0; p < pixels; ++p) {
            int const c = (p * 10) % 768;
            if (c < 256) {
              leds->setPixelColor(p, c, 255 - c, 0);
            } else if (c < 512) {
              leds->setPixelColor(p, 0, c - 256, 511 - c);
            } else {
              leds->setPixelColor(p, 767 - c, 0, c - 512);
            }
          }
        } else if (ch == 'z' || ch == 'Z') {
          Serial.printf("<<< Z >>> p%d: LED zebra\n", pins[pin_index]);

          int const pixels = leds->numPixels();
          for (int p = 0; p < pixels; ++p) {
            if (p % 32 < 16) {
              leds->setPixelColor(p, 255, 255, 255);
            } else {
              leds->setPixelColor(p, 0, 0, 0);
            }
          }
        } else {
          Serial.printf(
              "<<< %c >>> p%d: LED #%06x\n", ch, pins[pin_index], rgb);
          leds->fill(rgb);
        }

        Serial.flush();
        leds->show();
        led_spam = (ch < 'a');
        pin_chars[pin_index] = ch;
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

      if (led_pin == pins[pin_index]) {
        Serial.printf("=== p%d LED strip driver stopping\n", led_pin);
        delete leds;
        leds = nullptr;
      }

      // Avoid glitching with unnecessary pinMode() calls
      auto const new_mode_ch = toupper(ch);
      if (pin_chars[pin_index] != new_mode_ch) {
        pin_chars[pin_index] = new_mode_ch;
        pinMode(pins[pin_index], new_mode);
      }
      if (new_mode == OUTPUT) {
        auto const new_status = toupper(ch) == 'H' ? HIGH : LOW;
        digitalWrite(pins[pin_index], new_status);
      }
      break;
    }

    if (led_spam && leds != nullptr && leds->canShow()) {
      // Always rotate the LED buffer (though it only matters for the rainbow)
      uint8_t* const buf = leds->getPixels();
      int const buf_size = leds->numPixels() * 3;
      uint8_t temp[3];
      memcpy(temp, buf + buf_size - 3, 3);
      memmove(buf + 3, buf, buf_size - 3);
      memcpy(buf, temp, 3);
      leds->show();
    }

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
  while (Serial.available() > 0) Serial.read();

  for (size_t i = 0; i < pins.size(); ++i) {
    Serial.printf("📍 p%d => INPUT\n", pins[i]);
    Serial.flush();
    delay(20);
    pin_chars[i] = 'i';
    pinMode(pins[i], INPUT);
  }

  next_millis = millis();
}
