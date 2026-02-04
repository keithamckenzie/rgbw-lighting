import type { PinValidation, PinPurpose } from '$lib/types/config';
import { invokeWithTimeout } from '$lib/utils/invoke';
import { pushToast } from '$lib/stores/toast';

const PIN_TIMEOUT_MS = 5000;

/**
 * Validates a GPIO pin for a given purpose on a specific platform.
 * This is the single source of truth - calls the Rust backend.
 */
export async function validatePin(
	pin: number | string,
	purpose: PinPurpose,
	platform: string,
	module?: string
): Promise<PinValidation> {
	// Handle ESP8266 A0 special case
	if (typeof pin === 'string') {
		const trimmed = pin.trim();
		if (trimmed.toUpperCase() === 'A0' && platform.toLowerCase().includes('esp8266')) {
			// A0 is valid for ADC on ESP8266
			if (purpose === 'adc') {
				return {
					valid: true,
					severity: 'ok',
					message: ''
				};
			}
			return {
				valid: false,
				severity: 'error',
				message: 'A0 on ESP8266 can only be used for ADC input'
			};
		}
		if (!/^\d+$/.test(trimmed)) {
			return {
				valid: false,
				severity: 'error',
				message: `Invalid pin value: ${pin}`
			};
		}
		// Try to parse as number
		const parsed = parseInt(trimmed, 10);
		if (isNaN(parsed)) {
			return {
				valid: false,
				severity: 'error',
				message: `Invalid pin value: ${pin}`
			};
		}
		pin = parsed;
	}

	try {
		const result = await invokeWithTimeout<PinValidation>(
			'validate_pin',
			{
				pin,
				purpose,
				platform,
				module: module ?? null
			},
			PIN_TIMEOUT_MS
		);
		return result;
	} catch (error) {
		return {
			valid: false,
			severity: 'error',
			message: `Validation failed: ${error}`
		};
	}
}

/**
 * Gets the list of safe GPIO pins for a platform.
 */
export async function getSafePins(platform: string, module?: string): Promise<number[]> {
	try {
		return await invokeWithTimeout<number[]>(
			'get_safe_pins',
			{
				platform,
				module: module ?? null
			},
			PIN_TIMEOUT_MS
		);
	} catch (error) {
		console.error('Failed to get safe pins:', error);
		pushToast('Failed to load suggested pins', 'error');
		return [];
	}
}

/**
 * Returns the severity color class for Tailwind.
 */
export function getSeverityClass(severity: PinValidation['severity']): string {
	switch (severity) {
		case 'error':
			return 'text-red-500 border-red-500';
		case 'warning':
			return 'text-yellow-500 border-yellow-500';
		case 'info':
			return 'text-blue-500 border-blue-500';
		case 'ok':
		default:
			return 'text-green-500 border-green-500';
	}
}

/**
 * Returns the severity icon.
 */
export function getSeverityIcon(severity: PinValidation['severity']): string {
	switch (severity) {
		case 'error':
			return '✕';
		case 'warning':
			return '⚠';
		case 'info':
			return 'ℹ';
		case 'ok':
		default:
			return '✓';
	}
}
