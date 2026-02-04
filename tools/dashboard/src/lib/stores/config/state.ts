import { writable, get } from 'svelte/store';
import type { AppInfo, AppConfigState, DiscoveredEnvironment, SavedProfile } from '$lib/types/config';
import { getPlatformFromEnvironment } from '$lib/types/config';
import { buildFlagsFromDefines } from '$lib/utils/buildFlags';
import { getPlatformKey } from '$lib/utils/platform';

// Store mutations are funneled through exported action functions to centralize side effects.
// Current configuration state
export const configState = writable<AppConfigState>({
	appName: '',
	environment: '',
	defines: {},
	isDirty: false
});

// Actions
export function initConfig(app: AppInfo, environment: DiscoveredEnvironment): void {
	const defines: Record<string, string> = {};
	const platform = getPlatformFromEnvironment(environment);
	const platformKey = getPlatformKey(platform);

	const visibleDefines = [
		...app.config_schema.defines,
		...(app.config_schema.platform_conditional[platformKey] || []),
		...(app.config_schema.platform_conditional[platform] || [])
	];

	// Initialize with default values from schema (including platform conditionals)
	for (const define of visibleDefines) {
		defines[define.name] = define.default_value;
	}

	configState.set({
		appName: app.name,
		environment: environment.name,
		defines,
		isDirty: false
	});
}

export function updateDefine(name: string, value: string): void {
	configState.update((state) => ({
		...state,
		defines: { ...state.defines, [name]: value },
		isDirty: true
	}));
}

export function resetDirty(): void {
	configState.update((state) => ({
		...state,
		isDirty: false
	}));
}

export function applyProfile(profile: SavedProfile): void {
	configState.set({
		appName: profile.app_name,
		environment: profile.environment,
		defines: profile.defines,
		isDirty: false
	});
}

export function generateBuildFlags(): string[] {
	const state = get(configState);
	return buildFlagsFromDefines(state.defines);
}
