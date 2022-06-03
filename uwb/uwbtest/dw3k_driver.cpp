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

  auto const status_lo = dw3k_read<uint32_t>(DW3K_SYS_STATUS_LO);
  auto const status_hi = dw3k_read<uint32_t>(DW3K_SYS_STATUS_HI);
  if ((status_lo & 0x020C0000) || (status_hi & 0xF00)) {
    last_status = DW3KStatus::ChipError;
    error_text = "Chip: Status error";
    if ((status_lo & 0x00040000)) error_text = "Chip: Impulse analyzer failure";
    if ((status_lo & 0x00080000)) error_text = "Chip: Low voltage";
    if ((status_lo & 0x02000000)) error_text = "Chip: Clock PLL losing lock";
    if ((status_hi & 0x0100)) error_text = "Chip: Command error";
    if ((status_hi & 0x0E00)) error_text = "Chip: SPI error";
  }

  if (last_status == DW3KStatus::ResetWaitPLL) {
    if (!(status_lo & 0x2)) return last_status;
    dw3k_write<uint32_t>(DW3K_RX_CAL, 0x11);
    last_status = DW3KStatus::CalibrationWait;
  }

  if (last_status == DW3KStatus::CalibrationWait) {
    if (!dw3k_read<uint32_t>(DW3K_RX_CAL_STS)) return last_status;
    dw3k_write<uint32_t>(DW3K_RX_CAL, 0);
    last_status = DW3KStatus::Ready;
  }

  if (last_status == DW3KStatus::TransmitWait) {
    if ((status_lo & 0xF0)) {
      last_status = DW3KStatus::TransmitActive;
      dw3k_write<uint32_t>(DW3K_SYS_STATUS_LO, 0xF0);  // Clear bit
    } else if ((status_lo & 0x8000000)) {
      last_status = DW3KStatus::TransmitTooLate;
      dw3k_write<uint32_t>(DW3K_SYS_STATUS_LO, 0x8000000);  // Clear bit
    } else if (dw3k_read<uint32_t>(DW3K_SYS_STATE) == 0xD0000) {
      // See DW3000 user Manual 9.4.1 "Delayed TX Notes", and:
      // https://forum.qorvo.com/t/dw3000-hpdwarn-errata-need-clarification/12263
      // https://github.com/foldedtoad/dwm3000/blob/ece11140cffa069187865f6c9b432db267a941f7/decadriver/deca_device_api.h#L246
      last_status = DW3KStatus::TransmitTooLate;
    }
  }

  if ((last_status == DW3KStatus::TransmitActive) && (status_lo & 0x80)) {
    last_status = DW3KStatus::Ready;
    dw3k_write<uint32_t>(DW3K_SYS_STATUS_LO, 0x80);  // Clear bit
  }

  return last_status;
}

char const* dw3k_status_text() {
  switch (last_status) {
#define S(s) case DW3KStatus::s: return #s;
    S(Invalid);
    S(ResetActive);
    S(ResetWaitIRQ);
    S(ResetWaitPLL);
    S(CalibrationWait);
    S(Ready);
    S(ReceiveActive);
    S(ReceiveAnalysis);
    S(TransmitWait);
    S(TransmitActive);
    S(TransmitTooLate);
#undef S
    case DW3KStatus::ChipError: return error_text;
    case DW3KStatus::CodeBug: return error_text;
  }
  return "[BAD STATUS]";
}

uint32_t dw3k_clock_t32() {
  if (last_status < DW3KStatus::ResetWaitPLL)
    return bug("BUG: Not ready for dw3k_clock_t32"), 0;
  dw3k_write<uint8_t>(DW3K_SYS_TIME, 0);
  return dw3k_read<uint32_t>(DW3K_SYS_TIME);
}

void dw3k_schedule_tx(void const* data, int size, uint32_t sched_t32) {
  if (size < 0 || size >= 1023 - 2)
    return bug("BUG: Bad size for dw3k_start_transmit");
  if (last_status != DW3KStatus::Ready)
    return bug("BUG: Not ready for dw3k_start_transmit");

  dw3k_write(DW3K_TX_BUFFER, data, size);
  dw3k_write<uint32_t>(DW3K_TX_FCTRL_LO, 0x1C00 | size);
  dw3k_write<uint16_t>(DW3K_CHAN_CTRL, 0x031E);
  dw3k_write(DW3K_DX_TIME, sched_t32);
  dw3k_command(DW3K_DTX);
  last_status = DW3KStatus::TransmitWait;
}

uint32_t dw3k_tx_leadtime_t32(int size) {
  if (last_status < DW3KStatus::ResetWaitPLL)
    return bug("BUG: Not ready for dw3k_schedule_tx"), 0;

  int pre_sym;
  switch ((dw3k_read<uint32_t>(DW3K_TX_FCTRL_LO) >> 12) & 0xF) {
    case 0x1: pre_sym = 64; break;
    case 0x2: pre_sym = 1024; break;
    case 0x3: pre_sym = 4096; break;
    case 0x4: pre_sym = 32; break;
    case 0x5: pre_sym = 128; break;
    case 0x6: pre_sym = 1536; break;
    case 0x9: pre_sym = 256; break;
    case 0xA: pre_sym = 2048; break;
    case 0xD: pre_sym = 512; break;
    default:
      last_status = DW3KStatus::ChipError;
      error_text = "Chip: Bad TXPSR value";
      return 0;
  }

  auto const chan_ctrl = dw3k_read<uint16_t>(DW3K_CHAN_CTRL);
  auto const sym_count = pre_sym + (((chan_ctrl & 0x6) == 0x4) ? 16 : 8);
  auto const sym_t = ((chan_ctrl & 0xF8) <= 0x40) ? 993.59e-9 : 1017.63e-9;
  auto const spi_byte_t = 8 / 11e6;  // Conservative SPI (12MHz - overhead)
  auto const spi_bytes = (2 + size) + (2 + 4) + (2 + 2) + (2 + 4) + 1;
  auto const t = spi_bytes * spi_byte_t + sym_count * sym_t + 20e-6;
  return uint32_t(t * dw3k_time32_hz) + 1;
}

uint64_t dw3k_tx_expect_t40(uint32_t sched_t32) {
  if (last_status < DW3KStatus::ResetWaitPLL)
    return bug("BUG: Not ready for dw3k_tx_expect_t40"), 0;
  static auto tx_delay40 = dw3k_read<uint32_t>(DW3K_TX_ANTD);
  return (uint64_t(sched_t32 & ~1u) << 8) + tx_delay40;
}

uint64_t dw3k_tx_actual_t40() {
  if (last_status != DW3KStatus::Ready)
    return bug("BUG: Not ready for dw3k_tx_stamp"), 0;
  return dw3k_read<uint64_t>(DW3K_TX_STAMP_LO);
}

void dw3k_cancel_txrx() {
  switch (last_status) {
    case DW3KStatus::TransmitWait:
    case DW3KStatus::TransmitActive:
    case DW3KStatus::TransmitTooLate:
    case DW3KStatus::ReceiveActive:
    case DW3KStatus::ReceiveAnalysis:
    case DW3KStatus::Ready:
      break;
    default:
      return bug("BUG: Not ready for dw3k_cancel_txrx");
  }

  dw3k_command(DW3K_TXRXOFF);
  last_status = DW3KStatus::Ready;
}
