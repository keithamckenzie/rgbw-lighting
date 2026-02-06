// Human-readable metadata for PlatformIO environments
// Provides display names, descriptions, and feature flags

export interface EnvMeta {
  displayName: string;
  description: string;
  features?: string[];
  isTest?: boolean;
}

// Platform-based metadata (matches platform string from platformio.ini)
export const PLATFORM_METADATA: Record<string, EnvMeta> = {
  espressif32: {
    displayName: 'ESP32',
    description: 'Full-featured 32-bit microcontroller with WiFi, Bluetooth, and audio support',
    features: ['wifi', 'bluetooth', 'audio', 'ota']
  },
  esp32: {
    displayName: 'ESP32',
    description: 'Full-featured 32-bit microcontroller with WiFi, Bluetooth, and audio support',
    features: ['wifi', 'bluetooth', 'audio', 'ota']
  },
  espressif8266: {
    displayName: 'ESP8266',
    description: 'Budget WiFi microcontroller, no audio or Bluetooth',
    features: ['wifi']
  },
  esp8266: {
    displayName: 'ESP8266',
    description: 'Budget WiFi microcontroller, no audio or Bluetooth',
    features: ['wifi']
  },
  atmelavr: {
    displayName: 'Arduino (AVR)',
    description: 'Classic 8-bit Arduino boards like Uno and Nano',
    features: []
  },
  avr: {
    displayName: 'Arduino (AVR)',
    description: 'Classic 8-bit Arduino boards like Uno and Nano',
    features: []
  },
  native: {
    displayName: 'Native Tests',
    description: 'Run unit tests on your computer without hardware',
    features: [],
    isTest: true
  }
};

// Environment name pattern metadata for test environments
export const ENV_NAME_PATTERNS: Record<string, EnvMeta> = {
  'native-serpentine': {
    displayName: 'Native (Serpentine)',
    description: 'Test serpentine wiring pattern on your computer',
    isTest: true
  },
  'native-progressive': {
    displayName: 'Native (Progressive)',
    description: 'Test progressive wiring pattern on your computer',
    isTest: true
  }
};

/**
 * Get metadata for an environment by name and platform
 */
export function getEnvMetadata(envName: string, platform: string): EnvMeta {
  // Check for exact environment name match first
  for (const [pattern, meta] of Object.entries(ENV_NAME_PATTERNS)) {
    if (envName.startsWith(pattern)) {
      return meta;
    }
  }

  // Fall back to platform metadata
  const platformMeta = PLATFORM_METADATA[platform.toLowerCase()];
  if (platformMeta) {
    return platformMeta;
  }

  // Default fallback
  return {
    displayName: envName,
    description: `Build target: ${envName}`,
    features: []
  };
}

/**
 * Feature icon type (matches FeatureIcon component props)
 */
export type FeatureIconType = 'wifi' | 'bluetooth' | 'mic' | 'cloud';

/**
 * Feature display labels
 */
export const FEATURE_LABELS: Record<string, { label: string; icon: FeatureIconType }> = {
  wifi: { label: 'WiFi', icon: 'wifi' },
  bluetooth: { label: 'Bluetooth', icon: 'bluetooth' },
  audio: { label: 'Audio Input', icon: 'mic' },
  ota: { label: 'OTA Updates', icon: 'cloud' }
};

/**
 * Check if an environment has a specific feature
 */
export function hasFeature(features: string[] | undefined, feature: string): boolean {
  return features?.includes(feature) ?? false;
}
