// Interface to "power cradle" hardware (socket for WT32-ETH01)

#pragma once

#include <Arduino.h>

// Detected cradle hardware.
// All values are -1 before pg_detect_cradle(), 0 if feature not present.
extern struct pg_cradle_status {
  int scl_pin = -1;
  int sda_pin = -1;
  int extra_gpio_count = -1;
  int status_led_count = -1;
  int button_count = -1;
  bool has_pullups = false;
} pg_cradle;

// Cradle status LEDs for pg_set_cradle_led().
static constexpr int ORANGE_CRADLE_LED = 0;
static constexpr int GREEN_CRADLE_LED = 1;
static constexpr int BLUE_CRADLE_LED = 2;
static constexpr int WHITE_CRADLE_LED = 3;

// Expander pins for pg_cradle::(digitalWrite() / digitalRead() / setPinMode())
static constexpr int CRADLE_X0 = 100;
static constexpr int CRADLE_X1 = 101;
static constexpr int CRADLE_X2 = 102;
static constexpr int CRADLE_X3 = 103;
static constexpr int CRADLE_X4 = 104;
static constexpr int CRADLE_X5 = 105;
static constexpr int CRADLE_X6 = 106;

// Text styles for pg_cradle_line().
static constexpr int CRADLE_TEXT_SIZE_MIN = 6;
static constexpr int CRADLE_TEXT_SIZE_MAX = 15;
static constexpr int CRADLE_TEXT_BOLD = 0x70;     // Add to size
static constexpr int CRADLE_TEXT_INVERSE = 0x80;  // Add to size

// Raw display access (see github.com/olikraus/u8g2/wiki/u8g2reference).
class U8G2_SSD1306_64X32_1F_F_HW_I2C;  // Forward declaration.
using pg_cradle_screen_driver = U8G2_SSD1306_64X32_1F_F_HW_I2C;
extern pg_cradle_screen_driver* pg_cradle_screen;

// Detects and initializes cradle hardware, filling in pg_cradle.
void pg_detect_cradle();

// Versions of GPIO that support the expanded pins (and also regular pins).
// "using namespace pg_cradle_io;" will import these globally.
namespace pg_cradle_io {
  void pinMode(uint8_t pin, PinMode mode);
  void digitalWrite(uint8_t pin, PinStatus val);
  int digitalRead(uint8_t pin);
}

// Sets a status LED to a percent of "reasonable" brightness (can be >100).
void pg_set_cradle_led(int which, int percent);

// Gets the status of a cradle microbutton.
bool pg_cradle_button(int which);

// Prints text on the display; will not wrap, but \n advances to the next line.
//  line - starts at 0 for the first line
//  style - size (CRADLE_TEXT_SIZE_MIN/MAX) + flags (CRADLE_TEXT_BOLD/INVERSE)
void pg_cradle_printf(int line, int style, char const* format, ...);
