#include "pg_cradle.h"

#include <algorithm>
#include <cstdarg>

#include <Arduino.h>
#include <Wire.h>

#if ARDUINO_ARCH_ESP32
#include <esp_log.h>
#include <U8g2lib.h>
#define HAVE_U8G2 1
#else
#define log_i(fmt, ...) Serial.printf("ℹ️ " fmt "\n", ##__VA_ARGS__)
#define log_e(fmt, ...) Serial.printf("🔥 " fmt "\n", ##__VA_ARGS__)
#endif

static constexpr pg_cradle_status cradle_v8_1 = { 32, 33, 5, 4, 0, true };
static constexpr pg_cradle_status cradle_v8_2 = { 17, 5, 7, 4, 3, true };
static constexpr pg_cradle_status cradle_v9 = { 12, 2, 5, 4, 3, false };

static constexpr int NORMAL_LED_POWER[] = {8, 1, 16, 4};
static constexpr int MAX_LED_POWER = 137;  // 20mA if full scale 255 is 37mA

static constexpr int XGPIO_ADDR = 0x5B;
static constexpr int SCREEN_ADDR = 0x3C;

static constexpr int LINE_CHARS = 16;  // 32px wide
static constexpr int LINE_COUNT = 11;  // 64px high

struct cradle_line {
  int style = 0;
  char text[LINE_CHARS + 1] = "";
};

pg_cradle_status pg_cradle = {};
pg_cradle_screen_driver* pg_cradle_screen = nullptr;
static cradle_line lines[LINE_COUNT] = {};
static bool first_print = true;

static uint8_t xgpio_output = 0x00;  // All LOW to start
static uint8_t xgpio_dir = 0xFF;     // All INPUT to start
static uint8_t xgpio_mode = 0xFF;    // All GPIO to start
static uint8_t led_power[4] = {};    // Status LEDs OFF to start

static void draw_pg_logo() {
#if HAVE_U8G2
  auto* const s = pg_cradle_screen;
  auto const w = s->getDisplayWidth(), h = s->getDisplayHeight();
  s->setDrawColor(1);
  s->drawBox(4, 6, 16, 16);
  s->drawBox(20, 22, 8, 8);
  s->drawFrame(4, 6, 24, 24);
  s->setFont(u8g2_font_squeezed_b7_tr);
  s->drawStr(w / 2 - s->getStrWidth("Palace") / 2, 46, "Palace");
  s->drawStr(w / 2 - s->getStrWidth("Games") / 2, 58, "Games");
  s->sendBuffer();
#endif
}

