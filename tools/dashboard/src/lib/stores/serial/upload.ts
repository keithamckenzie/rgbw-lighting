import { invokeWithTimeout } from '$lib/utils/invoke';

const SERIAL_CMD_TIMEOUT_MS = 10000;

export async function acquirePortForUpload(portPath: string): Promise<void> {
	await invokeWithTimeout('acquire_port_for_upload', { portPath }, SERIAL_CMD_TIMEOUT_MS);
}

export async function releaseUploadLock(portPath: string): Promise<void> {
	await invokeWithTimeout('release_upload_lock', { portPath }, SERIAL_CMD_TIMEOUT_MS);
}
