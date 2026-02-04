use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PinValidation {
    pub valid: bool,
    pub severity: Severity,
    pub message: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "lowercase")]
pub enum Severity {
    Ok,
    Info,
    Warning,
    Error,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "lowercase")]
pub enum PinPurpose {
    Output,
    Input,
    Adc,
    I2c,
    Spi,
    I2s,
    Pwm,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "lowercase")]
pub enum Platform {
    Esp32,
    Esp8266,
    Avr,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "lowercase")]
pub enum Module {
    Standard,
    Wrover,
    Pico,
}

impl PinValidation {
    fn ok() -> Self {
        Self {
            valid: true,
            severity: Severity::Ok,
            message: String::new(),
        }
    }

    fn error(message: &str) -> Self {
        Self {
            valid: false,
            severity: Severity::Error,
            message: message.to_string(),
        }
    }

    fn warning(message: &str) -> Self {
        Self {
            valid: true,
            severity: Severity::Warning,
            message: message.to_string(),
        }
    }

    fn info(message: &str) -> Self {
        Self {
            valid: true,
            severity: Severity::Info,
            message: message.to_string(),
        }
    }
}

impl PinPurpose {
    pub fn is_output(&self) -> bool {
        matches!(self, PinPurpose::Output | PinPurpose::Pwm | PinPurpose::I2s)
    }
}

/// Validates a GPIO pin for a given purpose on a specific platform.
///
/// Based on docs/pin-reference.md:
/// - GPIO 6-11: SPI flash - NEVER use
/// - GPIO 34-39: Input-only (no pull-ups)
/// - ADC2 pins: Unavailable when WiFi active
/// - GPIO 12: HIGH at boot prevents normal boot
/// - GPIO 16/17: Unavailable on WROVER/PICO (PSRAM)
pub fn validate_pin(
    pin: u8,
    purpose: PinPurpose,
    platform: Platform,
    module: Option<Module>,
) -> PinValidation {
    match platform {
        Platform::Esp32 => validate_esp32_pin(pin, purpose, module),
        Platform::Esp8266 => validate_esp8266_pin(pin, purpose),
        Platform::Avr => validate_avr_pin(pin, purpose),
    }
}

fn validate_esp32_pin(pin: u8, purpose: PinPurpose, module: Option<Module>) -> PinValidation {
    // === ERRORS (block these) ===

    // GPIO 6-11: SPI flash - NEVER use
    if (6..=11).contains(&pin) {
        return PinValidation::error(
            "GPIO 6-11 are connected to SPI flash. Using them will crash the ESP32.",
        );
    }

    // GPIO 12: HIGH at boot prevents normal boot
    if pin == 12 && purpose.is_output() {
        return PinValidation::error(
            "GPIO 12 is a dangerous strapping pin. HIGH at boot selects 1.8V flash voltage, \
             preventing boot. Use only with 10k pull-down or avoid entirely.",
        );
    }

    // Input-only pins for output purposes
    if (34..=39).contains(&pin) && purpose.is_output() {
        return PinValidation::error("GPIO 34-39 are input-only (no output driver).");
    }

    // WROVER/PICO: GPIO 16, 17 unavailable
    if matches!(module, Some(Module::Wrover) | Some(Module::Pico)) && (pin == 16 || pin == 17) {
        return PinValidation::error("GPIO 16/17 are used for PSRAM on WROVER/PICO modules.");
    }

    // === WARNINGS ===

    // ADC2 pins with WiFi conflict
    let adc2_pins = [0, 2, 4, 12, 13, 14, 15, 25, 26, 27];
    if adc2_pins.contains(&pin) && purpose == PinPurpose::Adc {
        return PinValidation::warning(
            "GPIO is on ADC2. ADC2 is unavailable when WiFi is active. \
             Use ADC1 pins (32-39) for reliable analog input.",
        );
    }

    // GPIO 0: Boot mode (must be HIGH for normal boot)
    if pin == 0 {
        return PinValidation::warning(
            "GPIO 0 is a boot strapping pin. Must be HIGH at boot for normal operation. \
             Safe for output after boot, but external loads pulling LOW will prevent boot.",
        );
    }

    // GPIO 2: Built-in LED on many boards
    if pin == 2 {
        return PinValidation::warning(
            "GPIO 2 is connected to the built-in LED on many boards and is a boot strapping pin. \
             Will flicker during boot.",
        );
    }

    // GPIO 5: Strapping pin (low risk)
    if pin == 5 {
        return PinValidation::info(
            "GPIO 5 is a strapping pin affecting SDIO timing. Low risk for LED projects.",
        );
    }

    // GPIO 15: Boot log suppression
    if pin == 15 {
        return PinValidation::warning(
            "GPIO 15 is a strapping pin. LOW at boot suppresses UART boot messages.",
        );
    }

    // Valid pins check
    let safe_pins = [4, 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33];
    if safe_pins.contains(&pin) {
        return PinValidation::ok();
    }

    // Input-only pins are fine for input/ADC
    if (34..=39).contains(&pin) && !purpose.is_output() {
        return PinValidation::ok();
    }

    // Other pins might work but weren't explicitly validated
    PinValidation::info(&format!(
        "GPIO {} is not in the safe GPIO list. Verify it's appropriate for your board.",
        pin
    ))
}

