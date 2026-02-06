import { describe, it, expect } from 'vitest';
import { getPlatformLabel, getPlatformKey } from './platform';

describe('getPlatformLabel', () => {
	it('returns ESP32 for espressif32', () => {
		expect(getPlatformLabel('espressif32')).toBe('ESP32');
	});

	it('returns ESP32 for esp32', () => {
		expect(getPlatformLabel('esp32')).toBe('ESP32');
	});

	it('returns ESP8266 for espressif8266', () => {
		expect(getPlatformLabel('espressif8266')).toBe('ESP8266');
	});

	it('returns ESP8266 for esp8266', () => {
		expect(getPlatformLabel('esp8266')).toBe('ESP8266');
	});

	it('returns AVR for atmelavr', () => {
		expect(getPlatformLabel('atmelavr')).toBe('AVR');
	});

	it('returns AVR for avr', () => {
		expect(getPlatformLabel('avr')).toBe('AVR');
	});

	it('returns Native for native', () => {
		expect(getPlatformLabel('native')).toBe('Native');
	});

	it('returns raw value for unknown platforms', () => {
		expect(getPlatformLabel('teensy')).toBe('teensy');
	});

	it('is case-insensitive', () => {
		expect(getPlatformLabel('ESPRESSIF32')).toBe('ESP32');
		expect(getPlatformLabel('Espressif8266')).toBe('ESP8266');
		expect(getPlatformLabel('AtmelAVR')).toBe('AVR');
	});
});

describe('getPlatformKey', () => {
	it('returns __AVR__ for AVR', () => {
		expect(getPlatformKey('avr')).toBe('__AVR__');
	});

	it('returns ESP32 for espressif32', () => {
		expect(getPlatformKey('espressif32')).toBe('ESP32');
	});

	it('returns ESP8266 for espressif8266', () => {
		expect(getPlatformKey('espressif8266')).toBe('ESP8266');
	});

	it('uppercases the platform name', () => {
		expect(getPlatformKey('native')).toBe('NATIVE');
	});
});
