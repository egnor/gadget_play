#pragma once

#include <stdint.h>

enum class DW3KStatus {
  Invalid,
  ResetActive,
  ResetWaitIRQ,
  ResetWaitPLL,
  CalibrationWait,
  TransmitWait,
  TransmitActive,
  TransmitTooLate,
  ReceiveActive,
  ReceiveAnalysis,
  Ready,
  ChipError,
  CodeBug,
};

static constexpr double dw3k_chip_hz = 499.2e6;
static constexpr double dw3k_time32_hz = dw3k_chip_hz / 2;
static constexpr double dw3k_time40_hz = dw3k_chip_hz * 128;

void dw3k_reset();
DW3KStatus dw3k_poll();
char const* dw3k_status_text();
uint32_t dw3k_clock_t32();

void dw3k_schedule_tx(void const* data, int size, uint32_t sched_clock32);
uint32_t dw3k_tx_leadtime_t32(int size);
uint64_t dw3k_tx_expect_t40(uint32_t sched_clock32);
uint64_t dw3k_tx_actual_t40();

void dw3k_start_rx();

void dw3k_cancel_txrx();

char const* debug(DW3KStatus);
