#include "dw3k_driver.h"

#include <Arduino.h>

#include "dwm3k_pins.h"
#include "dw3k_registers.h"
#include "dw3k_spi.h"

DW3KStatus last_status = DW3KStatus::Invalid;

static char const* error_text = "[No error logged]";
static unsigned long reset_millis;

static void bug(char const* text) {
  last_status = DW3KStatus::CodeBug;
  error_text = text;
}

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
    if (millis() - reset_millis < 10) return last_status;
    if (digitalRead(DW3K_IRQ_PIN)) return last_status;
    pinMode(DW3K_RSTn_PIN, INPUT_PULLUP);
    last_status = DW3KStatus::ResetWaitIRQ;
  }

  if (last_status == DW3KStatus::ResetWaitIRQ) {
    if (!digitalRead(DW3K_IRQ_PIN)) return last_status;
    dw3k_init_spi();
    dw3k_maskset32(DW3K_SEQ_CTRL, ~0u, 0x100);
    dw3k_write<uint32_t>(DW3K_SYS_CFG, 0x00041498);
    last_status = DW3KStatus::ResetWaitPLL;
  }

  auto const sys_lo = dw3k_read<uint32_t>(DW3K_SYS_STATUS_LO);
  auto const sys_hi = dw3k_read<uint32_t>(DW3K_SYS_STATUS_HI);
  if ((sys_lo & 0xA0C0000) || (sys_hi & 0xF00)) {
    last_status = DW3KStatus::ChipError;
    error_text = "Chip: Status error";
    if ((sys_lo & 0x00040000)) error_text = "Chip: Impulse analyzer failure";
    if ((sys_lo & 0x00080000)) error_text = "Chip: Low voltage";
    if ((sys_lo & 0x02000000)) error_text = "Chip: Clock PLL losing lock";
    if ((sys_lo & 0x08000000)) error_text = "Chip: Schedule overrun";
    if ((sys_hi & 0x0100)) error_text = "Chip: Command error";
    if ((sys_hi & 0x0E00)) error_text = "Chip: SPI error";
  }

  if (last_status == DW3KStatus::ResetWaitPLL) {
    if (!(sys_lo & 0x2)) return last_status;
    dw3k_write<uint32_t>(DW3K_RX_CAL, 0x11);
    last_status = DW3KStatus::CalibrationWait;
  }

  if (last_status == DW3KStatus::CalibrationWait) {
    if (!dw3k_read<uint32_t>(DW3K_RX_CAL_STS)) return last_status;
    dw3k_write<uint32_t>(DW3K_RX_CAL, 0);
    last_status = DW3KStatus::Ready;
  }

  if (last_status == DW3KStatus::TransmitWait && (sys_lo & 0xF0)) {
    last_status = DW3KStatus::TransmitActive;
  }

  if (last_status == DW3KStatus::TransmitActive) {
    if ((sys_lo & 0x80))
      last_status = DW3KStatus::Ready;
  }

  return last_status;
}

uint32_t dw3k_clock32() {
  if (last_status < DW3KStatus::ResetWaitPLL)
    return bug("BUG: Not ready for dw3k_clock32"), 0;
  dw3k_write<uint8_t>(DW3K_SYS_TIME, 0);
  return dw3k_read<uint32_t>(DW3K_SYS_TIME);
}

void dw3k_schedule_tx(void const* data, int size, uint32_t sched_clock32) {
  if (size < 0 || size >= 1023 - 2)
    return bug("BUG: Bad size for dw3k_start_transmit");
  if (last_status != DW3KStatus::Ready)
    return bug("BUG: Not ready for dw3k_start_transmit");

  dw3k_write(DW3K_TX_BUFFER, data, size);
  dw3k_write<uint32_t>(DW3K_TX_FCTRL_LO, 0x1C00 | size);
  dw3k_write<uint16_t>(DW3K_CHAN_CTRL, 0x031E);
  dw3k_write(DW3K_DX_TIME, sched_clock32);
  dw3k_command(DW3K_DTX);
  last_status = DW3KStatus::TransmitWait;
}

uint64_t dw3k_tx_expect40(uint32_t sched_clock32) {
  if (last_status < DW3KStatus::ResetWaitPLL)
    return bug("BUG: Not ready for dw3k_tx_expect40"), 0;
  static auto tx_delay40 = dw3k_read<uint32_t>(DW3K_TX_ANTD);
  return (uint64_t(sched_clock32 & ~1u) << 8) + tx_delay40;
}

uint64_t dw3k_tx_actual40() {
  if (last_status != DW3KStatus::Ready)
    return bug("BUG: Not ready for dw3k_tx_stamp"), 0;
  return dw3k_read<uint64_t>(DW3K_TX_STAMP_LO);
}

char const* debug(DW3KStatus status) {
  switch (status) {
#define S(s) case DW3KStatus::s: return #s;
    S(Invalid);
    S(ResetActive);
    S(ResetWaitIRQ);
    S(ResetWaitPLL);
    S(CalibrationWait);
    S(Ready);
    S(TransmitWait);
    S(TransmitActive);
#undef S
    case DW3KStatus::ChipError: return error_text;
    case DW3KStatus::CodeBug: return error_text;
  }
  return "[BAD STATUS]";
}
