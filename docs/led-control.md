# LED Control

Color model, addressable strip protocols (SK6812 RGBW, WS2815B RGB), PWM channels, gamma correction, level shifting, and signal conditioning.

## Color Model

- **RGBW struct:** 4 bytes (r, g, b, w), each 0-255.
- **HSV struct:** hue (0-360), saturation (0-255), value (0-255).
- `hsvToRGBW()` extracts the white component from the minimum of RGB channels. Use this for smooth color transitions with efficient white LED usage.
- `lerpRGBW()` for interpolation between colors. `scaleBrightness()` for dimming.

## Addressable Strips (SK6812 RGBW and WS2815B RGB)

The `LEDStrip` library uses **NeoPixelBus** (by Makuna) as its underlying driver. Strip type is a compile-time template parameter, matching NeoPixelBus's own architecture:

```cpp
#include "led_strip.h"

// SK6812 RGBW
LEDStrip<StripType::SK6812_RGBW> rgbwStrip(5, 30);

// WS2815B RGB
LEDStrip<StripType::WS2815B_RGB> rgbStrip(5, 60);
```

Both strip types use the same `RGBW` color model. For WS2815B (3-channel), the white channel is folded back into RGB at output time (`show()` handles this automatically). This means all color math, animations, and effects work identically regardless of strip type.

### Protocol Comparison

| Feature                | SK6812 RGBW               | WS2815B RGB               |
|------------------------|---------------------------|---------------------------|
| **Channels**           | 4 (R, G, B, W)           | 3 (R, G, B)              |
| **Bits per pixel**     | 32                        | 24                        |
| **Byte order**         | GRB+W                     | GRB                       |
| **NeoPixelBus Feature/Method** | `NeoGrbwFeature` / `Sk6812Method` | `NeoGrbFeature` / `Ws2812xMethod` |
| **Operating voltage**  | 5V                        | 12V                       |
| **Current (full white)** | ~80 mA @ 5V (commonly cited as ~20 mA/ch) | ~30 mA @ 12V (commonly cited as ~10 mA/ch) |
| **Power per LED**      | ~400 mW                   | ~360 mW                   |
| **Reset time**         | >= 80 us                  | >= 280 us                 |
| **Backup data line**   | No                        | Yes (BIN)                 |
| **White channel**      | Dedicated white LED die   | RGB-mixed (no white die)  |
| **Max run (no power injection)** | ~1-2 m (voltage drop at 5V) | Longer than 5V strips (12V = lower voltage drop); varies with brightness and wire gauge |
| **Signal protocol**    | 800 kHz NRZ               | 800 kHz NRZ               |
| **Internal PWM rate**  | ~1.2 kHz                  | ~2 kHz                    |

### Signal Timing Specifications

Both use single-wire NRZ (Non-Return-to-Zero) at approximately 800 kHz:

**SK6812 RGBW Timing:**

| Parameter | Symbol | Typical | Range |
|-----------|--------|---------|-------|
| 0-bit high | T0H | 300 ns | 150-450 ns |
| 0-bit low | T0L | 900 ns | 750-1050 ns |
| 1-bit high | T1H | 600 ns | 450-750 ns |
| 1-bit low | T1L | 600 ns | 450-750 ns |
| Reset | Treset | >= 80 us | |

Bit period: T0H + T0L = T1H + T1L = ~1.25 us. Time per pixel: 32 bits x 1.25 us = **40 us**.

**WS2815B RGB Timing (per datasheet):**

| Parameter | Symbol | Range |
|-----------|--------|-------|
| 0-bit high | T0H | 220-380 ns |
| 0-bit low | T0L | 580-1600 ns |
| 1-bit high | T1H | 580-1600 ns |
| 1-bit low | T1L | 220-420 ns |
| Reset | Treset | >= 280 us |

The WS2815B has significantly wider timing tolerances than the SK6812. The nominal bit period is ~1.25 us (800 kHz) when driven by standard controllers. Time per pixel: 24 bits x 1.25 us = **~30 us**.

