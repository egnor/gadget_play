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

uint32_t dw3k_clock32();

void dw3k_schedule_tx(void const* data, int size, uint32_t sched_clock32);
uint64_t dw3k_tx_expect40(uint32_t sched_clock32);
uint64_t dw3k_tx_actual40();

void dw3k_start_rx();

char const* debug(DW3KStatus);
