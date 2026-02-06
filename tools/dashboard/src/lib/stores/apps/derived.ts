import { derived } from 'svelte/store';
import type { AppInfo } from '$lib/types/config';
import { monorepoInfo } from './discovery';
import { selectedAppName } from './state';

export const selectedApp = derived(
  [monorepoInfo, selectedAppName],
  ([$monorepoInfo, $selectedAppName]): AppInfo | null => {
    if (!$monorepoInfo || !$selectedAppName) return null;
    return $monorepoInfo.apps.find((app) => app.name === $selectedAppName) ?? null;
  }
);

export const appNames = derived(monorepoInfo, ($monorepoInfo) => {
  if (!$monorepoInfo) return [];
  return $monorepoInfo.apps.map((app) => app.name);
});
