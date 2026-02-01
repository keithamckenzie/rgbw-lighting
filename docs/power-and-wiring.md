# Power and Wiring

Power management, wire gauge, connectors, power injection, brownout prevention, and switches/physical input.

## Power Budget

### SK6812 RGBW (5V)

- Each LED draws up to ~80 mA at full white (commonly cited as ~20 mA per channel x 4; not specified in the IC datasheet).
- 30-LED strip = 2.4 A max. 60-LED strip = 4.8 A max.
- Power injection needed every 1-2 meters on longer runs.

### WS2815B RGB (12V)

- Each LED draws up to ~30 mA at full white (commonly cited as ~10 mA per channel x 3) at 12V.
- 30-LED strip = 0.9 A max. 60-LED strip = 1.8 A max.
- Lower current at 12V reduces voltage drop, allowing longer runs before power injection is needed (datasheet claims >5 m signal distance; actual power-drop limit depends on brightness and wire gauge).

The WS2815B's 12V operation results in approximately 1/3 the current of equivalent 5V strips. Typical strip constructions are understood to wire three LED dies in series per channel (~3.3V forward voltage each ≈ 10V, plus ~2V driver headroom), though this is a strip-level detail not specified in the WS2815B IC datasheet.

### Quick Power Budget Table

| Strip | LEDs | Max Current | Max Power | PSU Recommendation (20% headroom) |
|-------|------|------------|-----------|-----------------------------------|
| SK6812 | 30 | 2.4 A @ 5V | 12 W | 5V 3A (15W) |
| SK6812 | 60 | 4.8 A @ 5V | 24 W | 5V 6A (30W) |
| SK6812 | 150 | 12 A @ 5V | 60 W | 5V 15A (75W) |
| WS2815B | 60 | 1.8 A @ 12V | 21.6 W | 12V 2.5A (30W) |
| WS2815B | 150 | 4.5 A @ 12V | 54 W | 12V 6A (72W) |
| WS2815B | 300 | 9.0 A @ 12V | 108 W | 12V 12A (144W) |

### General Rules

- Always calculate worst-case current draw and size power supplies with **20% headroom**.
- Use `strip.setBrightness()` to cap maximum output and protect against overcurrent.
- For WS2815B, do NOT power the ESP32 from the 12V strip supply without a proper voltage regulator (buck converter or LM7805 + AMS1117-3.3).
- For long runs, inject power at multiple points (head, tail, or mid-run). Aim to keep any pixel within ~1 meter of a power feed to reduce voltage drop and color shift.

## Power Injection

### Why It's Needed

LED strip PCB traces have finite resistance (typically 0.1-0.3 ohm/meter for 10mm-wide strips). At high current, this causes voltage drop. Symptoms at the far end:
- Reduced brightness
- Color shift (red stays brightest as it has the lowest forward voltage; blue dims first)
- In extreme cases, the IC drops out and the pixel stops working

### Injection Point Guidelines

| Strip Length | Recommended Injection Points |
|-------------|------------------------------|
| 0-1 m | Head only |
| 1-2 m | Head + tail |
| 2-5 m | Head + tail, or head + middle + tail |
| 5-10 m | Every 2-3 meters (parallel from common PSU) |
| 10+ m | Every 1-2 meters, potentially multiple PSUs |

### Injection Methods

1. **Parallel feed from single PSU:** Run heavy-gauge wires (14-18 AWG) from the PSU to multiple points along the strip. The strip's PCB traces carry only local current between injection points. Most common approach.
2. **Star topology:** Separate power cables from PSU to each injection point. Minimizes voltage drop in feed wires but requires more cable.
3. **Bus bars:** Heavy-gauge wire or copper bar running parallel to the strip with short taps every 1-2 meters. Professional approach for very long runs.

### Measuring Voltage Drop

With the strip running a typical pattern (not all-white worst-case), measure voltage at the first pixel and at the farthest pixel from the power feed. If the difference exceeds 5% of supply (0.25V for 5V, 0.6V for 12V), add more injection points.

