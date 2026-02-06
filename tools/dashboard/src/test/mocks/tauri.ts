import { vi } from 'vitest';

// Mock @tauri-apps/api/core
export const invoke = vi.fn();

// Mock @tauri-apps/api/event
type EventCallback<T> = (event: { payload: T }) => void;

const listeners = new Map<string, Set<EventCallback<unknown>>>();

export const listen = vi.fn(
	async <T>(event: string, handler: EventCallback<T>): Promise<() => void> => {
		if (!listeners.has(event)) {
			listeners.set(event, new Set());
		}
		listeners.get(event)!.add(handler as EventCallback<unknown>);

		return () => {
			listeners.get(event)?.delete(handler as EventCallback<unknown>);
		};
	}
);

export const emit = vi.fn(async <T>(event: string, payload?: T): Promise<void> => {
	const handlers = listeners.get(event);
	if (handlers) {
		for (const handler of handlers) {
			handler({ payload: payload as unknown });
		}
	}
});

export function clearListeners(): void {
	listeners.clear();
}

// Register module mocks
vi.mock('@tauri-apps/api/core', () => ({
	invoke
}));

vi.mock('@tauri-apps/api/event', () => ({
	listen,
	emit
}));