void pg_detect_cradle() {
  if (pg_cradle.scl_pin >= 0) {
    log_e("Repeated cradle detection, ABORTING");
    abort();
  }

  // Initialize as nothing detected, modify if we do find cradle hardware
  pg_cradle.scl_pin = 0;
  pg_cradle.sda_pin = 0;
  pg_cradle.extra_gpio_count = 0;
  pg_cradle.status_led_count = 0;
  pg_cradle.button_count = 0;

#if !ARDUINO_ARCH_ESP32
  log_i("Not ESP32 => no power cradle");
  return;
#elif !CONFIG_IDF_TARGET_ESP32
  log_i("ESP32-Cx/Sx/etc variant => no power cradle");
  return;
#else
  log_i("Probing for power cradle");
  for (auto const probe : {cradle_v8_1, cradle_v8_2, cradle_v9}) {
    bool probe_fail = false;

    // Check that I2C lines can be driven low but are pulled high
    for (int const check_pin : {probe.scl_pin, probe.sda_pin}) {
      ::pinMode(check_pin, OUTPUT);
      digitalWrite(check_pin, LOW);
      bool const check_low = digitalRead(check_pin);
      ::pinMode(check_pin, probe.has_pullups ? INPUT : INPUT_PULLUP);
      bool const check_high = digitalRead(check_pin);
      if (check_low) {
        log_i("Can't pull p%d LOW => not I2C", check_pin);
        probe_fail = true;
        break;
      }
      if (!check_high) {
        log_i("No p%d HIGH float => not I2C", check_pin);
        probe_fail = true;
        break;
      }
    }

    if (probe_fail) continue;

    // Set up cradle I2C (GPIO expander, screen, Qwiic) as the default I2C bus
    Wire.begin(probe.sda_pin, probe.scl_pin);
    pg_cradle.scl_pin = probe.scl_pin;
    pg_cradle.sda_pin = probe.sda_pin;

    if (probe.extra_gpio_count > 0) {
      // Check for the GPIO expander
      Wire.beginTransmission(XGPIO_ADDR);
      Wire.write(0x10);  // Chip ID
      auto const gpio_error = Wire.endTransmission();
      if (gpio_error == 2) {
        log_i("XGPIO (AW9523) NOT found");
      } else if (gpio_error != 0) {
        log_e("I2C error #%d probing XGPIO (AW9523)", gpio_error);
      } else if (Wire.requestFrom(XGPIO_ADDR, 1) != 1) {
        log_e("Error reading XGPIO (AW9523) chip ID");
      } else {
        auto const chip_id = Wire.read();
        if (chip_id != 0x23) {
          log_e("Bad XGPIO (AW9523) chip ID 0x%02X (!=0x23)", chip_id);
        } else {
          log_i("XGPIO (AW9523) found");
          pg_cradle.extra_gpio_count = probe.extra_gpio_count;
          pg_cradle.status_led_count = probe.status_led_count;
          pg_cradle.button_count = probe.button_count;

          Wire.beginTransmission(XGPIO_ADDR);
          Wire.write(0x02);
          Wire.write(xgpio_output);  // 0x02 (Output_Port0)
          Wire.write(0xFF);          // 0x03 (Output_Port1): default
          Wire.write(xgpio_dir);     // 0x04 (Config_Port0)
          Wire.write(0xFF);          // 0x05 (Config_Port1): all INPUT
          Wire.endTransmission();

          Wire.beginTransmission(XGPIO_ADDR);
          Wire.write(0x11);
          Wire.write(0x10);        // 0x11 (GCR): set P0 to Push-Pull mode
          Wire.write(xgpio_mode);  // 0x12 (Mode_Port0)
          Wire.write(0x00);        // 0x13 (Mode_Port1): all LED
          Wire.endTransmission();

          Wire.beginTransmission(XGPIO_ADDR);
          Wire.write(0x2C);
          Wire.write(0x01);  // 0x2C (DIM11): min current for button pullup
          Wire.write(0x01);  // 0x2D (DIM12): same
          Wire.write(0x01);  // 0x2E (DIM13): same
          Wire.write(0x01);  // 0x2F (DIM14): same
          Wire.endTransmission();
        }
      }
    }

    Wire.beginTransmission(SCREEN_ADDR);
    auto const screen_error = Wire.endTransmission();
    if (screen_error == 2) {
      log_i("Screen (SSD1306) NOT found");
    } else if (screen_error != 0) {
      log_e("I2C error #%d probing screen (SSD1306)", screen_error);
    } else {
      pg_cradle_screen = new pg_cradle_screen_driver(U8G2_R1);
      if (pg_cradle_screen->begin()) {
        log_i("Screen (SSD1306) found");
        pg_cradle_screen->clearDisplay();
        pg_cradle_screen->setPowerSave(0);
        draw_pg_logo();
      } else {
        log_e("Error initializing screen (SSD1306)");
        delete pg_cradle_screen;
        pg_cradle_screen = nullptr;
      }
    }

    if (pg_cradle.extra_gpio_count > 0 || pg_cradle_screen != nullptr) {
      return;  // Found at least some hardware!
    }

    log_i("No cradle at scl=%d sda=%d", probe.scl_pin, probe.sda_pin);
    Wire.end();
    ::pinMode(probe.scl_pin, INPUT);
    ::pinMode(probe.sda_pin, INPUT);
    pg_cradle.scl_pin = 0;
    pg_cradle.sda_pin = 0;
  }

  log_i("No power cradle found");
#endif  // check for appropriate ESP32 variant
}

