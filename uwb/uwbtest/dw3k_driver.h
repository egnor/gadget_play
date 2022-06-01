#pragma once

enum class DW3KStatus {
  Unknown,
  ResetActive,
  ResetWaitIRQ,
  ResetWaitPLL,
  CalibrationWait,
  Ready,
};

void dw3k_reset();
DW3KStatus dw3k_poll();

char const* debug(DW3KStatus);
