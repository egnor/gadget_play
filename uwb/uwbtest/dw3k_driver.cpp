#include "dw3k_driver.h"

#include <Arduino.h>

#include "dwm3k_pins.h"
#include "dw3k_registers.h"
#include "dw3k_spi.h"

DW3KStatus last_status = DW3KStatus::Unknown;
static unsigned long reset_millis;

void dw3k_reset() {
  digitalWrite(DW3K_RSTn_PIN, 0);
  digitalWrite(DW3K_WAKEUP_PIN, 0);
  pinMode(DW3K_RSTn_PIN, OUTPUT);
  pinMode(DW3K_WAKEUP_PIN, OUTPUT);
  pinMode(DW3K_IRQ_PIN, INPUT);
  last_status = DW3KStatus::ResetActive;
  reset_millis = millis();
}

DW3KStatus dw3k_poll() {
  if (last_status == DW3KStatus::ResetActive) {
    if (digitalRead(DW3K_IRQ_PIN)) return last_status;
    if (millis() - reset_millis < 10) return last_status;
    pinMode(DW3K_RSTn_PIN, INPUT_PULLUP);
    last_status = DW3KStatus::ResetWaitIRQ;
  }

  if (last_status == DW3KStatus::ResetWaitIRQ) {
    if (!digitalRead(DW3K_IRQ_PIN)) return last_status;
    dw3k_init_spi();
    dw3k_maskset32(DW3K_SEQ_CTRL, ~0u, 0x100);
    last_status = DW3KStatus::ResetWaitPLL;
  }

  if (last_status == DW3KStatus::ResetWaitPLL) {
    if (!(dw3k_read32(DW3K_SYS_STATUS_LO) & 0x2)) return last_status;
    dw3k_maskset32(DW3K_RX_CAL, 0, 0x11);
    last_status = DW3KStatus::CalibrationWait;
  }

  if (last_status == DW3KStatus::CalibrationWait) {
    if (!dw3k_read32(DW3K_RX_CAL_STS)) return last_status;
    dw3k_maskset32(DW3K_RX_CAL, ~0x3u, 0);
    last_status = DW3KStatus::Ready;
  }

  return last_status;
}

char const* debug(DW3KStatus status) {
  switch (status) {
#define S(s) case DW3KStatus::s: return #s;
    S(Unknown);
    S(ResetActive);
    S(ResetWaitIRQ);
    S(ResetWaitPLL);
    S(CalibrationWait);
    S(Ready);
#undef S
  }
  return "[UNKNOWN]";
}
