#include <Arduino.h>
#include <avr/dtostrf.h>

#include "dw3k_driver.h"
#include "dw3k_registers.h"
#include "dw3k_spi.h"

static void wait_for_ready() {
  auto last = DW3KStatus::Invalid;
  for (int c = 0;; ++c) {
    auto const s = dw3k_poll();
    if (s != last || !(c % 10000)) Serial.printf("DW3K: %s...\n", debug(s));
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
  auto const start32 = dw3k_clock32();
  auto const sched32 = start32 + uint32_t(0.010 * dw3k_time32_hz);
  auto const expect40 = dw3k_tx_expect40(sched32);
  dw3k_schedule_tx("HELLO", 5, sched32);
  wait_for_ready();

  auto const actual40 = dw3k_tx_actual40();
  auto const after32 = dw3k_clock32();
  char buf[40];
  Serial.printf("start  %s\n", dtostrf(start32 / dw3k_time32_hz, 12, 9, buf));
  Serial.printf("sched  %s\n", dtostrf(sched32 / dw3k_time32_hz, 12, 9, buf));
  Serial.printf("expect %s\n", dtostrf(expect40 / dw3k_time40_hz, 15, 12, buf));
  Serial.printf("actual %s\n", dtostrf(actual40 / dw3k_time40_hz, 15, 12, buf));
  Serial.printf("after  %s\n", dtostrf(after32 / dw3k_time32_hz, 12, 9, buf));
}

void loop() {
  Serial.println("Looping...");
  delay(500);
}
