export function resolveSelection<T extends string>(
	urlValue: T | null,
	storedValue: T | null,
	available: T[],
	defaultValue: T | null = null
): T | null {
	if (urlValue && available.includes(urlValue)) return urlValue;
	if (storedValue && available.includes(storedValue)) return storedValue;
	if (defaultValue && available.includes(defaultValue)) return defaultValue;
	if (available.length > 0) return available[0];
	return defaultValue;
}
