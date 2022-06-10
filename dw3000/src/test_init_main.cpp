#include <Arduino.h>
#include <avr/dtostrf.h>

#include "dw3k.h"
#include "dw3k_registers.h"
#include "dw3k_spi.h"

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.printf("Resetting DW3K...\n");
  dw3k_reset();
  dw3k_wait_verbose(DW3KStatus::ResetWaitPLL);
  Serial.printf("DEV_ID      %08x\n", dw3k_read<uint32_t>(DW3K_DEV_ID));
  Serial.printf("SYS_CFG     %08x\n", dw3k_read<uint32_t>(DW3K_SYS_CFG));
  Serial.printf("TX_FCTRL    %012x\n", dw3k_read<uint64_t>(DW3K_TX_FCTRL_64));
  Serial.printf("TX_ANTD     %04x\n", dw3k_read<uint16_t>(DW3K_TX_ANTD));
  Serial.printf("CHAN_CTRL   %04x\n", dw3k_read<uint16_t>(DW3K_CHAN_CTRL));
  Serial.printf("DGC_CFG     %04x\n", dw3k_read<uint16_t>(DW3K_DGC_CFG));
  Serial.printf("RX_CAL_RESI %08x\n", dw3k_read<uint32_t>(DW3K_RX_CAL_RESI));
  Serial.printf("RX_CAL_RESQ %08x\n", dw3k_read<uint32_t>(DW3K_RX_CAL_RESQ));
  Serial.printf("DTUNE0      %04x\n", dw3k_read<uint16_t>(DW3K_DTUNE0));
  Serial.printf("RX_SFD_TOC  %04x\n", dw3k_read<uint16_t>(DW3K_RX_SFD_TOC));
  Serial.printf("PRE_TOC     %04x\n", dw3k_read<uint16_t>(DW3K_PRE_TOC));
  Serial.printf("DTUNE3      %08x\n", dw3k_read<uint32_t>(DW3K_DTUNE3));
  Serial.printf("RF_RX_CTRL2 %02x\n", dw3k_read<uint32_t>(DW3K_RF_RX_CTRL2));
  Serial.printf("RF_TX_CTRL1 %02x\n", dw3k_read<uint8_t>(DW3K_RF_TX_CTRL1));
  Serial.printf("RF_TX_CTRL2 %08x\n", dw3k_read<uint32_t>(DW3K_RF_TX_CTRL2));
  Serial.printf("LDO_TUNE    %016x\n", dw3k_read<uint64_t>(DW3K_LDO_TUNE_64));
  Serial.printf("LDO_CTRL    %08x\n", dw3k_read<uint32_t>(DW3K_LDO_CTRL));
  Serial.printf("SEQ_CTRL    %08x\n", dw3k_read<uint32_t>(DW3K_SEQ_CTRL));
  Serial.printf("CIA_CONF    %08x\n", dw3k_read<uint32_t>(DW3K_CIA_CONF));
  Serial.printf("\n");

  dw3k_wait_verbose(DW3KStatus::Ready);
}

void loop() {
  char n[40];

  Serial.printf("\nSending...\n");
  char msg[] = "TEST_INIT HELLO";
  dw3k_buffer_tx(msg, sizeof(msg));

  auto const lead_t32 = dw3k_tx_leadtime_t32();
  auto const extra_t32 = 100e-6 * dw3k_time32_hz;
  auto const start_t32 = dw3k_clock_t32();
  auto const sched_t32 = start_t32 + lead_t32 + extra_t32;
  auto const expect_t40 = dw3k_tx_expected_t40(sched_t32);
  auto const before_t32 = dw3k_clock_t32();
  dw3k_schedule_tx(sched_t32);
  auto const added_t32 = dw3k_clock_t32();
  dw3k_wait_verbose(DW3KStatus::TransmitDone);
  auto const done_t32 = dw3k_clock_t32();
  auto const actual_t40 = dw3k_tx_timestamp_t40();

  Serial.printf("start  %s\n", dtostrf(start_t32 / dw3k_time32_hz, 12, 9, n));
  Serial.printf("lead  +%s\n", dtostrf(lead_t32 / dw3k_time32_hz, 12, 9, n));
  Serial.printf("extra +%s\n", dtostrf(extra_t32 / dw3k_time32_hz, 12, 9, n));
  Serial.printf("before %s\n", dtostrf(before_t32 / dw3k_time32_hz, 12, 9, n));
  Serial.printf("added  %s\n", dtostrf(added_t32 / dw3k_time32_hz, 12, 9, n));
  Serial.printf("sched  %s\n", dtostrf(sched_t32 / dw3k_time32_hz, 12, 9, n));
  Serial.printf("expect %s\n", dtostrf(expect_t40 / dw3k_time40_hz, 15, 12, n));
  Serial.printf("actual %s\n", dtostrf(actual_t40 / dw3k_time40_hz, 15, 12, n));
  Serial.printf("done   %s\n", dtostrf(done_t32 / dw3k_time32_hz, 12, 9, n));

  dw3k_end_txrx();
  dw3k_wait_verbose(DW3KStatus::Ready);

  Serial.printf("\nReceiving...\n");
  dw3k_start_rx();
  dw3k_wait_verbose(DW3KStatus::ReceiveDone);

  uint8_t buf[dw3k_packet_size];
  auto const size = dw3k_rx_size();
  auto const rx_t40 = dw3k_rx_timestamp_t40();
  dw3k_retrieve_rx(0, size, buf);
  Serial.printf("received %s\n", dtostrf(rx_t40 / dw3k_time40_hz, 15, 12, n));
  Serial.printf("size     %d\n", size);
  Serial.printf("data     [");
  for (int i = 0; i < size; ++i)
    Serial.print(isPrintable(buf[i]) ? (char) buf[i] : '.');
  Serial.printf("]\n");

  dw3k_end_txrx();
  dw3k_wait_verbose(DW3KStatus::Ready);
}