### Ground Continuity

The data signal is referenced to ground. If you inject power at multiple points but do not connect the grounds, ground potential can differ between sections, corrupting the data signal. **Always connect all ground points.** Ground must be continuous along the entire strip.

## Wire Gauge

Target voltage drop < 5% of supply (0.25V at 5V, 0.6V at 12V). Drop formula: `V = I * 2 * L * R/m` (factor of 2 for round-trip through + and - wires).

| Scenario      | Min AWG    | Notes |
|---------------|------------|-------|
| 5V, 5A, <2m   | 18 AWG    | Standard for most SK6812 builds |
| 5V, 10A, <2m  | 14 AWG    | Still needs power injection on strip |
| 12V, 2A, <5m  | 22 AWG    | Typical WS2815B single-strip run |
| 12V, 5A, <5m  | 18 AWG    | |

### AWG Resistance Reference

| AWG | Resistance (ohm/m) | Max Current (chassis wiring) |
|-----|--------------------|-----------------------------|
| 14 | 0.0083 | 15 A |
| 16 | 0.0132 | 10 A |
| 18 | 0.0210 | 7 A |
| 20 | 0.0333 | 5 A |
| 22 | 0.0530 | 3 A |
| 24 | 0.0842 | 2 A |

Never go thinner than 22 AWG for power. 18 AWG silicone wire is the practical default for LED projects under 10A.

## Connectors

| Connector          | Rating | Use |
|--------------------|--------|-----|
| JST-SM 3/4-pin     | 3A     | Strip-to-strip (pre-installed on most strips) |
| 5.5x2.1mm barrel   | 3-5A (typical, varies by manufacturer) | 12V PSU input (center-positive) |
| Screw terminal     | 10-30A (varies by terminal size) | High-current power distribution |
| XT60               | 60A (per XT60 spec) | High-power PSU connections |
| Dupont 0.1" header | ~1A    | Prototyping only, never for power |

JST-SM connectors on LED strips are rated at 3A -- do not pass full strip current through them. Use separate heavy-gauge wires with screw terminals for power injection.

### Connector Selection Tips

- **Barrel jacks** are fine for single-strip setups under 3A. For higher current, use screw terminals or XT60.
- **XT60 connectors** are polarized (physically prevent reverse polarity). Good for field-deployable setups.
- **Wago 221 lever nuts** are convenient for prototyping power distribution without soldering.
- Always match connector rating to worst-case current draw, not typical draw.

## Brownout Detection

LED strips switching on cause inrush current that sags the power rail. If the ESP32's 3.3V drops below the brownout threshold (default level varies by ESP-IDF config; typically around 2.4-2.8V), it resets. The ESP32 reboots, LEDs turn off, voltage recovers, and the cycle repeats.

**Hardware mitigations (ranked):**
1. **Separate 3.3V regulation for ESP32** -- Do not power the ESP32 from the same 5V rail as the strips. Use a dedicated regulator with its own PSU connection upstream of the strip wiring.
2. **Bulk capacitance** -- 1000 uF electrolytic + 100 nF ceramic across the 5V/12V input, near the ESP32's regulator input.
3. **Software soft-start** -- Ramp brightness from 0 to target over 50-200 ms to limit dI/dt.

Lowering or disabling the brownout detector (`CONFIG_ESP32_BROWNOUT_DET_LVL`) is a last resort for development. Never disable it in production.

### Soft-Start Implementation

```cpp
void softStart(LEDStrip& strip, uint8_t targetBrightness, uint16_t rampMs = 200) {
    const uint8_t steps = 20;
    const uint16_t stepDelay = rampMs / steps;
    for (uint8_t i = 1; i <= steps; i++) {
        uint8_t b = (uint16_t)targetBrightness * i / steps;
        strip.setBrightness(b);
        strip.show();
        delay(stepDelay);
    }
}
```

## MOSFET Control

### N-Channel MOSFET Wiring (Low-Side Switching)

Low-side switching (MOSFET between LED strip and GND) is simpler and preferred for LED strips:

