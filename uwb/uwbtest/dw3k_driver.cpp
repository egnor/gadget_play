#include "dw3k_driver.h"

#include <Arduino.h>

#include "dwm3k_pins.h"
#include "dw3k_registers.h"
#include "dw3k_spi.h"

DW3KStatus last_status = DW3KStatus::Invalid;

static char const* error_text = "[No error logged]";
static unsigned long reset_millis;

static uint32_t cache_tx_fctrl_lo;
static uint16_t cache_chan_ctrl;
static uint16_t tx_buffer_size;

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
  using DS = DW3KStatus;

  if (last_status == DS::ResetActive) {
    if (millis() - reset_millis < 10) return last_status;
    if (digitalRead(DW3K_IRQ_PIN)) return last_status;
    pinMode(DW3K_RSTn_PIN, INPUT_PULLUP);
    last_status = DS::ResetWaitIRQ;
  }

  if (last_status == DS::ResetWaitIRQ) {
    if (!digitalRead(DW3K_IRQ_PIN)) return last_status;
    dw3k_init_spi();
    dw3k_maskset32(DW3K_SEQ_CTRL, ~0u, 0x100);
    dw3k_write(DW3K_SYS_CFG, 0x00041098);  // add 0x400 for RXAUTR
    dw3k_write(DW3K_TX_FCTRL_LO, (cache_tx_fctrl_lo = 0x1C00));
    dw3k_write(DW3K_CHAN_CTRL, (cache_chan_ctrl = 0x031E));
    last_status = DS::ResetWaitPLL;
  }

  uint64_t sys_status = 0;
  dw3k_read(DW3K_SYS_STATUS_LO, &sys_status, 6);
  if (sys_status & 0xF00020C0000) {
    last_status = DS::ChipError;
    error_text = "Chip: Status error";
    if (sys_status & 0x00040000) error_text = "Chip: Impulse analyzer failure";
    if (sys_status & 0x00080000) error_text = "Chip: Low voltage";
    if (sys_status & 0x02000000) error_text = "Chip: Clock PLL losing lock";
    if (sys_status & 0x10000000000) error_text = "Chip: Command error";
    if (sys_status & 0xE0000000000) error_text = "Chip: SPI error";
  }

  if (last_status == DS::ResetWaitPLL) {
    if (!(sys_status & 0x2)) return last_status;
    dw3k_write<uint8_t>(DW3K_RX_CAL, 0x11);
    last_status = DS::CalibrationWait;
  }

  if (last_status == DS::CalibrationWait) {
    if (!dw3k_read<uint8_t>(DW3K_RX_CAL_STS)) return last_status;
    dw3k_write<uint8_t>(DW3K_RX_CAL, 0);
    last_status = DS::Ready;
  }

  auto const sys_state = dw3k_read<uint32_t>(DW3K_SYS_STATE);
  if (last_status == DS::TransmitWait) {
    if (sys_status & 0xF0) {
      last_status = DS::TransmitActive;
      dw3k_write(DW3K_SYS_STATUS_LO, 0xF0);  // Clear bit
    } else if (sys_status & 0x8000000) {
      last_status = DS::TransmitTooLate;
      dw3k_write(DW3K_SYS_STATUS_LO, 0x8000000);  // Clear bit
    } else if (sys_state == 0xD0000) {
      // See DW3000 user Manual 9.4.1 "Delayed TX Notes", and:
      // https://forum.qorvo.com/t/dw3000-hpdwarn-errata-need-clarification/12263
      // https://github.com/foldedtoad/dwm3000/blob/ece11140cffa069187865f6c9b432db267a941f7/decadriver/deca_device_api.h#L246
      last_status = DS::TransmitTooLate;
    }
  }

  if (last_status == DS::TransmitActive && (sys_status & 0x80)) {
    dw3k_write(DW3K_SYS_STATUS_LO, 0x80);  // Clear bit
    last_status = DS::TransmitDone;
  }

  auto const pmsc_state = (sys_state >> 16) & 0xFF;
  if (
      (last_status == DS::TransmitWait || last_status == DS::TransmitActive) &&
      (pmsc_state < 0x8 || pmsc_state > 0xF) &&
      !(dw3k_read<uint32_t>(DW3K_SYS_STATUS_LO) & 0xF0)
  ) {
    last_status = DS::ChipError;
    error_text = "Chip: PMSC not in TX state";
  }

  if (last_status == DS::ReceiveListen && (sys_status & 0x4000)) {
    dw3k_write(DW3K_SYS_STATUS_LO, 0x4000);  // Clear bit
    last_status = DS::ReceiveAnalyze;
  }

  if (last_status == DS::ReceiveAnalyze && (sys_status & 0x400)) {
    dw3k_write(DW3K_SYS_STATUS_LO, 0x400);  // Clear bit
    last_status = DS::ReceiveDone;
  }

  if (
      (last_status == DS::ReceiveListen || last_status == DS::ReceiveAnalyze) &&
      (pmsc_state < 0x12 || pmsc_state > 0x19) &&
      !(dw3k_read<uint32_t>(DW3K_SYS_STATUS_LO) & 0x4400)
  ) {
    last_status = DS::ChipError;
    error_text = "Chip: PMSC not in RX state";
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
    S(ReceiveListen);
    S(ReceiveAnalyze);
    S(ReceiveDone);
    S(TransmitWait);
    S(TransmitActive);
    S(TransmitDone);
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

void dw3k_buffer_tx(void const* data, int size) {
  if (size < 0 || tx_buffer_size + size >= dw3k_max_size)
    return bug("BUG: Bad size for dw3k_start_transmit");
  if (last_status != DW3KStatus::Ready)
    return bug("BUG: Not ready for dw3k_tx_buffer");

  dw3k_write({DW3K_TX_BUFFER.file, tx_buffer_size}, data, size);
  tx_buffer_size += size;

  uint32_t const new_fctrl_lo = (cache_tx_fctrl_lo & ~0x300u) | tx_buffer_size;
  if (new_fctrl_lo != cache_tx_fctrl_lo)
    dw3k_write(DW3K_TX_FCTRL_LO, (cache_tx_fctrl_lo = new_fctrl_lo));
}

void dw3k_schedule_tx(uint32_t sched_t32) {
  if (last_status != DW3KStatus::Ready)
    return bug("BUG: Not ready for dw3k_start_transmit");

  dw3k_write(DW3K_DX_TIME, sched_t32);
  dw3k_command(DW3K_DTX);
  last_status = DW3KStatus::TransmitWait;
}

uint32_t dw3k_tx_leadtime_t32() {
  if (last_status < DW3KStatus::ResetWaitPLL)
    return bug("BUG: Not ready for dw3k_tx_leadtime_t32"), 0;

  int pre_sym;
  switch ((dw3k_read<uint16_t>(DW3K_TX_FCTRL_LO) >> 12) & 0xF) {
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

  auto const sym_count = pre_sym + ((cache_chan_ctrl & 0x6) == 0x4 ? 16 : 8);
  auto const sym_t = (cache_chan_ctrl & 0xF8) <= 0x40 ? 993.59e-9 : 1017.63e-9;
  auto const t = sym_count * sym_t + 20e-6;
  return uint32_t(t * dw3k_time32_hz) + 1;
}

uint64_t dw3k_tx_expected_t40(uint32_t sched_t32) {
  if (last_status < DW3KStatus::ResetWaitPLL)
    return bug("BUG: Not ready for dw3k_tx_expect_t40"), 0;
  static auto tx_delay40 = dw3k_read<uint16_t>(DW3K_TX_ANTD);
  return (uint64_t(sched_t32 & ~1u) << 8) + tx_delay40;
}

uint64_t dw3k_tx_timestamp_t40() {
  if (last_status != DW3KStatus::TransmitDone)
    return bug("BUG: Not ready for dw3k_tx_stamp"), 0;
  return dw3k_read<uint64_t>(DW3K_TX_STAMP_LO);
}

void dw3k_start_rx() {
  if (last_status != DW3KStatus::Ready)
    return bug("BUG: Not ready for dw3k_start_rx");
  dw3k_command(DW3K_RX);
  last_status = DW3KStatus::ReceiveListen;
}

void dw3k_retrieve_rx(int offset, int size, void* out) {
  if (last_status != DW3KStatus::ReceiveAnalyze &&
      last_status != DW3KStatus::ReceiveDone)
    return bug("BUF: Not ready for dw3k_retrieve_rx");
  if (offset < 0 || size < 0 || offset + size > dw3k_max_size + 2)
    return bug("BUG: Bad offset/size for dw3k_retrieve_rx");
  dw3k_read({DW3K_RX_BUFFER_0.file, uint16_t(offset)}, out, size);
}

uint64_t dw3k_rx_timestamp_t40() {
  if (last_status != DW3KStatus::ReceiveDone)
    return bug("BUF: Not ready for dw3k_rx_timestamp_t40"), 0;
  return dw3k_read<uint64_t>(DW3K_RX_STAMP_LO);
}

int dw3k_rx_size() {
  if (last_status != DW3KStatus::ReceiveAnalyze &&
      last_status != DW3KStatus::ReceiveDone)
    return bug("BUF: Not ready for dw3k_rx_size"), 0;
  auto const raw = dw3k_read<uint16_t>(DW3K_RX_FINFO) & 0x3F;
  return (raw < 2) ? 0 : (raw > dw3k_max_size + 2) ? dw3k_max_size : raw - 2;
}

void dw3k_end_txrx() {
  switch (last_status) {
    case DW3KStatus::TransmitWait:
    case DW3KStatus::TransmitActive:
    case DW3KStatus::TransmitTooLate:
    case DW3KStatus::ReceiveListen:
    case DW3KStatus::ReceiveAnalyze:
      dw3k_command(DW3K_TXRXOFF);
      break;
    case DW3KStatus::TransmitDone:
    case DW3KStatus::ReceiveDone:
    case DW3KStatus::Ready:
      break;
    default:
      return bug("BUG: Not ready for dw3k_end_txrx");
  }

  tx_buffer_size = 0;
  last_status = DW3KStatus::Ready;
}
