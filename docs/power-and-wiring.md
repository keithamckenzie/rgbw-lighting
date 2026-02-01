# Power and Wiring

Power management, wire gauge, connectors, power injection, brownout prevention, and switches/physical input.

## Safety and Protection

### AC/DC Separation

All circuits in this project are **low-voltage DC** (3.3V, 5V, 12V, 24V). Mains AC wiring (120V/240V) must be handled by the power supply only.

- **Never expose mains wiring** inside the same enclosure as DC control electronics unless the enclosure is rated for it and AC/DC sections are physically separated with appropriate creepage/clearance distances.
- Power supplies with IEC inlets or hardwired AC connections must have strain relief and an earth ground bond where required by the supply's certification.
- If mounting a PSU inside a project enclosure, use an enclosed/caged supply (not an open-frame board) to prevent accidental contact with AC terminals.

### Fusing

Add fuses to protect wiring and prevent fire in fault conditions:

| Location | Fuse Type | Sizing Rule of Thumb | Purpose |
|----------|-----------|---------------------|---------|
| **PSU output (main)** | Blade fuse or inline holder | Above normal load, below wire ampacity | Protects main power bus and PSU from short circuits |
| **Per-run / per-zone** | Inline fuse holder or PTC resettable fuse | Above zone's normal load, below wire ampacity | Protects individual wire runs from overload |

- Size fuses above normal operating current but below the wire's ampacity. Example: a zone drawing 3A max on 18 AWG wire (7A rated) should have a 5A fuse.
- Fuse manufacturers derate for continuous loads and elevated ambient temperatures (a fuse rated at 5A may nuisance-trip at sustained 5A depending on ambient temp and fuse type). Consult the fuse manufacturer's derating curve and select a rating that provides margin above your expected continuous load. As a starting point, 125-150% of normal operating current is common, but verify against the specific fuse's datasheet.
- PTC resettable fuses (polyfuses) are convenient for per-zone protection — they self-reset after the fault is cleared, avoiding the need to replace fuse elements. Note that PTC trip times are slower than blade fuses.
- Place the main fuse as close to the PSU output as practical.

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

### Future-Proofing Wiring for Addressable Strips

When running wire through walls, conduit, or other hard-to-access paths, plan for future strip upgrades:

