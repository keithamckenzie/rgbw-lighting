import { writable, get } from 'svelte/store';
import type { MonorepoInfo } from '$lib/types/config';
import { invokeWithTimeout } from '$lib/utils/invoke';
import { resolveSelection } from '$lib/utils/selection';
import { selectedAppName, getStoredAppName } from './state';

// Store mutations are funneled through exported action functions to centralize side effects.
export const monorepoInfo = writable<MonorepoInfo | null>(null);
export const monorepoError = writable<string | null>(null);
export const isLoading = writable(true);

const DISCOVER_TIMEOUT_MS = 20000;
let loadInFlight = false;

export async function loadApps(): Promise<void> {
  if (loadInFlight) return;
  loadInFlight = true;
  isLoading.set(true);
  monorepoError.set(null);

  try {
    const info = await invokeWithTimeout<MonorepoInfo>(
      'discover_apps',
      undefined,
      DISCOVER_TIMEOUT_MS
    );
    monorepoInfo.set(info);

    const current = get(selectedAppName);
    const stored = getStoredAppName();
    const available = info.apps.map((app) => app.name);
    const next = resolveSelection(current, stored, available, available[0] ?? null);
    selectedAppName.set(next);
  } catch (error) {
    monorepoError.set(error instanceof Error ? error.message : String(error));
  } finally {
    loadInFlight = false;
    isLoading.set(false);
  }
}
