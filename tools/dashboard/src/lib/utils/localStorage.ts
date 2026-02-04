export function getLocalStorageItem(key: string): string | null {
	if (typeof window === 'undefined') return null;
	try {
		return localStorage.getItem(key);
	} catch {
		return null;
	}
}

export function setLocalStorageItem(key: string, value: string | null): void {
	if (typeof window === 'undefined') return;
	try {
		if (value === null) {
			localStorage.removeItem(key);
		} else {
			localStorage.setItem(key, value);
		}
	} catch {
		// ignore storage failures
	}
}

export function getLocalStorageNumber(key: string): number | null {
	const stored = getLocalStorageItem(key);
	if (!stored) return null;
	const parsed = Number.parseInt(stored, 10);
	return Number.isFinite(parsed) ? parsed : null;
}

export function loadStoredString(key: string): string | null {
	return getLocalStorageItem(key);
}

export function persistStoredString(key: string, value: string | null): void {
	setLocalStorageItem(key, value);
}

export function loadStoredNumber(key: string): number | null {
	return getLocalStorageNumber(key);
}

export function persistStoredNumber(key: string, value: number): void {
	setLocalStorageItem(key, String(value));
}
