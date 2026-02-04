<script lang="ts">
	import { goto } from '$app/navigation';
	import { page } from '$app/stores';
	import { monorepoInfo } from '$lib/stores/apps';
	import { configState, initConfig, profileLoading } from '$lib/stores/config';
	import { buildFlagsFromDefines } from '$lib/utils/buildFlags';
	import { getPlatformFromEnvironment } from '$lib/types/config';
	import { getLocalStorageItem, setLocalStorageItem } from '$lib/utils/localStorage';
	import { resolveSelection } from '$lib/utils/selection';
	import type { AppInfo, DiscoveredEnvironment } from '$lib/types/config';

	import ConfigEditor from '$lib/components/form/ConfigEditor.svelte';
	import AudioConfig from '$lib/components/form/AudioConfig.svelte';
	import BuildPanel from '$lib/components/build/BuildPanel.svelte';
	import ProfileManager from '$lib/components/common/ProfileManager.svelte';

	let app: AppInfo | null = null;
	let selectedEnv: DiscoveredEnvironment | null = null;
	let activeTab: 'config' | 'audio' | 'build' = 'config';
	let pendingEnvNav: string | null = null;
	let lastInitKey = '';
	let lastAppName = '';

	const ENV_STORAGE_PREFIX = 'rgbw-dashboard:last-env:';

	function getStoredEnv(appName: string): string | null {
		return getLocalStorageItem(`${ENV_STORAGE_PREFIX}${appName}`);
	}

	function setStoredEnv(appName: string, envName: string): void {
		setLocalStorageItem(`${ENV_STORAGE_PREFIX}${appName}`, envName);
	}

	$: appName = $page.params.appName;
	$: envParam = $page.url.searchParams.get('env');

	$: {
		if ($monorepoInfo && appName) {
			app = $monorepoInfo.apps.find((a) => a.name === appName) ?? null;
		} else {
			app = null;
		}
	}

	$: {
		if (appName !== lastAppName) {
			lastAppName = appName;
			pendingEnvNav = null;
		}
	}

	$: {
		if (!app) {
			selectedEnv = null;
		} else {
			const envFromUrl = envParam && app.environments.some((e) => e.name === envParam) ? envParam : null;
			const storedEnv =
				app && app.name
					? (() => {
							const stored = getStoredEnv(app.name);
							return stored && app.environments.some((e) => e.name === stored) ? stored : null;
					  })()
					: null;
			const available = app.environments.map((env) => env.name);
			const preferred =
				app.environments.find((env) => env.is_hardware_target)?.name ?? available[0] ?? null;
			const resolvedEnvName = resolveSelection(envFromUrl, storedEnv, available, preferred);

			selectedEnv = resolvedEnvName
				? app.environments.find((e) => e.name === resolvedEnvName) ?? null
				: null;

			if (resolvedEnvName && envParam !== resolvedEnvName && !pendingEnvNav) {
				pendingEnvNav = resolvedEnvName;
				void goto(`/app/${app.name}?env=${encodeURIComponent(resolvedEnvName)}`, {
					replaceState: true,
					keepfocus: true,
					noScroll: true
				});
			}
		}
	}

	$: if (envParam === pendingEnvNav) {
		pendingEnvNav = null;
	}

	$: if (app && selectedEnv) {
		setStoredEnv(app.name, selectedEnv.name);
	}

	$: {
		if (app && selectedEnv) {
			const key = `${app.name}:${selectedEnv.name}`;
			if (lastInitKey !== key) {
				lastInitKey = key;
				if (!$profileLoading) {
					initConfig(app, selectedEnv);
				}
			}
		} else {
			lastInitKey = '';
		}
	}

	function selectEnvironment(env: DiscoveredEnvironment) {
		if (!app || env.name === envParam) {
			return;
		}

		pendingEnvNav = env.name;
		void goto(`/app/${app.name}?env=${encodeURIComponent(env.name)}`, {
			replaceState: true,
			keepfocus: true,
			noScroll: true
		});
	}

	$: platform = selectedEnv ? getPlatformFromEnvironment(selectedEnv) : '';
	$: canUpload = selectedEnv?.can_upload ?? false;
	$: isTestEnv = selectedEnv?.platform === 'native';
	$: buildFlags = buildFlagsFromDefines($configState.defines);

	$: if (activeTab === 'audio' && platform !== 'esp32') {
		activeTab = 'config';
	}
</script>

