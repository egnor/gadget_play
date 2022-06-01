#include <SPI.h>

#include "dw3k_driver.h"
#include "dw3k_registers.h"
#include "dw3k_spi.h"

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.printf("Resetting DW3K...\n");
  dw3k_reset();

  for (;;) {
    auto const status = dw3k_poll();
    Serial.printf("DW3K: %s...\n", debug(status));
    if (status == DW3KStatus::Ready) break;
    delay(10);
  }

  Serial.printf("DEV_ID      %08x\n", dw3k_read32(DW3K_DEV_ID));
  Serial.printf("SYS_CFG     %08x\n", dw3k_read32(DW3K_SYS_CFG));
  Serial.printf("SEQ_CTRL    %08x\n", dw3k_read32(DW3K_SEQ_CTRL));
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
