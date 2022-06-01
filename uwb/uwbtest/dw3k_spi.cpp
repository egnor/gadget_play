// Basic SPI interface to the DW3000 chip (within the DWM3000 module).
//
// See:
// DW3000 Datasheet 4.9.1. "SPI Functional Description"
// DW3000 User Manual 2.3.1.2. "Transaction formats of the SPI interface"

#include "dw3k_spi.h"

#include <SPI.h>
#include "wiring_private.h"  // for pinPeripheral()

#include "dwm3k_pins.h"

static SPIClass* spi = nullptr;

static void begin() {
  digitalWrite(DW3K_CSn_PIN, 0);
  spi->beginTransaction(SPISettings(36000000, MSBFIRST, SPI_MODE0));
}

static void end() {
  spi->endTransaction();
  digitalWrite(DW3K_CSn_PIN, 1);
}

static void send_data(void const* data, uint8_t n) {
  for (uint8_t i = 0; i < n; ++i)
    spi->transfer(((uint8_t const*) data)[i]);
}

void dw3k_init_spi() {
  if (!spi) {
    digitalWrite(DW3K_CSn_PIN, 1);
    pinMode(DW3K_CSn_PIN, OUTPUT);
#if ARDUINO_ARCH_AVR
    spi = &SPI;
    spi->begin();
#endif
#if ARDUINO_ARCH_SAMD
    // https://learn.adafruit.com/using-atsamd21-sercom-to-add-more-spi-i2c-serial-ports/creating-a-new-spi
    spi = new SPIClass(
      &sercom3, DW3K_MISO_PIN, DW3K_CLK_PIN, DW3K_MOSI_PIN,
      SPI_PAD_3_SCK_1, SERCOM_RX_PAD_0
    );
    spi->begin();  // Must come before pinPeripheral() apparently?
    pinPeripheral(DW3K_MISO_PIN, PIO_SERCOM_ALT);
    pinPeripheral(DW3K_CLK_PIN, PIO_SERCOM_ALT);
    pinPeripheral(DW3K_MOSI_PIN, PIO_SERCOM_ALT);
#endif
  }
}

void dw3k_command(DW3KFastCommand command) {
  begin();
  spi->transfer(0x81 | (command.bits << 1));
  end();
}

static void send_header(DW3KRegisterAddress addr, bool wr, uint8_t mbits) {
  if (!mbits && !addr.offset) {
    spi->transfer((wr ? 0x80 : 0) | (addr.file << 1));
  } else {
    spi->transfer((wr ? 0xC0 : 0x40) | (addr.file << 1) | (addr.offset >> 6));
    spi->transfer((addr.offset << 2) | mbits);
  }
}

void dw3k_read(DW3KRegisterAddress addr, void* data, uint8_t n) {
  begin();
  send_header(addr, false, 0);
  spi->transfer(data, n);
  end();
}

void dw3k_write(DW3KRegisterAddress addr, void const* data, uint8_t n) {
  begin();
  send_header(addr, true, 0);
  send_data(data, n);
  end();
}

void dw3k_maskset8(DW3KRegisterAddress addr, uint8_t mask, uint8_t set) {
  begin();
  send_header(addr, true, 1);
  send_data(&mask, sizeof(mask));
  send_data(&set, sizeof(set));
  end();
}

void dw3k_maskset16(DW3KRegisterAddress addr, uint16_t mask, uint16_t set) {
  begin();
  send_header(addr, true, 2);
  send_data(&mask, sizeof(mask));
  send_data(&set, sizeof(set));
  end();
}

void dw3k_maskset32(DW3KRegisterAddress addr, uint32_t mask, uint32_t set) {
  begin();
  send_header(addr, true, 3);
  send_data(&mask, sizeof(mask));
  send_data(&set, sizeof(set));
  end();
}
