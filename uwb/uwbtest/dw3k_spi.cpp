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

static void send_data(void const* data, uint8_t n) {
  for (uint8_t i = 0; i < n; ++i)
    SPI.transfer(((uint8_t const*) data)[i]);
}

void dw3k_command(DW3KFastCommand command) {
  begin();
  SPI.transfer(0x81 | (command.bits << 1));
  end();
}

static void send_header(DW3KRegisterAddress addr, bool wr, uint8_t mbits) {
  if (!mbits && !addr.offset) {
    SPI.transfer((wr ? 0x80 : 0) | (addr.file << 1));
  } else {
    SPI.transfer((wr ? 0xC0 : 0x40) | (addr.file << 1) | (addr.offset >> 6));
    SPI.transfer((addr.offset << 2) | mbits);
  }
}

void dw3k_read(DW3KRegisterAddress addr, void* data, uint8_t n) {
  begin();
  send_header(addr, false, 0);
  SPI.transfer(data, n);
  end();
}

void dw3k_write(DW3KRegisterAddress addr, void const* data, uint8_t n) {
  begin();
  send_header(addr, true, 0);
  send_data(data, n);
  end();
}

void dw3k_mask8(DW3KRegisterAddress addr, uint8_t mask, uint8_t set) {
  begin();
  send_header(addr, true, 1);
  send_data(&mask, sizeof(mask));
  send_data(&set, sizeof(set));
  end();
}

void dw3k_mask16(DW3KRegisterAddress addr, uint16_t mask, uint16_t set) {
  begin();
  send_header(addr, true, 2);
  send_data(&mask, sizeof(mask));
  send_data(&set, sizeof(set));
  end();
}

void dw3k_mask32(DW3KRegisterAddress addr, uint32_t mask, uint32_t set) {
  begin();
  send_header(addr, true, 3);
  send_data(&mask, sizeof(mask));
  send_data(&set, sizeof(set));
  end();
}
