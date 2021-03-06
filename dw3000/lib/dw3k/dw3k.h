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
  TransmitDone,
  TransmitTooLate,
  ReceiveListen,
  ReceiveAnalyze,
  ReceiveDone,
  Ready,
  ChipError,
  CodeBug,
};

static constexpr double dw3k_chip_hz = 499.2e6;
static constexpr double dw3k_time32_hz = dw3k_chip_hz / 2;
static constexpr double dw3k_time40_hz = dw3k_chip_hz * 128;
static constexpr int dw3k_packet_size = 1023 - 2;

void dw3k_reset();
DW3KStatus dw3k_poll();
uint32_t dw3k_clock_t32();

void dw3k_buffer_tx(void const* data, int size);
void dw3k_schedule_tx(uint32_t sched_t32);
uint32_t dw3k_tx_leadtime_t32();
uint64_t dw3k_tx_expected_t40(uint32_t sched_t32);
uint64_t dw3k_tx_timestamp_t40();

void dw3k_start_rx();
int dw3k_rx_size();
void dw3k_retrieve_rx(int offset, int size, void* out);
uint64_t dw3k_rx_timestamp_t40();
float dw3k_rx_clock_offset();

void dw3k_end_txrx();

char const* dw3k_status_text();
bool dw3k_wait_verbose(DW3KStatus wanted, int timeout_millis = 0);
