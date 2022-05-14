#include <stdint.h>

#include "dw3k_registers.h"

void dw3k_command(DW3KFastCommand);
void dw3k_read(DW3KRegisterFile, uint8_t off, void*, uint8_t len);
void dw3k_write(DW3KRegisterFile, uint8_t off, void const*, uint8_t len);
void dw3k_mask8(DW3KRegisterFile, uint8_t off, uint8_t mask, uint8_t set);
void dw3k_mask16(DW3KRegisterFile, uint8_t off, uint16_t mask, uint16_t set);
void dw3k_mask32(DW3KRegisterFile, uint8_t off, uint32_t mask, uint32_t set);

static inline uint32_t dw3k_read32(DW3KRegisterFile f, uint8_t off) {
  uint32_t val;
  dw3k_read(f, off, &val, sizeof(val));
  return val;
}

static inline void dw3k_write32(DW3KRegisterFile f, uint8_t off, uint32_t v) {
  dw3k_write(f, off, &v, sizeof(v));
}
