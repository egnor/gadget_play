#include <Arduino.h>
#include <avr/dtostrf.h>

#include "dw3k_driver.h"
#include "dw3k_registers.h"
#include "dw3k_spi.h"

static void wait_for(DW3KStatus wanted, unsigned long timeout_millis = 0) {
  auto last_status = DW3KStatus::Invalid;
  int last_counters[dw3k_num_counters] = {};
  auto start_millis = millis();
  for (int i = 0;; ++i) {
    auto const s = dw3k_poll();
    if (s != last_status || !(i % 10000)) {
      Serial.printf("DW3K %-15s", dw3k_status_text());
      if (s >= DW3KStatus::ResetWaitPLL) {
        Serial.printf(
            " (status=%04x:%08x state=%08x)",
            dw3k_read<uint32_t>(DW3K_SYS_STATUS_HI),
            dw3k_read<uint32_t>(DW3K_SYS_STATUS_LO),
            dw3k_read<uint32_t>(DW3K_SYS_STATE)
        );
      }
      Serial.printf("...\n");
    }

    if (!(i % 1000)) {
      bool counter_changed = false;
      for (int c = 0; c < dw3k_num_counters; ++c) {
        int const old_value = last_counters[c];
        dw3k_counter(c, &last_counters[c]);
        if (last_counters[c] != old_value) counter_changed = true;
      }
      if (counter_changed) {
        Serial.printf("DW3K counters:");
        for (int c = 0; c < dw3k_num_counters; ++c) {
          auto const v = last_counters[c];
          if (v) Serial.printf(" %s=%d", dw3k_counter(c), v);
        }
        Serial.printf("\n");
      }
    }

    if (timeout_millis && !(i % 100)) {
      auto const elapsed_millis = millis() - start_millis;
      if (elapsed_millis > timeout_millis) break;
    }

    if (s == wanted) break;
    delayMicroseconds(10);
    last_status = s;
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.printf("Resetting DW3K...\n");
  dw3k_reset();
  wait_for(DW3KStatus::Ready);

  Serial.printf("DEV_ID       %08x\n", dw3k_read<uint32_t>(DW3K_DEV_ID));
  Serial.printf("SYS_CFG      %08x\n", dw3k_read<uint32_t>(DW3K_SYS_CFG));
  Serial.printf("TX_FCTRL     %012x\n", dw3k_read<uint64_t>(DW3K_TX_FCTRL_LO));
  Serial.printf("TX_ANTD      %04x\n", dw3k_read<uint16_t>(DW3K_TX_ANTD));
  Serial.printf("CHAN_CTRL    %04x\n", dw3k_read<uint16_t>(DW3K_CHAN_CTRL));
  Serial.printf("DGC_CFG      %04x\n", dw3k_read<uint16_t>(DW3K_DGC_CFG));
  Serial.printf("RX_CAL_RESI  %08x\n", dw3k_read<uint32_t>(DW3K_RX_CAL_RESI));
  Serial.printf("RX_CAL_RESQ  %08x\n", dw3k_read<uint32_t>(DW3K_RX_CAL_RESQ));
  Serial.printf("DTUNE0       %04x\n", dw3k_read<uint16_t>(DW3K_DTUNE0));
  Serial.printf("RX_SFD_TOC   %04x\n", dw3k_read<uint16_t>(DW3K_RX_SFD_TOC));
  Serial.printf("PRE_TOC      %04x\n", dw3k_read<uint16_t>(DW3K_PRE_TOC));
  Serial.printf("DTUNE3       %08x\n", dw3k_read<uint32_t>(DW3K_DTUNE3));
  Serial.printf("RF_TX_CTRL_1 %02x\n", dw3k_read<uint8_t>(DW3K_RF_TX_CTRL_1));
  Serial.printf("RF_TX_CTRL_2 %08x\n", dw3k_read<uint32_t>(DW3K_RF_TX_CTRL_2));
  Serial.printf("SEQ_CTRL     %08x\n", dw3k_read<uint32_t>(DW3K_SEQ_CTRL));
  Serial.printf("CIA_CONF     %08x\n", dw3k_read<uint32_t>(DW3K_CIA_CONF));
}

void loop() {
  char n[40];

  Serial.printf("\nSending...\n");
  char msg[] = "HELLO";
  dw3k_buffer_tx(msg, sizeof(msg));

  auto const lead_t32 = dw3k_tx_leadtime_t32();
  auto const extra_t32 = 50e-6 * dw3k_time32_hz;
  auto const start_t32 = dw3k_clock_t32();
  auto const sched_t32 = start_t32 + lead_t32 + extra_t32;
  auto const expect_t40 = dw3k_tx_expected_t40(sched_t32);
  auto const before_t32 = dw3k_clock_t32();
  dw3k_schedule_tx(sched_t32);
  auto const added_t32 = dw3k_clock_t32();
  wait_for(DW3KStatus::TransmitDone);
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
  wait_for(DW3KStatus::Ready);

  Serial.printf("\nReceiving...\n");
  dw3k_start_rx();
  wait_for(DW3KStatus::ReceiveDone, 500);

  if (dw3k_poll() == DW3KStatus::ReceiveDone) {
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
  }

  dw3k_end_txrx();
  wait_for(DW3KStatus::Ready);
}
