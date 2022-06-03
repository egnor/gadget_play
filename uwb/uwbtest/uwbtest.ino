#include <Arduino.h>
#include <avr/dtostrf.h>

#include "dw3k_driver.h"
#include "dw3k_registers.h"
#include "dw3k_spi.h"

static void wait_for_ready() {
  auto last = DW3KStatus::Invalid;
  for (int c = 0;; ++c) {
    auto const s = dw3k_poll();
    if (s != last || !(c % 10000))
      Serial.printf("DW3K: %s...\n", dw3k_status_text());
    if (s == DW3KStatus::Ready) break;
    delayMicroseconds(10);
    last = s;
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.printf("Resetting DW3K...\n");
  dw3k_reset();
  wait_for_ready();

  Serial.printf("DEV_ID      %08x\n", dw3k_read<uint32_t>(DW3K_DEV_ID));
  Serial.printf("SYS_CFG     %08x\n", dw3k_read<uint32_t>(DW3K_SYS_CFG));
  Serial.printf("SEQ_CTRL    %08x\n", dw3k_read<uint32_t>(DW3K_SEQ_CTRL));
  Serial.printf("RX_CAL_RESI %08x\n", dw3k_read<uint32_t>(DW3K_RX_CAL_RESI));
  Serial.printf("RX_CAL_RESQ %08x\n", dw3k_read<uint32_t>(DW3K_RX_CAL_RESQ));
  Serial.printf("TX_ANTD     %08x\n", dw3k_read<uint32_t>(DW3K_TX_ANTD));
  Serial.printf("CIACONF     %08x\n", dw3k_read<uint32_t>(DW3K_CIA_CONF));
  Serial.printf(
      "SYS_STATUS  %03x:%08x\n",
      dw3k_read<uint32_t>(DW3K_SYS_STATUS_HI),
      dw3k_read<uint32_t>(DW3K_SYS_STATUS_LO)
  );

  Serial.printf("\nSending...\n");
  char msg[1000] = "HELLO";

  auto const lead_t32 = dw3k_tx_leadtime_t32(sizeof(msg));
  auto const extra_t32 = 100e-6 * dw3k_time32_hz;
  auto const start_t32 = dw3k_clock_t32();
  auto const sched_t32 = start_t32 + lead_t32 + extra_t32;
  auto const expect_t40 = dw3k_tx_expect_t40(sched_t32);
  auto const before_t32 = dw3k_clock_t32();
  dw3k_schedule_tx(msg, sizeof(msg), sched_t32);
  auto const added_t32 = dw3k_clock_t32();
  wait_for_ready();
  auto const done_t32 = dw3k_clock_t32();
  auto const actual_t40 = dw3k_tx_actual_t40();

  char t[40];
  Serial.printf("start  %s\n", dtostrf(start_t32 / dw3k_time32_hz, 12, 9, t));
  Serial.printf("lead  +%s\n", dtostrf(lead_t32 / dw3k_time32_hz, 12, 9, t));
  Serial.printf("extra +%s\n", dtostrf(extra_t32 / dw3k_time32_hz, 12, 9, t));
  Serial.printf("before %s\n", dtostrf(before_t32 / dw3k_time32_hz, 12, 9, t));
  Serial.printf("added  %s\n", dtostrf(added_t32 / dw3k_time32_hz, 12, 9, t));
  Serial.printf("sched  %s\n", dtostrf(sched_t32 / dw3k_time32_hz, 12, 9, t));
  Serial.printf("expect %s\n", dtostrf(expect_t40 / dw3k_time40_hz, 15, 12, t));
  Serial.printf("actual %s\n", dtostrf(actual_t40 / dw3k_time40_hz, 15, 12, t));
  Serial.printf("done   %s\n", dtostrf(done_t32 / dw3k_time32_hz, 12, 9, t));
}

void loop() {
  Serial.println("Looping...");
  delay(500);
}