```
+V ────────────────── LED Strip (+)
                          │
                      LED Strip (channel)
                          │
              Drain ──────┘
MOSFET Gate ←── 100R ── GPIO pin
              Source ──── GND
                │
              10k ── GND (pull-down)
```

- Gate -> ESP32 GPIO pin (through 100 ohm resistor to limit ringing)
- Source -> GND
- Drain -> LED strip cathode (R, G, B, or W channel)
- 10k ohm pull-down from gate to GND to ensure MOSFET stays off during boot

### MOSFET Selection Guide

| MOSFET | Rds(on) @ 4.5V (max) | Rds(on) @ 10V (max) | Max Id | Package | Notes |
|--------|----------------------|---------------------|--------|---------|-------|
| IRLB8721 | 16 mΩ | 8.7 mΩ | 62A | TO-220 | Good logic-level choice |
| IRLZ44N | 35 mΩ | 22 mΩ | 47A | TO-220 | Common, adequate at 4.5V+ |
| AOD4184A | 9.5 mΩ | 7 mΩ | 50A | TO-252 | SMD option |
| Si2302 | 85 mΩ | — | 2.3A | SOT-23 | Small signal only |

> **3.3V gate drive warning:** ESP32 GPIOs output 3.3V, which is below the 4.5V test condition. Rds(on) at Vgs=3.3V is substantially higher than the 4.5V column — check the Vgs-vs-Rds(on) curve in the MOSFET datasheet. For the IRLB8721, typical Rds(on) at 3.3V is in the 30-50 mΩ range. If heat dissipation is critical, use a gate driver or level shifter to provide 5V gate drive.

### Heat Dissipation

Power dissipated in MOSFET: P = I^2 * Rds(on)

| Current | Rds(on) = 18 mohm | Rds(on) = 50 mohm |
|---------|-------------------|-------------------|
| 1 A | 18 mW | 50 mW |
| 3 A | 162 mW | 450 mW |
| 5 A | 450 mW | 1.25 W (needs heatsink) |

For currents above 3A with higher-Rds(on) MOSFETs, add a heatsink or use a lower-Rds(on) part.

## Switches and Physical Input

### Button Debouncing

- Use hardware debouncing (100 nF capacitor across the switch) or software debouncing.
- Software debounce: ignore state changes within 50 ms of the last change.
- Use interrupts (`attachInterrupt()`) for responsive button handling; set a flag and process in the main loop.
- Configure `INPUT_PULLUP` to avoid floating pins.

### Software Debounce Pattern

```cpp
volatile bool buttonFlag = false;
uint32_t lastDebounce = 0;
const uint32_t DEBOUNCE_MS = 50;

void IRAM_ATTR buttonISR() {
    buttonFlag = true;
}

void setup() {
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_BUTTON), buttonISR, FALLING);
}

void loop() {
    if (buttonFlag && (millis() - lastDebounce > DEBOUNCE_MS)) {
        buttonFlag = false;
        lastDebounce = millis();
        // Handle button press
        cycleMode();
    }
}
```

### Rotary Encoders

- Read in an ISR or use a dedicated library (ESP32Encoder).
- Quadrature decoding requires reading both channels on each edge.
- The ESP32's PCNT (Pulse Counter) peripheral can decode quadrature in hardware, freeing the CPU entirely. Libraries like ESP32Encoder use this.

### Long Press / Short Press Detection

```cpp
uint32_t pressStart = 0;
bool wasPressed = false;

void checkButton() {
    bool pressed = !digitalRead(PIN_BUTTON);  // Active LOW with pullup
    if (pressed && !wasPressed) {
        pressStart = millis();
        wasPressed = true;
    }
    if (!pressed && wasPressed) {
        uint32_t duration = millis() - pressStart;
        wasPressed = false;
        if (duration > 1000) {
            handleLongPress();   // e.g., toggle power
        } else if (duration > DEBOUNCE_MS) {
            handleShortPress();  // e.g., cycle mode
        }
    }
}
```
