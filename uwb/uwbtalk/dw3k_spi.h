#include <stdint.h>

#include <SPI.h>

#include "dw3k_registers.h"

// DW3000 Datasheet 4.9.1. "SPI Functional Description"
static const SPISettings DW3K_SPI_SETTINGS(36000000, MSBFIRST, SPI_MODE0);

// DW3000 User Manual 2.3.1.2. "Transaction formats of the SPI interface"
static constexpr uint8_t dw3k_fast_command_header(DW3KFastCommand command) {
  return 0x81 | ((uint8_t) command << 1);
}

static constexpr uint8_t dw3k_short_read_header(DW3KRegisterFile file) {
  return ((uint8_t) file << 1);
}

static constexpr uint8_t dw3k_short_write_header(DW3KRegisterFile file) {
  return 0x80 | ((uint8_t) file << 1);
}

static constexpr uint16_t dw3k_read_header(DW3KRegisterFile file, uint8_t a) {
  return 0x40 | ((uint8_t) file << 1) | (a >> 6) | ((a & 0x3E) << 10);
}

static constexpr uint16_t dw3k_write_header(DW3KRegisterFile file, uint8_t a) {
  return 0xC0 | ((uint8_t) file << 1) | (a >> 6) | ((a & 0x3E) << 10);
}

static constexpr uint16_t dw3k_mask8_header(DW3KRegisterFile file, uint8_t a) {
  return 0xC0 | ((uint8_t) file << 1) | (a >> 6) | ((a & 0x3E) << 10) | 0x100;
}

static constexpr uint16_t dw3k_mask16_header(DW3KRegisterFile file, uint8_t a) {
  return 0xC0 | ((uint8_t) file << 1) | (a >> 6) | ((a & 0x3E) << 10) | 0x200;
}

static constexpr uint16_t dw3k_mask32_header(DW3KRegisterFile file, uint8_t a) {
  return 0xC0 | ((uint8_t) file << 1) | (a >> 6) | ((a & 0x3E) << 10) | 0x300;
}
