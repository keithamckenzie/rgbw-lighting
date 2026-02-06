import { describe, it, expect } from 'vitest';
import { resolveSelection } from './selection';

describe('resolveSelection', () => {
	const available = ['env1', 'env2', 'env3'];
	const preferred = 'env2';

	it('should return urlValue if valid', () => {
		expect(resolveSelection('env1', null, available, preferred)).toBe('env1');
	});

	it('should return storedValue if urlValue invalid but storedValue valid', () => {
		expect(resolveSelection(null, 'env3', available, preferred)).toBe('env3');
		expect(resolveSelection('invalid', 'env3', available, preferred)).toBe('env3');
	});

	it('should return preferred if both urlValue and storedValue invalid', () => {
		expect(resolveSelection(null, null, available, preferred)).toBe('env2');
		expect(resolveSelection('invalid', 'also-invalid', available, preferred)).toBe('env2');
	});

	it('should return first available if nothing matches and preferred invalid', () => {
		// When preferred is null but available is non-empty, returns first item
		expect(resolveSelection(null, null, available, null)).toBe('env1');
	});

	it('should return defaultValue for empty available list', () => {
		// When available is empty, returns the defaultValue (preferred)
		expect(resolveSelection('env1', 'env2', [], 'env3')).toBe('env3');
	});

	it('should return null for empty available and null preferred', () => {
		expect(resolveSelection(null, null, [], null)).toBeNull();
	});

	it('should return first available when preferred not in available', () => {
		// Preferred 'not-available' isn't in list, falls back to first available
		expect(resolveSelection(null, null, available, 'not-available')).toBe('env1');
	});
});
