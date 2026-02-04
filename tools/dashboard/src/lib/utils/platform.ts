export function getPlatformLabel(platform: string): string {
	const value = platform.toLowerCase();
	if (value.includes('espressif32') || value === 'esp32') return 'ESP32';
	if (value.includes('espressif8266') || value === 'esp8266') return 'ESP8266';
	if (value.includes('atmelavr') || value === 'avr') return 'AVR';
	if (value === 'native') return 'Native';
	return platform;
}

export function getPlatformKey(platform: string): string {
	const upper = platform.toUpperCase();
	return upper === 'AVR' ? '__AVR__' : upper.replace('ESPRESSIF', 'ESP');
}
