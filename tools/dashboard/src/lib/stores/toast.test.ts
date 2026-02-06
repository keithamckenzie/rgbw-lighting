import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';
import { toasts, pushToast, clearToastTimeouts } from './toast';

describe('toast store', () => {
	beforeEach(() => {
		vi.useFakeTimers();
		// Clear any lingering toasts
		toasts.set([]);
		clearToastTimeouts();
	});

	afterEach(() => {
		clearToastTimeouts();
		vi.useRealTimers();
	});

	it('pushToast adds a toast to the store', () => {
		pushToast('Test message', 'info');

		const current = get(toasts);
		expect(current).toHaveLength(1);
		expect(current[0].message).toBe('Test message');
		expect(current[0].kind).toBe('info');
		expect(current[0].id).toBeTruthy();
	});

	it('pushToast defaults to info kind', () => {
		pushToast('Default kind');

		const current = get(toasts);
		expect(current[0].kind).toBe('info');
	});

	it('auto-removes toast after duration', () => {
		pushToast('Temporary', 'success', 3000);

		expect(get(toasts)).toHaveLength(1);

		vi.advanceTimersByTime(3000);

		expect(get(toasts)).toHaveLength(0);
	});

	it('keeps toast indefinitely when durationMs is 0', () => {
		pushToast('Persistent', 'error', 0);

		expect(get(toasts)).toHaveLength(1);

		vi.advanceTimersByTime(60000);

		expect(get(toasts)).toHaveLength(1);
	});

	it('handles multiple toasts with different durations', () => {
		pushToast('Short', 'info', 1000);
		pushToast('Long', 'info', 5000);

		expect(get(toasts)).toHaveLength(2);

		vi.advanceTimersByTime(1000);
		expect(get(toasts)).toHaveLength(1);
		expect(get(toasts)[0].message).toBe('Long');

		vi.advanceTimersByTime(4000);
		expect(get(toasts)).toHaveLength(0);
	});

	it('clearToastTimeouts prevents auto-removal', () => {
		pushToast('Surviving', 'warning', 2000);

		expect(get(toasts)).toHaveLength(1);

		clearToastTimeouts();

		vi.advanceTimersByTime(5000);

		// Toast remains since its timeout was cleared
		expect(get(toasts)).toHaveLength(1);
	});

	it('generates unique IDs for each toast', () => {
		pushToast('First', 'info');
		pushToast('Second', 'info');

		const current = get(toasts);
		expect(current[0].id).not.toBe(current[1].id);
	});
});
