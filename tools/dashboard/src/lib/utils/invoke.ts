// Default timeout is short; long-running commands should pass an explicit timeout.
export async function invokeWithTimeout<T>(
	command: string,
	args?: Record<string, unknown>,
	timeoutMs = 30000
): Promise<T> {
	const { invoke } = await import('@tauri-apps/api/core');

	let timedOut = false;
	let timeoutId: ReturnType<typeof setTimeout> | null = null;
	const timeoutPromise = new Promise<never>((_, reject) => {
		timeoutId = setTimeout(() => {
			timedOut = true;
			reject(new Error(`Command '${command}' timed out after ${timeoutMs}ms`));
		}, timeoutMs);
	});

	try {
		const invokePromise = invoke<T>(command, args).catch((error) => {
			// If the timeout already fired, swallow late rejections to avoid warnings.
			if (timedOut) {
				return new Promise<T>(() => {});
			}
			throw error;
		});
		return await Promise.race([invokePromise, timeoutPromise]);
	} finally {
		if (timeoutId) clearTimeout(timeoutId);
	}
}