namespace pg_cradle_io {
  void pinMode(uint8_t pin, PinMode mode) {
    if (pin < CRADLE_X0) {
      ::pinMode(pin, mode);
      return;
    }
    if (pin >= CRADLE_X0 + pg_cradle.extra_gpio_count) {
      return;  // Uninitialized, or invalid pin number
    }

    auto const bit = 1 << (pin - CRADLE_X0);
    auto new_dir = xgpio_dir;
    if (mode == INPUT) {
      new_dir |= bit;
    } else if (mode == OUTPUT) {
      new_dir &= ~bit;
    } else {
      // TODO: Support INPUT_PULLUP if all XGPIO pins are pullup or output
      log_e("Bad pin mode %d, ABORTING", mode);
      abort();
    }

    if (new_dir != xgpio_dir) {
      Wire.beginTransmission(XGPIO_ADDR);
      Wire.write(0x04);
      Wire.write(new_dir);  // 0x04 (Config_Port0)
      if (auto const error = Wire.endTransmission()) {
        log_e("I2C error #%d setting dir=0x%02x", error, new_dir);
      } else {
        xgpio_dir = new_dir;
      }
    }
  }

  void digitalWrite(uint8_t pin, PinStatus val) {
    if (pin >= CRADLE_X0 + pg_cradle.extra_gpio_count) return;
    if (pin < CRADLE_X0) {
      ::digitalWrite(pin, val);
      return;
    }

    auto const bit = 1 << (pin - CRADLE_X0);
    auto const new_output = (xgpio_output & ~bit) | (val ? bit : 0);
    if (new_output != xgpio_output) {
      Wire.beginTransmission(XGPIO_ADDR);
      Wire.write(0x02);
      Wire.write(new_output);  // 0x02 (Output_Port0)
      if (auto const error = Wire.endTransmission()) {
        log_e("I2C error #%d setting output=0x%02x", error, new_output);
      } else {
        xgpio_output = new_output;
      }
    }

    pg_cradle_io::pinMode(pin, OUTPUT);
  }

  int digitalRead(uint8_t pin) {
    if (pin < CRADLE_X0) {
      return ::digitalRead(pin);
    }
    if (pin >= CRADLE_X0 + pg_cradle.extra_gpio_count) {
      return LOW;  // Uninitialized, or invalid pin number
    }

    Wire.beginTransmission(XGPIO_ADDR);
    Wire.write(0x00);  // Input_Port0
    Wire.endTransmission();
    if (Wire.requestFrom(XGPIO_ADDR, 1) != 1) {
      log_e("I2C error reading input", pin);
      return LOW;
    }
    uint8_t const input = Wire.read();
    return (input >> (pin - CRADLE_X0)) & 1;
  }
}

void pg_set_cradle_led(int which, int percent) {
  if (which < 0 || which >= pg_cradle.status_led_count) return;

  auto const power = (NORMAL_LED_POWER[which] * percent + 99) / 100;
  auto const clamped_power = std::clamp(power, 0, MAX_LED_POWER);
  if (clamped_power != led_power[which]) {
    led_power[which] = clamped_power;
    Wire.beginTransmission(XGPIO_ADDR);
    Wire.write(0x23 - which);  // LED0=DIM11 ... LED3=DIM8
    Wire.write(clamped_power);
    if (auto const error = Wire.endTransmission()) {
      log_e("I2C error #%d setting L%d power=%d", error, which, clamped_power);
    }
  }
}

bool pg_cradle_button(int which) {
  if (which < 0 || which >= pg_cradle.button_count) return false;

  // Momentarily toggle button to GPIO state
  Wire.beginTransmission(XGPIO_ADDR);
  Wire.write(0x13);
  Wire.write((0x40 >> which));
  Wire.endTransmission();

  Wire.beginTransmission(XGPIO_ADDR);
  Wire.write(0x01);
  Wire.endTransmission();
  if (Wire.requestFrom(XGPIO_ADDR, 1) != 1) {
    log_e("I2C error reading button %d", which);
    return false;
  }
  uint8_t const input = Wire.read();

  // Reset button back to LED state
  Wire.beginTransmission(XGPIO_ADDR);
  Wire.write(0x13);
  Wire.write(0x00);
  Wire.endTransmission();

  return (input & (0x40 >> which)) != 0;
}

#if HAVE_U8G2
static int line_height(cradle_line const& line) {
  return line.style & ~CRADLE_TEXT_BOLD & ~CRADLE_TEXT_INVERSE;
}

