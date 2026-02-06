import { describe, it, expect, beforeEach } from 'vitest';
import {
	getLocalStorageItem,
	setLocalStorageItem,
	loadStoredString,
	persistStoredString,
	loadStoredNumber,
	persistStoredNumber
} from './localStorage';

describe('localStorage utilities', () => {
	beforeEach(() => {
		localStorage.clear();
	});

	describe('getLocalStorageItem', () => {
		it('should return null for non-existent key', () => {
			expect(getLocalStorageItem('nonexistent')).toBeNull();
		});

		it('should return stored value', () => {
			localStorage.setItem('test-key', 'test-value');
			expect(getLocalStorageItem('test-key')).toBe('test-value');
		});
	});

	describe('setLocalStorageItem', () => {
		it('should store a value', () => {
			setLocalStorageItem('test-key', 'test-value');
			expect(localStorage.getItem('test-key')).toBe('test-value');
		});

		it('should remove item when value is null', () => {
			localStorage.setItem('test-key', 'existing');
			setLocalStorageItem('test-key', null);
			expect(localStorage.getItem('test-key')).toBeNull();
		});
	});

	describe('loadStoredString', () => {
		it('should return null for non-existent key', () => {
			expect(loadStoredString('nonexistent')).toBeNull();
		});

		it('should return stored string', () => {
			localStorage.setItem('test-key', 'hello');
			expect(loadStoredString('test-key')).toBe('hello');
		});
	});

	describe('persistStoredString', () => {
		it('should persist a string', () => {
			persistStoredString('test-key', 'world');
			expect(localStorage.getItem('test-key')).toBe('world');
		});
	});

	describe('loadStoredNumber', () => {
		it('should return null for non-existent key', () => {
			expect(loadStoredNumber('nonexistent')).toBeNull();
		});

		it('should return null for non-numeric value', () => {
			localStorage.setItem('test-key', 'not-a-number');
			expect(loadStoredNumber('nonexistent')).toBeNull();
		});

		it('should return parsed integer', () => {
			localStorage.setItem('test-key', '42');
			expect(loadStoredNumber('test-key')).toBe(42);
		});

		it('should truncate decimal numbers (uses parseInt)', () => {
			// Implementation uses parseInt, so decimals are truncated
			localStorage.setItem('test-key', '3.14');
			expect(loadStoredNumber('test-key')).toBe(3);
		});

		it('should handle negative numbers', () => {
			localStorage.setItem('test-key', '-100');
			expect(loadStoredNumber('test-key')).toBe(-100);
		});
	});

	describe('persistStoredNumber', () => {
		it('should persist a number as string', () => {
			persistStoredNumber('test-key', 123);
			expect(localStorage.getItem('test-key')).toBe('123');
		});
	});
});