**Key difference:** WS2815B has much wider timing tolerances and a 280 us reset (3.5x longer than SK6812's 80 us). If mixing both strip types in the same project (different pins), use the 280 us reset time for all strips to ensure both latch correctly.

### Frame Rate Limits by Strip Length

| Pixel Count | Frame Time (SK6812) | Frame Time (WS2815B) | Max FPS |
|-------------|--------------------|-----------------------|---------|
| 30 | 1.2 ms + reset | 0.9 ms + reset | 500+ |
| 60 | 2.4 ms + reset | 1.8 ms + reset | 300+ |
| 150 | 6.0 ms + 0.08 ms | 4.5 ms + 0.28 ms | ~150+ |
| 300 | 12.0 ms + 0.08 ms | 9.0 ms + 0.28 ms | ~80 |
| 600 | 24.0 ms + 0.08 ms | 18.0 ms + 0.28 ms | ~41 |
| 1000 | 40.0 ms + 0.08 ms | 30.0 ms + 0.28 ms | ~25 |

For smooth animation, target 30+ FPS. This caps practical strip length at ~800 pixels (SK6812) or ~1100 pixels (WS2815B).

### Timing Quirks and Practical Notes

- **Byte order is GRBW, not RGBW.** Green byte is transmitted first. NeoPixelBus's `NeoGrbwFeature` handles this.
- **The +/- 150 ns tolerance is generous** but a T0H of exactly 450 ns sits at the 0/1 boundary and may cause glitches on some batches.
- **Real-world clones vary.** Some SK6812 variants need T0H closer to 350 ns. If you see random color glitches, timing margin is the first suspect.
- **Many implementations use 300+ us reset for safety.** NeoPixelBus uses protocol-appropriate reset times by default. Using exactly 80 us can cause issues on strips with slightly longer latch requirements.
- **Data appears to be latched on the reset pulse (observed behavior, not documented in IC datasheets).** In practice, a partial frame (if interrupted mid-transfer) does not produce a partial update — the strip retains the previous complete frame.
- **Internal PWM refresh (~1.2 kHz for SK6812, ~2 kHz for WS2815).** This is visible in fast-panning camera footage (not to the naked eye). There is no way to change this rate.
- **Each channel has internal constant-current regulation** (commonly cited as ~20 mA per channel for SK6812, ~10 mA for WS2815B). Brightness is stable across a range of supply voltages.
- **Temperature sensitivity.** At extreme temperatures (<-20C or >70C), the internal oscillator can drift, narrowing timing margins. Rarely a problem indoors.

### White Channel Handling

- **SK6812 RGBW:** Has a dedicated white LED die. `hsvToRGBW()` extracts the white component from the minimum of RGB for efficient usage.
- **WS2815B RGB:** No white die. The `LEDStrip::show()` method automatically folds `RGBW.w` back into the RGB channels with clamping (`r = min(r + w, 255)`). Calling code does not need special handling.
- Use `rgbwToRgb()` from RGBWCommon if you need explicit RGBW-to-RGB conversion outside the strip driver.

### SK6812 White LED Color Temperature Variants

SK6812 RGBW is manufactured with different white LED die options:

| Variant | Common Name | CCT | CRI | Notes |
|---------|-------------|-----|-----|-------|
| SK6812-WWA | Warm White | ~2700-3000K | — | Yellowish, like incandescent |
| SK6812-NWA | Neutral White | ~4000-4500K | — | Clean white, most versatile |
| SK6812-CWA | Cool White | ~6500-7000K | — | Bluish, appears brighter |

CCT values are commonly advertised by strip vendors; the SK6812 IC datasheet specifies RGB wavelengths but not white-die CCT. CRI is not consistently specified; verify per batch if color accuracy matters.

**Software implications:**
- **Warm white:** `min(R,G,B)` white extraction shifts colors warm. Use a reduced extraction ratio (0.6-0.8).
- **Cool white:** Shifts colors cool. Also reduce extraction ratio or add a warm RGB bias.
- **Neutral white:** Closest to what `min(R,G,B)` assumes. Best general-purpose option.

**Efficiency:** Dedicated white LED dies are generally significantly more efficient (lm/W) than producing white by mixing RGB — the advantage is commonly cited as 2-3x. Maximizing W channel usage for white and pastel colors is both brighter and more power-efficient.

### WS2815B Backup Data Line (BIN)

The WS2815B has a backup data input (BIN) for fault tolerance:

1. Each IC has DIN (primary) and BIN (backup) inputs.
2. On typical pre-made strips, BIN of LED N is wired to DOUT of LED N-2 (strip-vendor wiring convention, not specified in the IC datasheet).
3. Under normal operation, each IC receives data on DIN, consumes its 24 bits, and forwards on DOUT.
4. If DIN fails (no valid transitions detected), the IC switches to BIN automatically.
5. The dead pixel goes dark, but all downstream LEDs continue functioning (with a one-pixel data offset).

**Limitations:**
- Only survives **single-pixel failures**. Two adjacent dead LEDs break the bypass chain.
- The switchover is automatic -- no software changes needed.
- For custom wiring (not pre-made strips), BIN must be manually wired.

### Level Shifting

Both strip types use 5V-logic data lines (WS2815B has an internal regulator despite 12V power). NeoPixel guidance calls for a signal high of about 70% of pixel supply voltage, so a 3.3V MCU can be marginal when pixels are at 5V. Arduino 5V boards need no level shifting.

**ESP32 level shifting options (ranked):**
1. **74HCT125 quad buffer** (best): Accepts 3.3V input, outputs 5V. Still place a 300-500 ohm resistor near the first pixel (see Signal Conditioning below); an optional 33-ohm resistor at the buffer output can damp ringing on long traces but does not replace the pixel-end resistor.
2. **Sacrificial LED at 3.3V:** Wire one LED powered at 3.3V as the first pixel; its DOUT reshapes the signal to valid logic levels. Works reliably with SK6812; not recommended for WS2815B (12V IC may not operate at 3.3V).
3. **BSS138 MOSFET level shifter:** Generally considered too slow for 800 kHz NRZ data due to asymmetric rise/fall times. Avoid for LED data lines (fine for I2C at 100-400 kHz).

**WS2815B power domain note:** The 12V supply powers the LEDs only. The data line is 5V logic. Never connect 12V to the level shifter. Keep 12V and 5V/3.3V domains separated with common ground.

### Signal Conditioning and Wiring Safety (NeoPixel best practices)

- Add a 500-1000 uF capacitor across + and - at the strip input to buffer inrush/current spikes.
- Place a 300-500 ohm resistor in series with the data line, physically close to the first pixel. This provides impedance matching to reduce reflections.
- Connect ground first, then +V, then data; disconnect in reverse.
- Keep the data line between MCU and first pixel as short as possible; long runs are unreliable.

**Signal integrity on long strips:**
- Each LED IC acts as a signal repeater -- the output waveform is regenerated from the IC's internal oscillator, not passed through. Signal quality does NOT degrade cumulatively over a long strip.
- The critical path is controller-to-first-pixel only. Beyond that, degradation is a power (voltage drop) problem, not a signal problem.
- For long controller-to-first-pixel runs (>30 cm): use a level shifter at the controller end, use shielded twisted-pair cable, and add the 300-500 ohm termination resistor at the pixel end.

### RMT Channel Limits (ESP32)

Multiple `LEDStrip` instances are supported (each gets its own NeoPixelBus driver). Limited by RMT TX channels:

| Chip       | RMT TX | RMT RX | Max strips (RMT) | Notes |
|------------|--------|--------|-------------------|-------|
| ESP32      | 8 total (configurable TX/RX) | -- | up to 8 | Channels are shared between TX and RX |
| ESP32-S2   | 4 total (configurable TX/RX) | -- | up to 4 | Channels are shared between TX and RX |
| ESP32-S3   | 4      | 4      | 4                 | TX and RX channels are separate |
| ESP32-C3   | 2      | 2      | 2                 | |

- `LEDStrip` uses `NeoEsp32RmtN*` methods on ESP32, which support runtime RMT channel assignment. Each `LEDStrip` instance automatically claims the next available RMT TX channel via a static counter. No manual channel assignment is needed — just create multiple `LEDStrip` instances on different pins. If all RMT TX channels are exhausted, `Begin()` will fail. When you run out of RMT TX channels, NeoPixelBus provides I2S and SPI methods as alternatives.
- If using an IR receiver alongside LED strips, assign IR to an RX-only channel so it doesn't consume TX channels.
- **I2S LED output caveat:** Using I2S for LED output conflicts with I2S audio input. Do not use the I2S LED method on the same I2S peripheral as your audio input.

### Electromagnetic Interference (EMI)

Long LED strips are effectively antennas. The 800 kHz data signal and its harmonics can cause interference:
- Keep LED strip runs away from audio cables, antenna wires, and sensitive analog inputs.
- Use ferrite cores on the data line and power cables near the controller if you experience radio interference.
- Keep strip data/power wires at least 5-10 cm away from the ESP32's WiFi antenna area.

## 24V Non-Addressable Strips (COB / SMD)

Non-addressable strips have no data line — brightness and color are controlled entirely by PWM via MOSFETs (the `LEDPWM` library). Each color channel is a continuous circuit switched by its own MOSFET.

### COB (Chip-On-Board) Strips

COB strips use densely packed bare LED dies bonded directly to the PCB, producing a continuous line of light with no visible hotspots. Example product: LBL-COB-280-24V-8MM-100FT-IP20 (280 LEDs/m, 24V, 8mm width). Specs below are representative of this class of strip; actual values vary by manufacturer and density — always check the specific product's datasheet.

| Parameter | Typical Value | Notes |
|-----------|--------------|-------|
| **Operating voltage** | 24V DC | Higher voltage = lower current = longer runs |
| **Power consumption** | ~1.5 W/ft (~5 W/m) | Varies by density and manufacturer; check datasheet |
| **Cut points** | Every 3 LEDs (~10.7 mm at 280/m) | Marked on the strip; cutting elsewhere damages the circuit |
| **Max run (single feed)** | 5-10 m | Depends on power rating and wire gauge; measure voltage at far end |
| **IP rating** | IP20 (bare), IP65 (silicone sleeve) | IP20 for indoor; IP65+ for outdoor/wet |
| **Dimming** | PWM via MOSFET | No data protocol; brightness is purely duty cycle |

### Single-Color vs Multi-Channel Non-Addressable

- **Single-color (warm white, cool white, etc.):** Two wires (+24V and GND). One MOSFET per zone for dimming.
- **RGB:** Four wires (+24V, R, G, B cathodes). Three MOSFETs per zone.
- **RGBW:** Five wires (+24V, R, G, B, W cathodes). Four MOSFETs per zone. Use the `LEDPWM` library directly.

### Power Budget for 24V Non-Addressable

At 24V, current is lower for the same wattage compared to 5V strips:

| Strip Rating | Current per Meter | 5m Run Current | PSU (20% headroom) |
|-------------|-------------------|----------------|---------------------|
| 5 W/m | 0.21 A/m | 1.04 A | 24V 1.5A (36W) |
| 10 W/m | 0.42 A/m | 2.08 A | 24V 2.5A (60W) |
| 14 W/m | 0.58 A/m | 2.92 A | 24V 3.5A (84W) |

Power injection is needed less frequently at 24V than at 5V due to lower current. For runs under 5m at moderate brightness, a single power feed at the head is typically sufficient.

### Software Control

Non-addressable strips are driven by the `LEDPWM` library, not `LEDStrip`. All color math (`hsvToRGBW()`, `lerpRGBW()`, `scaleBrightness()`, gamma correction) applies identically — the difference is only in the output stage (PWM duty cycle instead of NRZ data protocol).

## PWM Channels (MOSFET-driven)

- Use the `LEDPWM` library for discrete R, G, B, W channels via MOSFETs.
- **MOSFET selection:** Use logic-level N-channel MOSFETs (e.g., IRLZ44N, IRLB8721). Select based on low Rds(on) at the actual gate-drive voltage (3.3V from ESP32); check the datasheet Rds(on) at Vgs=3.3V or 4.5V, not just the Vgs(th). If Rds(on) is too high at 3.3V, add a gate driver.
- **Flyback protection:** Add a diode across inductive loads (not needed for LED strips).

### N-Channel MOSFET Wiring

- **Low-side switching** (MOSFET between LED strip and GND): simpler, preferred for LED strips.
- Gate -> ESP32 GPIO pin (through a series gate resistor — see below).
- Source -> GND.
- Drain -> LED strip cathode (R, G, B, or W channel).
- Add 10k ohm pull-down resistor from gate to GND to ensure MOSFET stays off during boot.

**Gate resistor selection (100R vs 220R):** Both values are valid. 100 ohm provides faster switching transitions and is specified in IRLB8721 application notes — use this as the default. 220 ohm provides more conservative switching with additional GPIO current protection and less ringing on longer gate traces — common in hobby circuits and a reasonable choice when switching speed is not critical (PWM frequencies under 50 kHz). Either value works for LED PWM dimming at 20 kHz.

### ESP32 LEDC Resolution/Frequency (APB_CLK = 80 MHz)

The default is 19531 Hz / 12-bit, which balances smooth dimming (4096 levels) with inaudible PWM frequency.

| Resolution (bits) | Max Frequency | Levels | Use Case                    |
|-------------------|---------------|--------|-----------------------------|
| 8                 | 312.5 kHz     | 256    | Simple indicators only      |
| 10                | 78.1 kHz      | 1024   | Acceptable dimming          |
| 12                | 19.5 kHz      | 4096   | Good smooth dimming         |
| 13                | 9.77 kHz      | 8192   | Great for RGBW strips       |
| 16                | 1.22 kHz      | 65536  | Best dimming quality        |

Recommended: **12-bit at ~19.5 kHz** balances smooth dimming with inaudible PWM frequency. On AVR, `analogWrite()` is 8-bit; ~490 Hz on most Uno pins, ~980 Hz on pins 5/6 (Timer0 uses fast PWM mode). Frequency is board- and timer-dependent.

### PWM Frequency Notes

- 19.5 kHz default is above human hearing range and flicker-free.
- Lower frequencies (5 kHz) may cause audible buzzing from MOSFET/inductor resonance.
- ESP32 LEDC max frequency = APB_CLK / (2^resolution). With default 80 MHz APB clock: 312.5 kHz at 8-bit, 19.5 kHz at 12-bit.

## Gamma Correction

Human vision perceives brightness logarithmically. Without gamma correction, 50% duty cycle appears ~75% brightness. Apply gamma correction for perceptually uniform dimming:

```cpp
// Pre-computed gamma 2.2 lookup table (8-bit input -> 8-bit output)
static const uint8_t PROGMEM gammaTable[256] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255
};

RGBW gammaCorrect(const RGBW& color) {
    return RGBW(
        pgm_read_byte(&gammaTable[color.r]),
        pgm_read_byte(&gammaTable[color.g]),
        pgm_read_byte(&gammaTable[color.b]),
        pgm_read_byte(&gammaTable[color.w])
    );
}
```

For higher-resolution PWM (10-16 bit), compute gamma at runtime:
```cpp
uint16_t gammaCorrect16(uint8_t input, uint8_t resolution) {
    float normalized = input / 255.0f;
    float corrected = powf(normalized, 2.2f);
    return (uint16_t)(corrected * ((1 << resolution) - 1));
}
```

### Alternative: CIE L* Curve

The CIE L* (lightness) curve is considered more perceptually accurate than a simple power-law gamma 2.2:

```cpp
// CIE L* lightness curve (8-bit input -> 16-bit output)
uint16_t cieLightness(uint8_t input) {
    float L = input / 255.0f * 100.0f;  // Scale to 0-100
    float Y;
    if (L <= 8.0f) {
        Y = L / 903.3f;
    } else {
        float t = (L + 16.0f) / 116.0f;
        Y = t * t * t;
    }
    return (uint16_t)(Y * 65535.0f);
}
```

NeoPixelBus provides both `NeoGammaTableMethod` (gamma 2.2 lookup) and `NeoGammaCieLabMethod` (CIE L*) built in.
