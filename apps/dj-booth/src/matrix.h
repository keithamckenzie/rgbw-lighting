#pragma once

#include <stdint.h>
#include "config.h"

// Map 2D panel coordinates to the linear strip index.
// (0,0) is top-left. Returns NUM_PIXELS when out of bounds.
uint16_t mapXY(uint16_t x, uint16_t y);