{#if !app}
	<div class="flex h-full items-center justify-center">
		<p class="text-slate-400">App not found: {appName}</p>
	</div>
{:else}
	<div class="flex h-full">
		<!-- Sidebar -->
		<div class="w-80 flex-shrink-0 overflow-auto border-r border-slate-700 bg-slate-800">
			<div class="p-4">
				<h2 class="text-lg font-bold text-white">{app.name}</h2>
				{#if app.has_config}
					<span class="mt-1 inline-block rounded bg-primary-600 px-2 py-0.5 text-xs text-white">
						Configurable
					</span>
				{/if}
			</div>

			<!-- Environment selector -->
			<div class="border-t border-slate-700 p-4">
				<h3 class="mb-2 text-sm font-medium text-slate-300">Environment</h3>
				<div class="flex flex-wrap gap-2">
					{#each app.environments as env}
						<button
							type="button"
							class="rounded border px-3 py-1.5 text-sm transition-colors focus:outline-none focus:ring-2 focus:ring-primary-500"
							class:border-primary-500={selectedEnv?.name === env.name}
							class:bg-primary-600={selectedEnv?.name === env.name}
							class:text-white={selectedEnv?.name === env.name}
							class:border-slate-600={selectedEnv?.name !== env.name}
							class:text-slate-300={selectedEnv?.name !== env.name}
							class:hover:border-slate-500={selectedEnv?.name !== env.name}
							on:click={() => selectEnvironment(env)}
						>
							{env.name}
							{#if !env.is_hardware_target}
								<span class="ml-1 text-xs opacity-75">(Tests)</span>
							{/if}
						</button>
					{/each}
				</div>
			</div>

			<!-- Tabs -->
			<div class="border-t border-slate-700">
				<nav class="flex">
					<button
						type="button"
						class="flex-1 border-b-2 px-4 py-3 text-sm font-medium transition-colors"
						class:border-primary-500={activeTab === 'config'}
						class:text-primary-400={activeTab === 'config'}
						class:border-transparent={activeTab !== 'config'}
						class:text-slate-400={activeTab !== 'config'}
						class:hover:text-slate-300={activeTab !== 'config'}
						on:click={() => (activeTab = 'config')}
					>
						Config
					</button>
					{#if selectedEnv && platform === 'esp32'}
						<button
							type="button"
							class="flex-1 border-b-2 px-4 py-3 text-sm font-medium transition-colors"
							class:border-primary-500={activeTab === 'audio'}
							class:text-primary-400={activeTab === 'audio'}
							class:border-transparent={activeTab !== 'audio'}
							class:text-slate-400={activeTab !== 'audio'}
							class:hover:text-slate-300={activeTab !== 'audio'}
							on:click={() => (activeTab = 'audio')}
						>
							Audio
						</button>
					{/if}
					<button
						type="button"
						class="flex-1 border-b-2 px-4 py-3 text-sm font-medium transition-colors"
						class:border-primary-500={activeTab === 'build'}
						class:text-primary-400={activeTab === 'build'}
						class:border-transparent={activeTab !== 'build'}
						class:text-slate-400={activeTab !== 'build'}
						class:hover:text-slate-300={activeTab !== 'build'}
						on:click={() => (activeTab = 'build')}
					>
						Build
					</button>
				</nav>
			</div>

			<!-- Tab content -->
			<div class="p-4">
				{#if activeTab === 'config'}
					{#if app.has_config && selectedEnv}
						<ConfigEditor schema={app.config_schema} environment={selectedEnv} />
						<div class="mt-4">
							<ProfileManager appName={app.name} environments={app.environments} />
						</div>
					{:else}
						<p class="text-slate-400">
							This app has no configurable options. Select an environment and build.
						</p>
					{/if}
				{:else if activeTab === 'audio'}
					<AudioConfig {platform} schema={app.config_schema} />
				{:else if activeTab === 'build'}
					<div class="space-y-4">
						<div class="rounded border border-slate-700 bg-slate-900 p-3">
							<h4 class="mb-2 text-sm font-medium text-slate-300">Build Flags</h4>
							<div class="max-h-40 overflow-auto text-xs">
								{#each buildFlags as flag}
									<div class="text-slate-400">{flag}</div>
								{:else}
									<div class="text-slate-500">No custom flags</div>
								{/each}
							</div>
						</div>
					</div>
				{/if}
			</div>
		</div>

		<!-- Main panel: Build output -->
		<div class="flex-1">
			{#if selectedEnv}
				<BuildPanel
					appName={app.name}
					environment={selectedEnv.name}
					{canUpload}
					{isTestEnv}
				/>
			{:else}
				<div class="flex h-full items-center justify-center">
					<p class="text-slate-400">Select an environment to build</p>
				</div>
			{/if}
		</div>
	</div>
{/if}