- **Pull extra conductors.** A minimum 4-conductor cable (V+, GND, DATA, spare) covers current single-data-line strips (SK6812, WS2815B) plus a spare for clock-based protocols (APA102/SK9822 use a separate clock line) or a backup data line.
- **Cat5/UTP cable** (8 conductors, 24 AWG solid) is a practical option for signal + light-duty power over moderate distances. Pair usage: one pair for data + GND, remaining pairs paralleled for V+ and GND to reduce resistance. Ampacity of 24 AWG varies with insulation type, bundling, and ambient temperature — treat Cat5 as suitable for signal and low-power runs only, not high-current feeds. Check the specific cable's datasheet for allowable current if carrying meaningful power.
- **Recommended gauge for combined power + signal:** 18 AWG for power conductors, 22-24 AWG for signal. Pre-made 4-pin or 5-pin LED extension cable (typically 20-22 AWG) works for runs under 3m at moderate current.
- **Label both ends** of every cable run with zone name and conductor assignment. Use consistent color coding (e.g., red = V+, black = GND, green = DATA, white = spare/CLK).

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
template<StripType T>
void softStart(LEDStrip<T>& strip, uint8_t targetBrightness, uint16_t rampMs = 200) {
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
MOSFET Gate ←── 100R-220R ── GPIO pin
              Source ──── GND
                │
              10k ── GND (pull-down)
```

- Gate -> ESP32 GPIO pin (through 100-220 ohm series resistor to limit ringing; see [gate resistor selection](led-control.md#n-channel-mosfet-wiring) for tradeoffs)
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

## Line-Voltage Fixtures (Pot Lights, Hardwired Fixtures)

This project focuses on low-voltage DC LED strips, but many installations include line-voltage fixtures (recessed pot lights, hardwired sconces) alongside strip lighting. These fixtures cannot be directly controlled by an ESP32 GPIO or MOSFET.

### Control Options for Line-Voltage Fixtures

| Method | How It Works | ESP32 Integration | Notes |
|--------|-------------|-------------------|-------|
| **Smart switch/relay** | WiFi/Zigbee relay replaces wall switch | HTTP/MQTT commands from ESP32 | Simplest; on/off only (no dimming) |
| **Triac dimmer** | Phase-cut AC dimming (leading or trailing edge) | Requires isolated AC dimmer hardware (zero-cross detector + optotriac/triac stage), or use a commercial smart dimmer with WiFi/Zigbee | Requires dimmable LED driver/bulb; trailing-edge preferred for LED. MCU-controlled phase-cut is possible but involves mains-voltage circuitry — use proper isolation. |
| **0-10V dimming** | Analog 0-10V signal controls driver brightness | Depends on driver type — see 0-10V note below | Requires 0-10V compatible LED driver; smooth dimming, no flicker |
| **DALI** | Digital addressable lighting interface (2-wire bus) | Requires a DALI controller/gateway; ESP32 talks to the gateway via serial/IP | Professional/commercial systems; not practical for DIY |

### 0-10V Dimming: Sink vs Source

0-10V dimming interfaces come in two types — check the LED driver's datasheet before wiring:

- **Current-sink (most common):** The LED driver provides 10V on its dim wire and expects the controller to sink current to ground through a variable resistance or active circuit. The driver reads the resulting voltage to set brightness. 10V = full brightness, 0V = off (or minimum).
- **Current-source:** The controller must provide the 0-10V signal. Less common in architectural LED drivers.

**Sink circuit for ESP32 (current-sink drivers):**

The driver sources ~10V on its DIM+ wire. An NPN transistor (open-collector) controlled by the ESP32 sinks that line toward ground. PWM duty cycle sets the average voltage the driver sees.

```
LED Driver DIM+ ───┬─── 10k (pull-up to driver's 10V, keeps line high when transistor is off)
                    │
                    ├─── Collector
                    │
              NPN (2N2222 / BC547)
                    │
                    Emitter ──── LED Driver DIM- (GND)
                    │
              Base ──── 1k ──── ESP32 GPIO (PWM)
```

- **PWM low (transistor off):** DIM+ floats to ~10V via the driver's internal pull-up. Full brightness.
- **PWM high (transistor on):** DIM+ pulled to ~0V. Minimum brightness / off.
- **PWM intermediate:** RC-filtered average voltage between 0-10V. Add a 10k + 1 uF RC filter on the DIM+ line if the driver doesn't tolerate PWM ripple.

The PWM sense is inverted (higher duty cycle = dimmer). Invert in software: `dutyCycle = maxDuty - desiredLevel`.

An optocoupler (e.g., PC817) can replace the NPN transistor for galvanic isolation between the ESP32 and the driver's control circuit.

**For current-source drivers:** The controller must provide 0-10V. The ESP32 DAC (0-3.3V) needs a non-inverting amplifier (op-amp with gain of ~3, powered from 12V) to produce 0-10V. This configuration is less common in architectural LED drivers.

**Key point:** Line-voltage fixture dimming is a function of the fixture's driver, not something the ESP32 controls directly. The ESP32 can send commands to smart switches/dimmers over WiFi, generate a 0-10V analog signal (with appropriate interface circuitry), or trigger a triac dimmer through isolated hardware — but it cannot switch or dim mains AC with a GPIO alone.

For integrated systems (LED strips + pot lights controlled together), use WiFi-based smart switches for the pot lights and coordinate them with the ESP32 via MQTT or HTTP API calls.

## Multi-Zone Architecture

A multi-zone setup uses independent MOSFET channels to control separate LED strip sections (zones) from a single ESP32. Each zone gets its own PWM output pin and MOSFET, allowing independent brightness and color control.

### Home-Run Wiring Topology

All wire runs originate from a central control location (where the ESP32 and power supplies are located) and run to each zone individually. This is the simplest topology and avoids daisy-chaining power through long strip runs.

```
                    ┌── Zone 1 (perimeter)
                    ├── Zone 2 (corners)
ESP32 + PSU(s) ─────├── Zone 3 (chevrons)
                    ├── Zone 4 (accent)
                    └── Zone N ...
```

Each zone has:
- One GPIO pin driving a MOSFET gate (through 100-220R series resistor)
- One N-channel MOSFET per color channel (1 for single-color, 3 for RGB, 4 for RGBW)
- Dedicated power and ground wires back to the PSU

### Centralized vs Distributed MOSFET Placement

| Approach | Description | Pros | Cons |
|----------|-------------|------|------|
| **Centralized** | All MOSFETs at the control board location; switched power runs to each zone | Single enclosure, easy debugging, short gate traces | Heavier wire to remote zones (carrying full load current) |
| **Distributed** | MOSFETs at or near each zone; only signal + unswitched power in the cable run | Lighter signal wiring to zones, shorter high-current paths | Multiple enclosures, harder to service, gate signal integrity over long runs |

For most installations, **centralized placement** is simpler. Use it when wire runs are under 5-10m and current per zone is under 5A. Distributed placement is worth considering for long runs (>10m) or high-current zones where voltage drop in the switched-side wires becomes a problem.

### Installation and Pre-Wire Strategy

- **Access panels:** If running wires through walls or ceilings, plan access points at the control location and at each zone endpoint. Fishing wire through closed walls after the fact is difficult.
- **Conduit:** Use flexible conduit (smurf tube / ENT) or raceway for wire runs that may need future replacement or additions. This is especially valuable in finished spaces.
- **Slack:** Leave 30-60 cm of slack at each endpoint (control box and zone) to allow for retermination, connector changes, or repositioning.
- **Labeling:** Label both ends of every wire run with zone name, conductor assignment, and wire gauge. Heat-shrink labels or cable tags survive longer than tape.

### Zone Count vs GPIO Budget

The ESP32 has more than enough safe GPIO pins for typical zone counts:

| Zones | MOSFETs (single-color) | MOSFETs (RGBW) | GPIO Pins Needed |
|-------|----------------------|----------------|------------------|
| 6 | 6 | 24 | 6-24 |
| 9 | 9 | 36 | 9-36 (use shift register for RGBW) |
| 11 | 11 | 44 | 11 (single-color) or shift register |

For single-color zones (one MOSFET per zone), 11 zones easily fit within the ESP32's safe GPIO budget. For RGBW zones (4 MOSFETs each), consider using a shift register (e.g., 74HC595) or PCA9685 I2C PWM driver to expand outputs.

See [pin-reference.md](pin-reference.md) for safe GPIO assignments.

### Mixing Voltage Domains (12V and 24V)

When different zones use different voltage strips (e.g., 12V addressable and 24V non-addressable):

- Each voltage domain needs its own power supply (or a multi-output supply).
- **Common ground is required** between all PSUs and the ESP32. Connect all PSU negative terminals together and to the ESP32 GND.
- MOSFETs are voltage-agnostic on the drain side — the same IRLB8721 works for both 12V and 24V zones.
- Keep the different voltage wires clearly labeled and physically separated to prevent accidental cross-connection.
- The ESP32 gate drive (3.3V) is independent of the load voltage.

### Multiple Power Supply Management

- Size each PSU for its zones with 20-33% headroom.
- Connect all PSU GND terminals to a common ground bus.
- Do NOT connect PSU positive outputs together (different voltages).
- Add bulk capacitance (1000+ uF) at each PSU output near the control board.
- Consider a master power switch or relay upstream of all PSUs for whole-system on/off.

### Common Ground and DC Supply Isolation

Tying DC negative terminals together (common ground) is safe and required when:
- Multiple PSUs power different zones controlled by the same ESP32 (the ESP32 GND must be shared with all MOSFET source pins and data line returns).
- All PSUs are non-isolated DC outputs from the same AC mains circuit.

**Check for isolated outputs:** Some multi-output PSUs have galvanically isolated outputs (the negative terminals are not internally connected). If the outputs are isolated, you must externally tie the negatives together for a common ground. Check the PSU datasheet or measure continuity between output negatives with the PSU unpowered. If continuity reads open, the outputs are isolated and need external bonding.

**Do not tie negatives together** if the PSUs are on different AC circuits with different earth ground references (e.g., different buildings or different phases without a common ground). In practice, residential LED projects on the same circuit are fine.

## ESP32 Power from LED Supply

When the ESP32 is co-located with a 12V or 24V LED power supply, a buck converter steps the LED voltage down to power the ESP32.

### Dev Board vs Bare Module

| Target | Buck Output | Feed To | Notes |
|--------|------------|---------|-------|
| **Dev board** (DevKitC, NodeMCU, etc.) | 5V | 5V/VIN pin | The on-board AMS1117-3.3 regulates down to 3.3V |
| **Bare ESP32 module** (WROOM, WROVER) | 3.3V | 3V3 pin directly | No on-board 5V regulator; feeding 5V to a bare module's 3V3 pin will damage it |

If using a bare module, either set the buck converter output to 3.3V or add a dedicated 3.3V LDO (e.g., AMS1117-3.3 or AP2112K-3.3) between a 5V buck and the module's 3V3 pin. Most projects use dev boards.

### Buck Converter Requirements

| Parameter | Requirement | Notes |
|-----------|------------|-------|
| **Input voltage** | Must accept your PSU voltage (12V or 24V) | Check input range on the module |
| **Output voltage** | 5V (dev board) or 3.3V (bare module) | See table above |
| **Output current** | >= 1A (2-3A recommended for 5V) | ESP32 peak draw ~500 mA; headroom for peripherals |
| **Efficiency** | > 85% | Cheap modules may be lower; verify with a meter |

Common modules: Mini-360 (MP2307), LM2596-based boards, or adjustable XL4015 boards (verify output voltage is correct before connecting).

### Wiring (Dev Board Example)

```
24V PSU (+) ──── Buck Converter IN+ ──── Buck Converter OUT+ (5V) ──── ESP32 5V/VIN
24V PSU (-) ──── Buck Converter IN- ──── Buck Converter OUT- ──────── ESP32 GND
                      │
                      └──── Also connects to MOSFET source/GND bus
```

**Common ground:** The buck converter's ground output, the ESP32 GND, and the PSU negative terminal must all be connected together. This is the same ground reference used by the MOSFET source pins.

### Brownout Considerations

When the ESP32 shares a power supply with LEDs (even through a buck converter), large inrush current from the LEDs can sag the PSU voltage. If the buck converter's input drops below its minimum operating voltage, the ESP32 loses power and resets.

Mitigations:
- Size the PSU with 20-33% headroom beyond the combined LED + ESP32 load.
- Add bulk capacitance (1000+ uF electrolytic) on the buck converter input.
- Use software soft-start to ramp LED brightness gradually (see [Brownout Detection](#brownout-detection)).
- For critical applications, power the ESP32 from a separate small PSU (e.g., a USB phone charger) to fully isolate it from LED current transients.
