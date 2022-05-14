#include <SPI.h>

#include "dwm3k_pins.h"
#include "dw3k_registers.h"
#include "dw3k_spi.h"

void setup() {
  Serial.begin(115200);
  Serial.println("Resetting DW3K...");
  digitalWrite(DW3K_RSTn_PIN, 0);
  digitalWrite(DW3K_IRQ_PIN, 0);
  digitalWrite(DW3K_WAKEUP_PIN, 0);
  pinMode(DW3K_EXTON_PIN, INPUT);

  digitalWrite(DW3K_CLK_PIN, 0);
  pinMode(DW3K_MISO_PIN, INPUT_PULLUP);
  digitalWrite(DW3K_MOSI_PIN, 0);
  digitalWrite(DW3K_CSn_PIN, 1);
  SPI.begin();

  pinMode(DW3K_GPIO0_PIN, INPUT);
  pinMode(DW3K_GPIO1_PIN, INPUT);
  pinMode(DW3K_GPIO2_LED2_PIN, INPUT);
  pinMode(DW3K_GPIO3_LED3_PIN, INPUT);
  pinMode(DW3K_GPIO4_PIN, INPUT);
  digitalWrite(DW3K_GPIO5_SPIPOL_PIN, 0);
  digitalWrite(DW3K_GPIO6_SPIPHA_PIN, 0);
  pinMode(DW3K_GPIO7_PIN, INPUT);

  delay(100);
  pinMode(DW3K_IRQ_PIN, INPUT);
  pinMode(DW3K_RSTn_PIN, INPUT);
  while (!digitalRead(DW3K_RSTn_PIN)) {
    Serial.println("Waiting for DW3K /RST");
    delay(100);
  }
  while (!digitalRead(DW3K_IRQ_PIN)) {
    Serial.println("Waiting for DW3K IRQ");
    delay(100);
  }
  Serial.println("DW3K ready (/RST, IRQ)!");

  Serial.print("DW3K DEV_ID: ");
  Serial.print(dw3k_read32(DW3KRegisterFile::GEN_CFG_AES0, 0x0), HEX);
  Serial.println();
}

void loop() {
  Serial.println("Hello!");
  delay(500);
}
