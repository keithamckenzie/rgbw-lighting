import { describe, it, expect, vi } from 'vitest';
import { invoke } from '../../test/mocks/tauri';

// Must import after mocks are registered
import { invokeWithTimeout } from './invoke';

describe('invokeWithTimeout', () => {
	it('returns result on successful invoke', async () => {
		invoke.mockResolvedValueOnce('ok');

		const result = await invokeWithTimeout<string>('test_cmd');
		expect(result).toBe('ok');
		expect(invoke).toHaveBeenCalledWith('test_cmd', undefined);
	});

	it('passes args to invoke', async () => {
		invoke.mockResolvedValueOnce(42);

		const result = await invokeWithTimeout<number>('calc', { x: 1, y: 2 });
		expect(result).toBe(42);
		expect(invoke).toHaveBeenCalledWith('calc', { x: 1, y: 2 });
	});

	it('rejects on timeout', async () => {
		// Invoke never resolves — use a very short timeout for the test
		invoke.mockReturnValueOnce(new Promise(() => {}));

		await expect(invokeWithTimeout('slow_cmd', undefined, 50)).rejects.toThrow(
			"Command 'slow_cmd' timed out after 50ms"
		);
	}, 10000);

	it('propagates invoke errors', async () => {
		invoke.mockRejectedValueOnce(new Error('Backend error'));

		await expect(invokeWithTimeout('broken_cmd')).rejects.toThrow('Backend error');
	});

	it('clears timeout on success', async () => {
		const clearTimeoutSpy = vi.spyOn(global, 'clearTimeout');
		invoke.mockResolvedValueOnce('ok');

		await invokeWithTimeout('cmd');

		expect(clearTimeoutSpy).toHaveBeenCalled();
		clearTimeoutSpy.mockRestore();
	});
});
