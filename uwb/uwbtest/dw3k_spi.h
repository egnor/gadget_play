#include <stdint.h>

#include "dw3k_registers.h"

void dw3k_init_spi();
void dw3k_command(DW3KFastCommand);
void dw3k_read(DW3KRegisterAddress, void*, int len);
void dw3k_write(DW3KRegisterAddress, void const*, int len);
void dw3k_maskset8(DW3KRegisterAddress, uint8_t mask, uint8_t set);
void dw3k_maskset16(DW3KRegisterAddress, uint16_t mask, uint16_t set);
void dw3k_maskset32(DW3KRegisterAddress, uint32_t mask, uint32_t set);

template <typename T>
static inline uint32_t dw3k_read(DW3KRegisterAddress addr) {
  T val;
  dw3k_read(addr, &val, sizeof(val));
  return val;
}

template <typename T>
static inline void dw3k_write(DW3KRegisterAddress addr, T const& v) {
  dw3k_write(addr, &v, sizeof(v));
}
