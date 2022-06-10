#include <Arduino.h>
#include <avr/dtostrf.h>

#include "dw3k.h"

struct PingPong {
  char type[8];
  uint64_t ping_tx_t40;
  uint64_t ping_rx_t40;
  float ping_offset;
  uint64_t pong_tx_t40;
};

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  Serial.printf("Resetting DW3K...\n");
  dw3k_reset();
  dw3k_wait_verbose(DW3KStatus::Ready);
}

void loop() {
  Serial.printf("\nWaiting for PING...\n");

  PingPong message = {};
  dw3k_start_rx();
  dw3k_wait_verbose(DW3KStatus::ReceiveDone);
  if (dw3k_rx_size() != sizeof(message)) {
    Serial.printf("*** Size=%d != %d\n", dw3k_rx_size(), sizeof(message));
  } else {
    dw3k_retrieve_rx(0, sizeof(message), &message);
    if (strncmp(message.type, "PING", sizeof(message.type))) {
      Serial.printf("*** Type [%.8s] != PING\n", message.type);
    } else {
      strcpy(message.type, "PONG");
      message.ping_rx_t40 = dw3k_rx_timestamp_t40();
      message.ping_offset = dw3k_rx_clock_offset();
      dw3k_end_txrx();

      auto const lead_t32 = dw3k_tx_leadtime_t32();
      uint32_t const extra_t32 = 1e-3 * dw3k_time32_hz;
      uint32_t const sched_t32 = dw3k_clock_t32() + lead_t32 + extra_t32;
      message.pong_tx_t40 = dw3k_tx_expected_t40(sched_t32);
      dw3k_buffer_tx(&message, sizeof(message));
      dw3k_schedule_tx(sched_t32);
      Serial.printf("Replying with PONG...\n");
      dw3k_wait_verbose(DW3KStatus::TransmitDone);
    }
  }

  dw3k_end_txrx();
  dw3k_wait_verbose(DW3KStatus::Ready);
}
