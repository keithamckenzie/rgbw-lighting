import { get } from 'svelte/store';
import { invokeWithTimeout } from '$lib/utils/invoke';
import { pushToast } from '$lib/stores/toast';
import type { SerialEvent } from '$lib/types/config';
import { serialBuffers, serialState } from './state';

// Store mutations are funneled through exported action functions to centralize side effects.
// Errors here are thrown so UI components can present context-aware toasts.
const MAX_SERIAL_BUFFER = 10000;
const SERIAL_CMD_TIMEOUT_MS = 10000;

export async function openSerialPort(portPath: string, baudRate: number): Promise<string | null> {
	try {
		const connectionId = await invokeWithTimeout<string>(
			'open_serial',
			{
				portPath,
				baudRate
			},
			SERIAL_CMD_TIMEOUT_MS
		);

		serialState.update((state) => {
			const connections = new Map(state.connections);
			connections.set(connectionId, {
				connectionId,
				portPath,
				baudRate
			});
			return {
				...state,
				connections,
				activeConnectionId: connectionId
			};
		});

		serialBuffers.update((buffers) => {
			const next = new Map(buffers);
			next.set(connectionId, { lines: [], partialLine: '' });
			return next;
		});

		return connectionId;
	} catch (error) {
		console.error('Failed to open port:', error);
		throw error;
	}
}

export async function closeSerialPort(connectionId: string): Promise<void> {
	try {
		await invokeWithTimeout('close_serial', { connectionId }, SERIAL_CMD_TIMEOUT_MS);

		serialState.update((state) => {
			const connections = new Map(state.connections);
			connections.delete(connectionId);

			const nextKey = connections.keys().next();
			const newActive =
				state.activeConnectionId === connectionId
					? nextKey.done
						? null
						: nextKey.value
					: state.activeConnectionId;

			return {
				...state,
				connections,
				activeConnectionId: newActive
			};
		});

		serialBuffers.update((buffers) => {
			const next = new Map(buffers);
			next.delete(connectionId);
			return next;
		});
	} catch (error) {
		console.error('Failed to close port:', error);
		pushToast(`Failed to close serial port: ${String(error)}`, 'error');
		throw error;
	}
}

export async function writeSerial(connectionId: string, data: string): Promise<void> {
	try {
		await invokeWithTimeout('write_serial', { connectionId, data }, SERIAL_CMD_TIMEOUT_MS);
	} catch (error) {
		console.error('Failed to write to serial:', error);
		pushToast(`Failed to write to serial: ${String(error)}`, 'error');
		throw error;
	}
}

export function handleSerialEvent(event: SerialEvent): void {
	switch (event.type) {
		case 'data':
			// Ensure the connection is still known; events can arrive before buffers are initialized.
			if (!get(serialState).connections.has(event.connection_id)) {
				return;
			}
			serialBuffers.update((buffers) => {
				const buffer = buffers.get(event.connection_id) ?? { lines: [], partialLine: '' };

				const text = event.text ?? '';
				const normalized = (buffer.partialLine + text)
					.replace(/\r\n/g, '\n')
					.replace(/\r/g, '\n');
				const parts = normalized.split('\n');
				let nextPartial = '';

				if (!normalized.endsWith('\n')) {
					nextPartial = parts.pop() ?? '';
				} else if (parts.length > 0 && parts[parts.length - 1] === '') {
					parts.pop();
				}

				const nextLines = [...buffer.lines, ...parts];
				const trimmedLines =
					nextLines.length > MAX_SERIAL_BUFFER
						? nextLines.slice(-MAX_SERIAL_BUFFER)
						: nextLines;

				const next = new Map(buffers);
				next.set(event.connection_id, { lines: trimmedLines, partialLine: nextPartial });
				return next;
			});
			break;

		case 'error':
			console.error(`Serial error on ${event.connection_id}: ${event.message}`);
			pushToast(`Serial error: ${event.message ?? 'Unknown error'}`, 'error');
			break;

		case 'closed':
			serialState.update((state) => {
				if (!state.connections.has(event.connection_id)) return state;
				const connections = new Map(state.connections);
				connections.delete(event.connection_id);

				const nextKey = connections.keys().next();
				const newActive =
					state.activeConnectionId === event.connection_id
						? nextKey.done
							? null
							: nextKey.value
						: state.activeConnectionId;

				return {
					...state,
					connections,
					activeConnectionId: newActive
				};
			});

			serialBuffers.update((buffers) => {
				if (!buffers.has(event.connection_id)) return buffers;
				const next = new Map(buffers);
				next.delete(event.connection_id);
				return next;
			});
			break;
	}
}

export function clearSerialBuffer(connectionId: string): void {
	serialBuffers.update((buffers) => {
		const buffer = buffers.get(connectionId);
		if (!buffer) return buffers;
		const next = new Map(buffers);
		next.set(connectionId, { lines: [], partialLine: '' });
		return next;
	});
}

export function setActiveConnection(connectionId: string | null): void {
	serialState.update((state) => ({
		...state,
		activeConnectionId: connectionId
	}));
}
