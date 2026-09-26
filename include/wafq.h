#pragma once
#include <Arduino.h>

// Classical 4x4 wafq: base square (constant 34) shifted so every row, column
// and main diagonal sums to `total`. Cells 13..16 sit one per row/column/diagonal
// and absorb the remainder. Needs total >= 30.
static bool wafq(long total, long out[16]) {
  static const uint8_t BASE[16] = {8, 11, 14, 1, 13, 2, 7, 12, 3, 16, 9, 6, 10, 5, 4, 15};
  if (total < 30) return false;
  long q = (total - 30) / 4, r = (total - 30) % 4;
  for (int i = 0; i < 16; i++) out[i] = BASE[i] - 1 + q + (BASE[i] >= 13 ? r : 0);
  return true;
}
