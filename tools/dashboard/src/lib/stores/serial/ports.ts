import { invokeWithTimeout } from '$lib/utils/invoke';
import { pushToast } from '$lib/stores/toast';
import type { PortInfo } from '$lib/types/config';
import { serialState } from './state';

const SERIAL_LIST_TIMEOUT_MS = 5000;

export async function refreshPorts(): Promise<void> {
	try {
		const ports = await invokeWithTimeout<PortInfo[]>(
			'list_serial_ports',
			undefined,
			SERIAL_LIST_TIMEOUT_MS
		);
		serialState.update((state) => ({
			...state,
			availablePorts: ports
		}));
	} catch (error) {
		console.error('Failed to list ports:', error);
		pushToast('Failed to list serial ports', 'error');
	}
}