fn validate_esp8266_pin(pin: u8, purpose: PinPurpose) -> PinValidation {
    // ESP8266 has fewer pins and different constraints

    // GPIO 6-11: Flash pins - never use
    if (6..=11).contains(&pin) {
        return PinValidation::error(
            "GPIO 6-11 are connected to SPI flash on ESP8266. Never use.",
        );
    }

    // GPIO 0: Boot mode
    if pin == 0 {
        return PinValidation::warning(
            "GPIO 0 is a boot strapping pin. Must be HIGH at boot. \
             Can be used after boot but may interfere with programming.",
        );
    }

    // GPIO 2: Must be HIGH at boot
    if pin == 2 {
        return PinValidation::warning(
            "GPIO 2 must be HIGH at boot. Also connected to built-in LED on many boards.",
        );
    }

    // GPIO 15: Must be LOW at boot
    if pin == 15 {
        return PinValidation::warning(
            "GPIO 15 must be LOW at boot (via pull-down). Safe to use after boot.",
        );
    }

    // GPIO 16: Special - no PWM, no I2C, limited features
    if pin == 16 && matches!(purpose, PinPurpose::Pwm | PinPurpose::I2c) {
        return PinValidation::error(
            "GPIO 16 does not support PWM or I2C on ESP8266.",
        );
    }

    // Safe pins for general use
    let safe_pins = [4, 5, 12, 13, 14];
    if safe_pins.contains(&pin) {
        return PinValidation::ok();
    }

    PinValidation::info(&format!(
        "GPIO {} may have constraints on ESP8266. Verify it's appropriate for your use.",
        pin
    ))
}

fn validate_avr_pin(pin: u8, _purpose: PinPurpose) -> PinValidation {
    // AVR (Arduino Uno) has pins 0-19 (D0-D13, A0-A5)
    if pin > 19 {
        return PinValidation::error(&format!(
            "Pin {} is out of range for Arduino Uno (0-19).",
            pin
        ));
    }

    // Pins 0, 1 are UART (Serial)
    if pin == 0 || pin == 1 {
        return PinValidation::warning(
            "Pins 0/1 are used for Serial communication. Using them will interfere with USB programming.",
        );
    }

    // Pin 13 has built-in LED
    if pin == 13 {
        return PinValidation::info(
            "Pin 13 has a built-in LED which may cause slight current draw.",
        );
    }

    PinValidation::ok()
}

/// Returns a list of safe GPIO pins for a platform.
pub fn get_safe_pins(platform: Platform, module: Option<Module>) -> Vec<u8> {
    match platform {
        Platform::Esp32 => {
            let mut pins = vec![4, 13, 14, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33];
            // Add 16, 17 only if not WROVER/PICO
            if !matches!(module, Some(Module::Wrover) | Some(Module::Pico)) {
                pins.push(16);
                pins.push(17);
            }
            pins.sort();
            pins
        }
        Platform::Esp8266 => vec![4, 5, 12, 13, 14],
        Platform::Avr => (2..=13).chain(14..=19).collect(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_esp32_flash_pins_blocked() {
        for pin in 6..=11 {
            let result = validate_pin(pin, PinPurpose::Output, Platform::Esp32, None);
            assert!(!result.valid);
            assert_eq!(result.severity, Severity::Error);
            assert!(result.message.contains("SPI flash"));
        }
    }

    #[test]
    fn test_esp32_gpio12_output_blocked() {
        let result = validate_pin(12, PinPurpose::Output, Platform::Esp32, None);
        assert!(!result.valid);
        assert_eq!(result.severity, Severity::Error);
    }

    #[test]
    fn test_esp32_input_only_pins_block_output() {
        for pin in 34..=39 {
            let result = validate_pin(pin, PinPurpose::Output, Platform::Esp32, None);
            assert!(!result.valid);
            assert!(result.message.contains("input-only"));
        }
    }

    #[test]
    fn test_esp32_input_only_pins_allow_input() {
        for pin in 34..=39 {
            let result = validate_pin(pin, PinPurpose::Input, Platform::Esp32, None);
            assert!(result.valid);
        }
    }

    #[test]
    fn test_esp32_adc2_warning() {
        let adc2_pins = [0, 2, 4, 12, 13, 14, 15, 25, 26, 27];
        for pin in adc2_pins {
            // Skip GPIO 12 which has a different error
            if pin == 12 {
                continue;
            }
            let result = validate_pin(pin, PinPurpose::Adc, Platform::Esp32, None);
            assert!(result.valid);
            assert_eq!(result.severity, Severity::Warning);
            assert!(result.message.contains("ADC2"));
        }
    }

    #[test]
    fn test_esp32_wrover_psram_pins() {
        let result = validate_pin(16, PinPurpose::Output, Platform::Esp32, Some(Module::Wrover));
        assert!(!result.valid);
        assert!(result.message.contains("PSRAM"));

        let result = validate_pin(17, PinPurpose::Output, Platform::Esp32, Some(Module::Pico));
        assert!(!result.valid);
    }

    #[test]
    fn test_esp32_safe_pins_ok() {
        let safe = vec![4, 13, 14, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33];
        for pin in safe {
            let result = validate_pin(pin, PinPurpose::Output, Platform::Esp32, None);
            assert!(result.valid, "Pin {} should be valid", pin);
        }
    }

    #[test]
    fn test_esp8266_flash_pins_blocked() {
        for pin in 6..=11 {
            let result = validate_pin(pin, PinPurpose::Output, Platform::Esp8266, None);
            assert!(!result.valid);
        }
    }

    #[test]
    fn test_avr_serial_pins_warning() {
        let result = validate_pin(0, PinPurpose::Output, Platform::Avr, None);
        assert!(result.valid);
        assert_eq!(result.severity, Severity::Warning);
    }
}
