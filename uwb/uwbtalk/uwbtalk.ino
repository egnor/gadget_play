#include <SPI.h>

#include "dwm3k_pins.h"
#include "dw3k_registers.h"
#include "dw3k_spi.h"

void setup() {
  Serial.begin(115200);
  Serial.println("Resetting DW3K...");
  digitalWrite(DW3K_RSTn_PIN, 0);
  digitalWrite(DW3K_WAKEUP_PIN, 0);
  pinMode(DW3K_RSTn_PIN, OUTPUT);
  pinMode(DW3K_WAKEUP_PIN, OUTPUT);

  digitalWrite(DW3K_CSn_PIN, 1);
  pinMode(DW3K_CSn_PIN, OUTPUT);
  SPI.begin();

  delay(100);
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
