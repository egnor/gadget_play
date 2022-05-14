#include "dw3k_spi.h"

#include <SPI.h>

#include "dwm3k_pins.h"

// See:
// DW3000 Datasheet 4.9.1. "SPI Functional Description"
// DW3000 User Manual 2.3.1.2. "Transaction formats of the SPI interface"

static void begin() {
  digitalWrite(DW3K_CSn_PIN, 0);
  SPI.beginTransaction(SPISettings(36000000, MSBFIRST, SPI_MODE0));
}

static void end() {
  SPI.endTransaction();
  digitalWrite(DW3K_CSn_PIN, 1);
}

static void send(void const* data, uint8_t n) {
  for (uint8_t i = 0; i < n; ++i)
    SPI.transfer(((uint8_t const*) data)[i]);
}

void dw3k_command(DW3KFastCommand command) {
  begin();
  SPI.transfer(0x81 | ((uint8_t) command << 1));
  end();
}

void dw3k_read(DW3KRegisterFile file, uint8_t a, void* data, uint8_t n) {
  auto const f = (uint8_t) file << 1;
  begin();
  SPI.transfer(a ? 0x40 | f | (a >> 6) | ((a & 0x3E) << 10) : f);
  SPI.transfer(data, n);
  end();
}

void dw3k_write(DW3KRegisterFile file, uint8_t a, void const* data, uint8_t n) {
  auto const f = (uint8_t) file << 1;
  begin();
  SPI.transfer(a ? 0xC0 | f | (a >> 6) | ((a & 0x3E) << 10) : 0x80 | f);
  send(data, n);
  end();
}

void dw3k_mask8(DW3KRegisterFile file, uint8_t a, uint8_t ma, uint8_t ms) {
  begin();
  auto const f = (uint8_t) file << 1;
  SPI.transfer(0xC0 | f | (a >> 6) | ((a & 0x3E) << 10) | 0x100);
  send(&ma, sizeof(ma));
  send(&ms, sizeof(ms));
  end();
}

void dw3k_mask16(DW3KRegisterFile file, uint8_t a, uint16_t ma, uint16_t ms) {
  begin();
  auto const f = (uint8_t) file << 1;
  SPI.transfer(0xC0 | f | (a >> 6) | ((a & 0x3E) << 10) | 0x200);
  send(&ma, sizeof(ma));
  send(&ms, sizeof(ms));
  end();
}

void dw3k_mask32(DW3KRegisterFile file, uint8_t a, uint32_t ma, uint32_t ms) {
  begin();
  auto const f = (uint8_t) file << 1;
  SPI.transfer(0xC0 | f | (a >> 6) | ((a & 0x3E) << 10) | 0x300);
  send(&ma, sizeof(ma));
  send(&ms, sizeof(ms));
  end();
}
