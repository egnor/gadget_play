#include <SPI.h>

#include "dwm3k_pins.h"
#include "dw3k_registers.h"
#include "dw3k_spi.h"

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.printf("Resetting DW3K...\n");
  digitalWrite(DW3K_RSTn_PIN, 0);
  digitalWrite(DW3K_WAKEUP_PIN, 0);
  pinMode(DW3K_RSTn_PIN, OUTPUT);
  pinMode(DW3K_WAKEUP_PIN, OUTPUT);

  delay(100);
  pinMode(DW3K_RSTn_PIN, INPUT_PULLUP);
  pinMode(DW3K_IRQ_PIN, INPUT);
  while (!digitalRead(DW3K_IRQ_PIN)) {
    Serial.printf("Waiting for DW3K IRQ...\n");
    delay(100);
  }

  Serial.println("DW3K ready (/RST, IRQ)!");
  dw3k_init();

  Serial.printf("DEV_ID      %08x\n", dw3k_read32(DW3K_DEV_ID));
  Serial.printf("SYS_CFG     %08x\n", dw3k_read32(DW3K_SYS_CFG));

  dw3k_maskset32(DW3K_SEQ_CTRL, ~0u, (1u << 8));
  Serial.printf("SEQ_CTRL*   %08x\n", dw3k_read32(DW3K_SEQ_CTRL));

  while (!(dw3k_read32(DW3K_SYS_STATUS_LO) & (1u << 1))) {
    Serial.printf("Waiting for DW3K CPLOCK\n");
    delay(10);
  }

  dw3k_maskset32(DW3K_RX_CAL, ~1u, (1u << 4) | 1);
  while (!dw3k_read32(DW3K_RX_CAL_STS)) {
    Serial.printf("Waiting for DW3K RX_CAL_STS\n");
    delay(100);
  }
  dw3k_maskset32(DW3K_RX_CAL, ~1u, 0);
  Serial.printf("RX_CAL_RESI %08x\n", dw3k_read32(DW3K_RX_CAL_RESI));
  Serial.printf("RX_CAL_RESQ %08x\n", dw3k_read32(DW3K_RX_CAL_RESQ));

  Serial.printf(
      "SYS_STATUS  %08x:%08x\n",
      dw3k_read32(DW3K_SYS_STATUS_HI),
      dw3k_read32(DW3K_SYS_STATUS_LO)
  );

  Serial.printf("SYS_TIME    %08x\n", dw3k_read32(DW3K_SYS_TIME));
  dw3k_write32(DW3K_SYS_TIME, 0);
  Serial.printf("SYS_TIME+   %08x\n", dw3k_read32(DW3K_SYS_TIME));
}

void loop() {
  Serial.println("Looping...");
  delay(500);
}
