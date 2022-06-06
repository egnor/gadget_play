#include <Arduino.h>
#include <avr/dtostrf.h>

#include "dw3k.h"

static void wait_for(DW3KStatus wanted) {
  auto p = DW3KStatus::Invalid;
  for (int i = 0;; ++i) {
    auto const s = dw3k_poll();
    if (s != p || !(i % 10000)) Serial.printf(">> %s...\n", dw3k_status_text());
    if (s == wanted) break;
    delayMicroseconds(10);
    p = s;
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  Serial.printf("Resetting DW3K...\n");
  dw3k_reset();
  wait_for(DW3KStatus::Ready);
}

void loop() {
  char msg[] = "TEST_TX HELLO";
  Serial.printf("\nSending [%s]...\n", msg);
  dw3k_buffer_tx(msg, sizeof(msg));

  auto const lead_t32 = dw3k_tx_leadtime_t32();
  uint32_t const extra_t32 = 50e-6 * dw3k_time32_hz;
  dw3k_schedule_tx(dw3k_clock_t32() + lead_t32 + extra_t32);
  wait_for(DW3KStatus::TransmitDone);
  dw3k_end_txrx();
  wait_for(DW3KStatus::Ready);
  delay(500);
}
