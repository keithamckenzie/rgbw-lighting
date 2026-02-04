export type DebouncedFunction<T extends (...args: any[]) => void> = T & {
	cancel: () => void;
};

export function debounce<T extends (...args: any[]) => void>(
	fn: T,
	waitMs: number
): DebouncedFunction<T> {
	let timeout: ReturnType<typeof setTimeout> | null = null;

	const debounced = ((...args: Parameters<T>) => {
		if (timeout) {
			clearTimeout(timeout);
		}
		timeout = setTimeout(() => {
			timeout = null;
			fn(...args);
		}, waitMs);
	}) as DebouncedFunction<T>;

	debounced.cancel = () => {
		if (timeout) {
			clearTimeout(timeout);
			timeout = null;
		}
	};

	return debounced;
}