// Draws a line of text, returns the Y of the line bottom
static int redraw_line(cradle_line const& line, int top_y) {
  int const line_h = line_height(line);
  if (line_h < CRADLE_TEXT_SIZE_MIN) {
    if (line_h != 0) log_e("Bad text size=%d", line.style);
    return top_y;
  }

  bool const bold = line.style & CRADLE_TEXT_BOLD;
  bool const inverse = line.style & CRADLE_TEXT_INVERSE;

  int const screen_w = pg_cradle_screen->getDisplayWidth();
  pg_cradle_screen->setDrawColor(inverse);
  pg_cradle_screen->drawBox(0, top_y, screen_w, line_h);

  uint8_t const* font;
  switch (line_h) {
    case 6: font = u8g2_font_u8glib_4_tr;  // No bold at this size
    case 7: font = bold ? u8g2_font_wedge_tr : u8g2_font_tinypixie2_tr; break;
    case 8: font = bold ? u8g2_font_squeezed_b6_tr : u8g2_font_5x7_tr; break;
    case 9:
    case 10: font = bold ? u8g2_font_NokiaSmallBold_tr
                         : u8g2_font_NokiaSmallPlain_tr; break;
    case 11: font = bold ? u8g2_font_helvB08_tr : u8g2_font_helvR08_tr; break;
    case 12: font = bold ? u8g2_font_t0_11b_tr : u8g2_font_t0_11_tr; break;
    case 13:
    case 14: font = bold ? u8g2_font_t0_13b_tr : u8g2_font_t0_13_tr; break;
    default: font = bold ? u8g2_font_helvB10_tr : u8g2_font_helvR10_tr; break;
  }

  pg_cradle_screen->setDrawColor(!inverse);
  pg_cradle_screen->setFont(font);
  pg_cradle_screen->setFontMode(1);  // For _tr fonts
  pg_cradle_screen->setFontPosTop();

  // https://github.com/olikraus/u8g2/issues/2299
  int const x_offset = u8g2_GetStrX(pg_cradle_screen->getU8g2(), line.text);
  pg_cradle_screen->drawStr(-x_offset, top_y, line.text);
  return top_y + line_h;
}

static void set_line(int which, int style, char const* text) {
  auto* const lp = &lines[which];
  if (strncmp(text, lp->text, LINE_CHARS) == 0 && style == lp->style) {
    return;
  }

  int const screen_w = pg_cradle_screen->getDisplayWidth();
  int const screen_h = pg_cradle_screen->getDisplayHeight();

  int top_y = 0;
  for (int li = 0; li < which; ++li) {
    top_y += line_height(lines[li]);
  }

  auto const old_line_h = line_height(*lp);
  lp->style = style;
  strncpy(lp->text, text, LINE_CHARS);

  int bot_y = redraw_line(*lp, top_y);
  int const delta_y = (bot_y - top_y) - old_line_h;
  if (delta_y != 0) {
    for (int li = which + 1; li < LINE_COUNT && bot_y < screen_h; ++li) {
      bot_y = redraw_line(lines[li], bot_y);
    }
  }

  int const clear_y = first_print ? screen_h : bot_y - delta_y;
  if (clear_y > bot_y) {
    pg_cradle_screen->setDrawColor(0);
    pg_cradle_screen->drawBox(0, bot_y, screen_w, clear_y - bot_y);
    bot_y = clear_y;
  }

  int const tx = (screen_h - bot_y) / 8;
  int const tw = (screen_h - top_y + 7) / 8 - tx;
  int const ty = 0, th = (screen_w + 7) / 8;
  pg_cradle_screen->updateDisplayArea(tx, ty, tw, th);
  first_print = false;
}
#endif  // HAVE_U8G2

void pg_cradle_printf(int line, int style, char const* format, ...) {
#if HAVE_U8G2
  if (!pg_cradle_screen || line < 0) return;

  char buf[(LINE_CHARS * LINE_COUNT) + 1];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);
  set_line(line, style, buf);

  // Allow multiple lines in one print, using newlines
  for (char* next = buf; char* text = strtok_r(next, "\r\n", &next); ) {
    if (line >= LINE_COUNT) break;
    set_line(line++, style, text);
  }
#endif
}
