// Human-readable metadata for config fields
// This registry provides labels, descriptions, grouping, and validation constraints

import type { FieldMeta } from '$lib/types/config';

export const FIELD_GROUPS = {
  DISPLAY: 'Display Settings',
  HARDWARE: 'Hardware',
  PINS: 'Pin Assignments',
  AUDIO: 'Audio Settings',
  TIMING: 'Timing & Performance',
  INPUT: 'Input Settings',
  ADVANCED: 'Advanced'
} as const;

export const FIELD_METADATA: Record<string, FieldMeta> = {
  // === Display Settings ===
  PANEL_WIDTH: {
    label: 'Panel Width',
    description: 'Number of LEDs in the horizontal direction',
    group: FIELD_GROUPS.DISPLAY,
    min: 1,
    max: 500
  },
  PANEL_HEIGHT: {
    label: 'Panel Height',
    description: 'Number of LEDs in the vertical direction',
    group: FIELD_GROUPS.DISPLAY,
    min: 1,
    max: 500
  },
  NUM_PIXELS: {
    label: 'Total Pixels',
    description: 'Auto-calculated from width × height',
    group: FIELD_GROUPS.DISPLAY,
    advanced: true,
    readOnly: true,
    derivedFrom: ['PANEL_WIDTH', 'PANEL_HEIGHT']
  },
  NUM_LEDS: {
    label: 'Number of LEDs',
    description: 'Total number of LEDs in your strip or panel',
    group: FIELD_GROUPS.DISPLAY,
    min: 1,
    max: 2000
  },
  STRIP_TYPE: {
    label: 'LED Strip Type',
    description: 'The type of addressable LED strip you are using',
    group: FIELD_GROUPS.HARDWARE,
    enumDescriptions: {
      '0': 'SK6812 RGBW: 4-channel with dedicated white LED. 5V, 80mA per LED.',
      '1': 'WS2815B RGB: 3-channel with backup data line. 12V, 30mA per LED.'
    }
  },
  WIRING_PATTERN: {
    label: 'Wiring Pattern',
    description: 'How the LED strip is physically wired on your panel',
    group: FIELD_GROUPS.DISPLAY,
    enumDescriptions: {
      '0': 'Serpentine Horizontal: Rows alternate direction (zigzag pattern)',
      '1': 'Progressive Horizontal: All rows run the same direction',
      '2': 'Serpentine Vertical: Columns alternate direction'
    }
  },

  // === Pin Assignments ===
  PIN_LED_DATA: {
    label: 'LED Data Pin',
    description: 'GPIO pin connected to the LED strip data line',
    group: FIELD_GROUPS.PINS
  },
  PIN_SWITCH: {
    label: 'Switch Pin',
    description: 'GPIO pin for the main control button/switch',
    group: FIELD_GROUPS.PINS
  },
  PIN_BRIGHTNESS: {
    label: 'Brightness Pot Pin',
    description: 'ADC pin for the brightness potentiometer',
    group: FIELD_GROUPS.PINS
  },
  PIN_I2S_SCK: {
    label: 'I2S Clock Pin (SCK/BCLK)',
    description: 'Bit clock pin for I2S audio input',
    group: FIELD_GROUPS.PINS,
    advanced: true
  },
  PIN_I2S_WS: {
    label: 'I2S Word Select Pin (WS/LRCLK)',
    description: 'Word select / left-right clock pin for I2S',
    group: FIELD_GROUPS.PINS,
    advanced: true
  },
  PIN_I2S_SD: {
    label: 'I2S Data Pin (SD/DIN)',
    description: 'Serial data input pin for I2S microphone',
    group: FIELD_GROUPS.PINS,
    advanced: true
  },
  PIN_I2S_MCLK: {
    label: 'I2S Master Clock Pin (MCLK)',
    description: 'Master clock output pin (GPIO 0 is hardware-fixed on ESP32)',
    group: FIELD_GROUPS.PINS,
    advanced: true
  },

  // === Audio Settings ===
  AUDIO_ENABLED: {
    label: 'Audio Reactive',
    description: 'Enable sound-reactive lighting effects',
    group: FIELD_GROUPS.AUDIO
  },
  AUDIO_INPUT_MODE: {
    label: 'Audio Input Mode',
    description: 'Type of audio input hardware',
    group: FIELD_GROUPS.AUDIO,
    enumDescriptions: {
      '0': 'I2S Microphone: Digital mic like INMP441, SPH0645, or ICS-43432',
      '1': 'I2S ADC: External ADC like PCM1808 for line-level input'
    }
  },
  BEAT_THRESHOLD: {
    label: 'Beat Threshold',
    description: 'Energy ratio required to detect a beat (higher = less sensitive)',
    group: FIELD_GROUPS.AUDIO,
    min: 1.0,
    max: 3.0,
    advanced: true
  },
  BEAT_COOLDOWN_MS: {
    label: 'Beat Cooldown (ms)',
    description: 'Minimum time between detected beats',
    group: FIELD_GROUPS.AUDIO,
    min: 50,
    max: 500,
    advanced: true
  },
  BPM_EMA_ALPHA: {
    label: 'BPM Smoothing',
    description: 'Exponential moving average factor for BPM tracking (lower = smoother)',
    group: FIELD_GROUPS.AUDIO,
    min: 0.05,
    max: 0.5,
    advanced: true
  },

  // === Timing & Performance ===
  TARGET_FPS: {
    label: 'Target Frame Rate',
    description: 'Target frames per second for LED updates',
    group: FIELD_GROUPS.TIMING,
    min: 10,
    max: 60,
    advanced: true
  },
  FRAME_MS: {
    label: 'Frame Duration',
    description: 'Auto-calculated from target FPS',
    group: FIELD_GROUPS.TIMING,
    advanced: true,
    readOnly: true,
    derivedFrom: ['TARGET_FPS'],
    unit: 'ms'
  },

  // === Input Settings ===
  LONG_PRESS_MS: {
    label: 'Long Press Duration (ms)',
    description: 'How long a button must be held for a long press',
    group: FIELD_GROUPS.INPUT,
    min: 200,
    max: 2000,
    advanced: true
  },
  ADC_READ_INTERVAL: {
    label: 'ADC Read Interval (ms)',
    description: 'Time between potentiometer readings',
    group: FIELD_GROUPS.INPUT,
    min: 10,
    max: 200,
    advanced: true
  },
  ADC_SAMPLES: {
    label: 'ADC Sample Count',
    description: 'Moving average window size for analog readings',
    group: FIELD_GROUPS.INPUT,
    min: 1,
    max: 32,
    advanced: true
  },

  // === PWM Settings ===
  PWM_FREQUENCY: {
    label: 'PWM Frequency (Hz)',
    description: 'Frequency for PWM output channels',
    group: FIELD_GROUPS.HARDWARE,
    min: 100,
    max: 40000,
    advanced: true
  },
  PWM_RESOLUTION: {
    label: 'PWM Resolution (bits)',
    description: 'Bit depth for PWM duty cycle (8=256 steps, 12=4096 steps)',
    group: FIELD_GROUPS.HARDWARE,
    min: 8,
    max: 16,
    advanced: true
  },

  // === Color Settings ===
  DEFAULT_BRIGHTNESS: {
    label: 'Default Brightness',
    description: 'Initial brightness level (0-255)',
    group: FIELD_GROUPS.DISPLAY,
    min: 0,
    max: 255
  },
  GAMMA_CORRECTION: {
    label: 'Gamma Correction',
    description: 'Enable gamma correction for perceptually uniform brightness',
    group: FIELD_GROUPS.DISPLAY,
    advanced: true
  },
  WHITE_TEMPERATURE: {
    label: 'White Temperature (K)',
    description: 'Color temperature for white channel (2700K warm, 6500K cool)',
    group: FIELD_GROUPS.DISPLAY,
    min: 2000,
    max: 10000,
    advanced: true
  }
};

/**
 * Convert a MACRO_NAME to "Macro Name" for fallback display
 */
export function toHumanLabel(macroName: string): string {
  return macroName
    .replace(/_/g, ' ')
    .toLowerCase()
    .replace(/\b\w/g, (c) => c.toUpperCase())
    .replace(/\bLed\b/gi, 'LED')
    .replace(/\bI2s\b/gi, 'I2S')
    .replace(/\bAdc\b/gi, 'ADC')
    .replace(/\bPwm\b/gi, 'PWM')
    .replace(/\bMs\b/gi, 'ms')
    .replace(/\bFps\b/gi, 'FPS')
    .replace(/\bBpm\b/gi, 'BPM');
}
