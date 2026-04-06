#include "matrix.h"

uint16_t mapXY(uint16_t x, uint16_t y) {
    if (x >= PANEL_WIDTH || y >= PANEL_HEIGHT) {
        return NUM_PIXELS;
    }

#if WIRING_PATTERN == WIRING_SERPENTINE_H
    if (y & 1U) {
        return static_cast<uint16_t>(y * PANEL_WIDTH + (PANEL_WIDTH - 1U - x));
    }
    return static_cast<uint16_t>(y * PANEL_WIDTH + x);
#elif WIRING_PATTERN == WIRING_PROGRESSIVE_H
    return static_cast<uint16_t>(y * PANEL_WIDTH + x);
#elif WIRING_PATTERN == WIRING_SERPENTINE_V
    if (x & 1U) {
        return static_cast<uint16_t>(x * PANEL_HEIGHT + (PANEL_HEIGHT - 1U - y));
    }
    return static_cast<uint16_t>(x * PANEL_HEIGHT + y);
#else
    #error "Unknown WIRING_PATTERN"
#endif
}
