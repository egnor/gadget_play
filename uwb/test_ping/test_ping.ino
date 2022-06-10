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
  PingPong m = {};
  strcpy(m.type, "PING");
  Serial.printf("\nSending PING...\n", m.type);

  auto const lead_t32 = dw3k_tx_leadtime_t32();
  uint32_t const extra_t32 = 200e-6 * dw3k_time32_hz;
  uint32_t const sched_t32 = dw3k_clock_t32() + lead_t32 + extra_t32;
  m.ping_tx_t40 = dw3k_tx_expected_t40(sched_t32);
  dw3k_buffer_tx(&m, sizeof(m));
  dw3k_schedule_tx(sched_t32);
  dw3k_wait_verbose(DW3KStatus::TransmitDone);
  dw3k_end_txrx();
  dw3k_wait_verbose(DW3KStatus::Ready);

  Serial.printf("\nWaiting for PONG...\n");
  dw3k_start_rx();
  if (!dw3k_wait_verbose(DW3KStatus::ReceiveDone, 200)) {
    Serial.printf("*** No response\n");
  } else if (dw3k_rx_size() != sizeof(m)) {
    Serial.printf("*** Size=%d != %d\n", dw3k_rx_size(), sizeof(m));
  } else {
    dw3k_retrieve_rx(0, sizeof(m), &m);
    if (strncmp(m.type, "PONG", sizeof(m.type))) {
      Serial.printf("*** Type [%.8s] != PONG\n", m.type);
    } else {
      char n1[40], n2[40], n3[40];
      Serial.printf(
          "\nPING %ss --> %ss %sppm offset\n",
          dtostrf(m.ping_tx_t40 / dw3k_time40_hz, 15, 12, n1),
          dtostrf(m.ping_rx_t40 / dw3k_time40_hz, 15, 12, n2),
          dtostrf(m.ping_offset * 1e6, 8, 3, n3)
      );

      uint64_t const pong_rx_t40 = dw3k_rx_timestamp_t40();
      auto const local_s = (pong_rx_t40 - m.ping_tx_t40) / dw3k_time40_hz;
      auto const remote_s = (m.pong_tx_t40 - m.ping_rx_t40) / dw3k_time40_hz;
      Serial.printf(
          " dt  %ss  -  %ss = %ss (raw)\n",
          dtostrf(local_s, 15, 12, n1), dtostrf(remote_s, 15, 12, n2),
          dtostrf(local_s - remote_s, 15, 12, n3)
      );

      float const pong_offset = dw3k_rx_clock_offset();
      auto const offset = (pong_offset - m.ping_offset) / 2;
      auto const remote_adj_s = remote_s + remote_s * offset;
      Serial.printf(
          " dt  %ss  -  %ss = %ss (adjusted)\n",
          dtostrf(local_s, 15, 12, n1),
          dtostrf(remote_adj_s, 15, 12, n2),
          dtostrf(local_s - remote_adj_s, 15, 12, n3)
      );

      Serial.printf(
          "PONG %ss <-- %ss %sppm offset\n\n",
          dtostrf(pong_rx_t40 / dw3k_time40_hz, 15, 12, n1),
          dtostrf(m.pong_tx_t40 / dw3k_time40_hz, 15, 12, n2),
          dtostrf(pong_offset * 1e6, 8, 3, n3)
      );
    }
  }

  dw3k_end_txrx();
  dw3k_wait_verbose(DW3KStatus::Ready);
  delay(500);
}
