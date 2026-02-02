#include "matrix.h"

uint16_t mapXY(uint16_t x, uint16_t y) {
    if (x >= PANEL_WIDTH || y >= PANEL_HEIGHT) {
        return NUM_PIXELS;  // out of bounds sentinel
    }

#if WIRING_PATTERN == WIRING_SERPENTINE_H
    // Serpentine horizontal: even rows left-to-right, odd rows right-to-left
    if (y & 1) {
        return y * PANEL_WIDTH + (PANEL_WIDTH - 1 - x);
    }
    return y * PANEL_WIDTH + x;

#elif WIRING_PATTERN == WIRING_PROGRESSIVE_H
    // Progressive horizontal: all rows left-to-right
    return y * PANEL_WIDTH + x;

#elif WIRING_PATTERN == WIRING_SERPENTINE_V
    // Serpentine vertical: even columns top-to-bottom, odd columns bottom-to-top
    if (x & 1) {
        return x * PANEL_HEIGHT + (PANEL_HEIGHT - 1 - y);
    }
    return x * PANEL_HEIGHT + y;

#else
    #error "Unknown WIRING_PATTERN"
#endif
}
