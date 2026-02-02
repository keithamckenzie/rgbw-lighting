#pragma once

#include <stdint.h>
#include "config.h"

// Map 2D panel coordinates to linear pixel index.
// (0,0) is top-left. x increases rightward, y increases downward.
// Returns NUM_PIXELS if out of bounds.
uint16_t mapXY(uint16_t x, uint16_t y);
