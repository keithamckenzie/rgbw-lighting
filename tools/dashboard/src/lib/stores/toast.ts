import { writable } from 'svelte/store';

export type ToastKind = 'info' | 'success' | 'error' | 'warning';

export interface Toast {
	id: string;
	message: string;
	kind: ToastKind;
}

export const toasts = writable<Toast[]>([]);
const toastTimeouts = new Map<string, ReturnType<typeof setTimeout>>();

function removeToast(id: string) {
	const timeout = toastTimeouts.get(id);
	if (timeout) {
		clearTimeout(timeout);
		toastTimeouts.delete(id);
	}
	toasts.update((list) => list.filter((toast) => toast.id !== id));
}

export function pushToast(
	message: string,
	kind: ToastKind = 'info',
	durationMs = 4000
): void {
	const id = `${Date.now()}-${Math.random().toString(36).slice(2, 8)}`;
	toasts.update((list) => [...list, { id, message, kind }]);

	if (durationMs > 0) {
		const timeout = setTimeout(() => removeToast(id), durationMs);
		toastTimeouts.set(id, timeout);
	}
}

export function clearToastTimeouts(): void {
	for (const timeout of toastTimeouts.values()) {
		clearTimeout(timeout);
	}
	toastTimeouts.clear();
}
