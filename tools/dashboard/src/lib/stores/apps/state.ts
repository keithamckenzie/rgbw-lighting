import { writable } from 'svelte/store';
import { getLocalStorageItem, setLocalStorageItem } from '$lib/utils/localStorage';

const STORAGE_KEY = 'rgbw-dashboard:selected-app';

// Store mutations are funneled through exported action functions to centralize side effects.
export const selectedAppName = writable<string | null>(null);

export function selectApp(appName: string): void {
  selectedAppName.set(appName);
}

export function getStoredAppName(): string | null {
  return getLocalStorageItem(STORAGE_KEY);
}

let unsubscribePersist: (() => void) | null = null;
let persistTimer: ReturnType<typeof setTimeout> | null = null;

if (typeof window !== 'undefined') {
  // One module-level subscription keeps localStorage in sync; HMR cleanup prevents leaks.
  unsubscribePersist = selectedAppName.subscribe((value) => {
    if (persistTimer) {
      clearTimeout(persistTimer);
    }
    persistTimer = setTimeout(() => {
      setLocalStorageItem(STORAGE_KEY, value);
    }, 150);
  });
}

if (import.meta.hot) {
  import.meta.hot.dispose(() => {
    unsubscribePersist?.();
    unsubscribePersist = null;
    if (persistTimer) {
      clearTimeout(persistTimer);
      persistTimer = null;
    }
  });
}
