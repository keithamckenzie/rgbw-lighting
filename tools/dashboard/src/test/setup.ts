import '@testing-library/jest-dom/vitest';
import { beforeEach, vi } from 'vitest';

// Import Tauri mocks (registers vi.mock calls for @tauri-apps/api/*)
import './mocks/tauri';

// Mock localStorage
const localStorageMock = {
	store: {} as Record<string, string>,
	getItem: (key: string) => localStorageMock.store[key] ?? null,
	setItem: (key: string, value: string) => {
		localStorageMock.store[key] = value;
	},
	removeItem: (key: string) => {
		delete localStorageMock.store[key];
	},
	clear: () => {
		localStorageMock.store = {};
	}
};

Object.defineProperty(globalThis, 'localStorage', {
	value: localStorageMock
});

// Clear state between tests
beforeEach(() => {
	localStorageMock.clear();
	vi.clearAllMocks();
});
