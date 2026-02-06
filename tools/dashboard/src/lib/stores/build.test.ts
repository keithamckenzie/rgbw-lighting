import { describe, it, expect, beforeEach } from 'vitest';
import { get } from 'svelte/store';
import {
	buildState,
	handleBuildEvent,
	addBuildLine,
	clearBuildLog,
	clearBuildStatus,
	resetBuildState,
	runBuild,
	runUpload,
	runTests
} from './build';
import { invoke } from '../../test/mocks/tauri';

describe('build store', () => {
	beforeEach(() => {
		// Reset to initial state
		buildState.set({
			isBuilding: false,
			appName: null,
			environment: null,
			success: null,
			durationMs: null,
			lines: []
		});
	});

	describe('handleBuildEvent', () => {
		it('handles started event', () => {
			handleBuildEvent({
				type: 'started',
				app_name: 'led-panel',
				environment: 'esp32'
			});

			const state = get(buildState);
			expect(state.isBuilding).toBe(true);
			expect(state.appName).toBe('led-panel');
			expect(state.environment).toBe('esp32');
			expect(state.success).toBeNull();
			expect(state.durationMs).toBeNull();
			expect(state.lines).toEqual([]);
		});

		it('handles output event', () => {
			handleBuildEvent({ type: 'output', line: 'Compiling main.cpp' });

			const state = get(buildState);
			expect(state.lines).toEqual(['Compiling main.cpp']);
		});

		it('handles error event', () => {
			handleBuildEvent({ type: 'error', message: 'Syntax error' });

			const state = get(buildState);
			expect(state.lines).toEqual(['ERROR: Syntax error']);
		});

		it('handles complete event with success', () => {
			handleBuildEvent({ type: 'started' });
			handleBuildEvent({ type: 'complete', success: true, duration_ms: 5000 });

			const state = get(buildState);
			expect(state.isBuilding).toBe(false);
			expect(state.success).toBe(true);
			expect(state.durationMs).toBe(5000);
		});

		it('handles complete event with failure', () => {
			handleBuildEvent({ type: 'started' });
			handleBuildEvent({ type: 'complete', success: false, duration_ms: 1000 });

			const state = get(buildState);
			expect(state.isBuilding).toBe(false);
			expect(state.success).toBe(false);
			expect(state.durationMs).toBe(1000);
		});

		it('ignores output event with no line', () => {
			handleBuildEvent({ type: 'output' });

			const state = get(buildState);
			expect(state.lines).toEqual([]);
		});
	});

	describe('addBuildLine', () => {
		it('appends a line', () => {
			addBuildLine('Line 1');
			addBuildLine('Line 2');

			const state = get(buildState);
			expect(state.lines).toEqual(['Line 1', 'Line 2']);
		});

		it('respects MAX_LOG_LINES limit', () => {
			// Add more than 5000 lines
			for (let i = 0; i < 5010; i++) {
				addBuildLine(`Line ${i}`);
			}

			const state = get(buildState);
			expect(state.lines.length).toBe(5000);
			// Should keep the most recent lines
			expect(state.lines[0]).toBe('Line 10');
			expect(state.lines[4999]).toBe('Line 5009');
		});
	});

	describe('clearBuildLog', () => {
		it('clears lines only', () => {
			addBuildLine('Line 1');
			buildState.update((s) => ({ ...s, success: true }));

			clearBuildLog();

			const state = get(buildState);
			expect(state.lines).toEqual([]);
			expect(state.success).toBe(true);
		});
	});

	describe('clearBuildStatus', () => {
		it('clears success and durationMs', () => {
			buildState.update((s) => ({
				...s,
				success: true,
				durationMs: 5000,
				lines: ['output']
			}));

			clearBuildStatus();

			const state = get(buildState);
			expect(state.success).toBeNull();
			expect(state.durationMs).toBeNull();
			expect(state.lines).toEqual(['output']);
		});
	});

	describe('resetBuildState', () => {
		it('resets all transient state', () => {
			buildState.update((s) => ({
				...s,
				isBuilding: true,
				success: true,
				durationMs: 5000,
				lines: ['output']
			}));

			resetBuildState();

			const state = get(buildState);
			expect(state.isBuilding).toBe(false);
			expect(state.success).toBeNull();
			expect(state.durationMs).toBeNull();
			expect(state.lines).toEqual([]);
		});
	});

	describe('runBuild', () => {
		it('invokes run_build and returns result', async () => {
			invoke.mockResolvedValueOnce(true);

			const result = await runBuild('led-panel', 'esp32', ['-DFOO=1']);

			expect(invoke).toHaveBeenCalledWith('run_build', {
				appName: 'led-panel',
				environment: 'esp32',
				buildFlags: ['-DFOO=1']
			});
			expect(result).toBe(true);
		});

		it('handles errors gracefully', async () => {
			invoke.mockRejectedValueOnce(new Error('Build timeout'));

			const result = await runBuild('led-panel', 'esp32', []);

			expect(result).toBe(false);
			const state = get(buildState);
			expect(state.isBuilding).toBe(false);
			expect(state.success).toBe(false);
			expect(state.lines.some((l) => l.includes('Build failed'))).toBe(true);
		});
	});

	describe('runUpload', () => {
		it('invokes run_upload with port', async () => {
			invoke.mockResolvedValueOnce(true);

			const result = await runUpload('led-panel', 'esp32', [], '/dev/ttyUSB0');

			expect(invoke).toHaveBeenCalledWith('run_upload', {
				appName: 'led-panel',
				environment: 'esp32',
				buildFlags: [],
				uploadPort: '/dev/ttyUSB0'
			});
			expect(result).toBe(true);
		});

		it('passes null for missing port', async () => {
			invoke.mockResolvedValueOnce(true);

			await runUpload('led-panel', 'esp32', []);

			expect(invoke).toHaveBeenCalledWith('run_upload', {
				appName: 'led-panel',
				environment: 'esp32',
				buildFlags: [],
				uploadPort: null
			});
		});
	});

	describe('runTests', () => {
		it('invokes run_tests', async () => {
			invoke.mockResolvedValueOnce(true);

			const result = await runTests('led-panel', 'native');

			expect(invoke).toHaveBeenCalledWith('run_tests', {
				appName: 'led-panel',
				environment: 'native'
			});
			expect(result).toBe(true);
		});

		it('handles errors gracefully', async () => {
			invoke.mockRejectedValueOnce(new Error('Test timeout'));

			const result = await runTests('led-panel', 'native');

			expect(result).toBe(false);
			const state = get(buildState);
			expect(state.lines.some((l) => l.includes('Tests failed'))).toBe(true);
		});
	});
});
