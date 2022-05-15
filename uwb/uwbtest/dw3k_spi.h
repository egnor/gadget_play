#include <stdint.h>

#include "dw3k_registers.h"

void dw3k_command(DW3KFastCommand);
void dw3k_read(DW3KRegisterAddress, void*, uint8_t len);
void dw3k_write(DW3KRegisterAddress, void const*, uint8_t len);
void dw3k_mask8(DW3KRegisterAddress, uint8_t mask, uint8_t set);
void dw3k_mask16(DW3KRegisterAddress, uint16_t mask, uint16_t set);
void dw3k_mask32(DW3KRegisterAddress, uint32_t mask, uint32_t set);

static inline uint32_t dw3k_read32(DW3KRegisterAddress addr) {
  uint32_t val;
  dw3k_read(addr, &val, sizeof(val));
  return val;
}

static inline void dw3k_write32(DW3KRegisterAddress addr, uint32_t v) {
  dw3k_write(addr, &v, sizeof(v));
}
