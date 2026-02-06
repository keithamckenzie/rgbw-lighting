<script lang="ts">
  import { goto } from '$app/navigation';
  import { page } from '$app/stores';
  import { monorepoInfo } from '$lib/stores/apps';
  import { configState, initConfig, profileLoading } from '$lib/stores/config';
  import { buildFlagsFromDefines } from '$lib/utils/buildFlags';
  import { getPlatformFromEnvironment } from '$lib/types/config';
  import { getLocalStorageItem, setLocalStorageItem } from '$lib/utils/localStorage';
  import { resolveSelection } from '$lib/utils/selection';
  import { getEnvMetadata, FEATURE_LABELS } from '$lib/config/environmentMetadata';
  import type { AppInfo, DiscoveredEnvironment } from '$lib/types/config';

  import ConfigEditor from '$lib/components/form/ConfigEditor.svelte';
  import AudioConfig from '$lib/components/form/AudioConfig.svelte';
  import BuildPanel from '$lib/components/build/BuildPanel.svelte';
  import ProfileManager from '$lib/components/common/ProfileManager.svelte';
  import FeatureIcon from '$lib/components/common/FeatureIcon.svelte';

  let app = $state<AppInfo | null>(null);
  let selectedEnv = $state<DiscoveredEnvironment | null>(null);
  let activeTab = $state<'config' | 'audio' | 'build'>('config');
  let sidebarOpen = $state(false);
  let pendingEnvNav = $state<string | null>(null);
  let lastInitKey = $state('');
  let lastAppName = $state('');

  const ENV_STORAGE_PREFIX = 'rgbw-dashboard:last-env:';

  function getStoredEnv(appName: string): string | null {
    return getLocalStorageItem(`${ENV_STORAGE_PREFIX}${appName}`);
  }

  function setStoredEnv(appName: string, envName: string): void {
    setLocalStorageItem(`${ENV_STORAGE_PREFIX}${appName}`, envName);
  }

  const appName = $derived($page.params.appName);
  const envParam = $derived($page.url.searchParams.get('env'));

  // Resolve app from monorepoInfo
  $effect(() => {
    if ($monorepoInfo && appName) {
      app = $monorepoInfo.apps.find((a) => a.name === appName) ?? null;
    } else {
      app = null;
    }
  });

  // Reset pendingEnvNav when app changes
  $effect(() => {
    if (appName !== lastAppName) {
      lastAppName = appName ?? '';
      pendingEnvNav = null;
    }
  });

  // Resolve selected environment
  $effect(() => {
    if (!app) {
      selectedEnv = null;
    } else {
      const envFromUrl =
        envParam && app.environments.some((e) => e.name === envParam) ? envParam : null;
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
        ? (app.environments.find((e) => e.name === resolvedEnvName) ?? null)
        : null;

      if (resolvedEnvName && envParam !== resolvedEnvName && !pendingEnvNav) {
        pendingEnvNav = resolvedEnvName;
        void goto(`/app/${app.name}?env=${encodeURIComponent(resolvedEnvName)}`, {
          replaceState: true,
          keepFocus: true,
          noScroll: true
        });
      }
    }
  });

  // Clear pendingEnvNav when URL catches up
  $effect(() => {
    if (envParam === pendingEnvNav) {
      pendingEnvNav = null;
    }
  });

  // Persist selected environment
  $effect(() => {
    if (app && selectedEnv) {
      setStoredEnv(app.name, selectedEnv.name);
    }
  });

  // Initialize config when app/env changes
  $effect(() => {
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
  });

  function selectEnvironment(env: DiscoveredEnvironment) {
    if (!app || env.name === envParam) {
      return;
    }

    pendingEnvNav = env.name;
    void goto(`/app/${app.name}?env=${encodeURIComponent(env.name)}`, {
      replaceState: true,
      keepFocus: true,
      noScroll: true
    });
  }

  const platform = $derived(selectedEnv ? getPlatformFromEnvironment(selectedEnv) : '');
  const canUpload = $derived(selectedEnv?.can_upload ?? false);
  const isTestEnv = $derived(selectedEnv?.platform === 'native');
  const buildFlags = $derived(buildFlagsFromDefines($configState.defines));

  // Reset audio tab if not on ESP32
  $effect(() => {
    if (activeTab === 'audio' && platform !== 'esp32') {
      activeTab = 'config';
    }
  });

  // Get environment metadata for display
  const envMeta = $derived(
    selectedEnv ? getEnvMetadata(selectedEnv.name, selectedEnv.platform) : null
  );

  // Tab descriptions for guidance
  const TAB_DESCRIPTIONS: Record<string, string> = {
    config: 'Panel dimensions, LED type, pin assignments, and hardware settings',
    audio: 'Sound-reactive lighting configuration (microphone, beat detection)',
    build: 'View generated compiler flags and build configuration'
  };
</script>

{#if !app}
  <div class="flex h-full items-center justify-center">
    <p class="text-zinc-400">App not found: {appName}</p>
  </div>
{:else}
  <div class="flex h-full">
    <!-- Mobile sidebar toggle -->
    <button
      type="button"
      class="fixed left-3 top-16 z-30 rounded-md bg-zinc-800 p-2 text-zinc-300 shadow-lg hover:bg-zinc-700 lg:hidden"
      aria-label={sidebarOpen ? 'Close sidebar' : 'Open sidebar'}
      onclick={() => (sidebarOpen = !sidebarOpen)}
    >
      <svg class="h-5 w-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
        {#if sidebarOpen}
          <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M6 18L18 6M6 6l12 12" />
        {:else}
          <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M4 6h16M4 12h16M4 18h16" />
        {/if}
      </svg>
    </button>

    <!-- Sidebar overlay on mobile -->
    {#if sidebarOpen}
      <button
        type="button"
        class="fixed inset-0 z-20 bg-black/50 lg:hidden"
        aria-label="Close sidebar"
        onclick={() => (sidebarOpen = false)}
      ></button>
    {/if}

    <!-- Sidebar -->
    <div
      class="sidebar w-80 flex-shrink-0 overflow-auto bg-zinc-900 max-lg:fixed max-lg:inset-y-0 max-lg:left-0 max-lg:z-30 max-lg:transition-transform max-lg:duration-200"
      class:max-lg:-translate-x-full={!sidebarOpen}
      class:max-lg:translate-x-0={sidebarOpen}
    >
      <!-- Contextual header -->
      <div class="border-b border-zinc-800 bg-zinc-950/50 p-4">
        <div class="mb-1 flex items-center gap-2 text-xs text-zinc-400">
          <a href="/" class="hover:text-zinc-300">{app.name}</a>
          <span>&rsaquo;</span>
          <span class="text-zinc-300">{envMeta?.displayName ?? selectedEnv?.name ?? 'Select'}</span>
        </div>
        <h2 class="text-lg font-bold text-white">
          {#if selectedEnv}
            Configure {envMeta?.displayName ?? selectedEnv.name}
          {:else}
            {app.name}
          {/if}
        </h2>
        {#if selectedEnv && envMeta}
          <p class="mt-1 text-sm text-zinc-400">{envMeta.description}</p>
          {#if envMeta.features && envMeta.features.length > 0}
            <div class="mt-2 flex flex-wrap gap-1.5">
              {#each envMeta.features as feature}
                {@const featureInfo = FEATURE_LABELS[feature]}
                {#if featureInfo}
                  <span class="badge badge-success">
                    <FeatureIcon icon={featureInfo.icon} size="sm" />
                    {featureInfo.label}
                  </span>
                {/if}
              {/each}
            </div>
          {/if}
        {/if}
        {#if app.has_config && selectedEnv}
          <p class="mt-2 text-xs text-zinc-500">
            Adjust settings below, then click "Build" to compile. Connect your device and click "Upload" to flash.
          </p>
        {/if}
      </div>

      <!-- Environment selector -->
      <div class="border-t border-zinc-800 p-4">
        <h3 class="mb-2 text-sm font-medium text-zinc-300">Environment</h3>
        <div class="flex flex-wrap gap-2">
          {#each app.environments as env}
            <button
              type="button"
              class="rounded-md border px-3 py-1.5 text-sm transition-all focus:outline-none focus:ring-2 focus:ring-primary-500/50"
              class:border-primary-500={selectedEnv?.name === env.name}
              class:bg-primary-600={selectedEnv?.name === env.name}
              class:text-white={selectedEnv?.name === env.name}
              class:shadow-sm={selectedEnv?.name === env.name}
              class:border-zinc-700={selectedEnv?.name !== env.name}
              class:bg-zinc-800={selectedEnv?.name !== env.name}
              class:text-zinc-300={selectedEnv?.name !== env.name}
              class:hover:border-zinc-600={selectedEnv?.name !== env.name}
              class:hover:bg-zinc-700={selectedEnv?.name !== env.name}
              onclick={() => selectEnvironment(env)}
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
      <div class="border-t border-zinc-800">
        <div class="flex" role="tablist" aria-label="Configuration tabs">
          <button
            type="button"
            role="tab"
            id="tab-config"
            aria-selected={activeTab === 'config'}
            aria-controls="tabpanel"
            class="flex-1 border-b-2 px-4 py-3 text-sm font-medium transition-colors"
            class:border-primary-500={activeTab === 'config'}
            class:text-primary-400={activeTab === 'config'}
            class:border-transparent={activeTab !== 'config'}
            class:text-zinc-400={activeTab !== 'config'}
            class:hover:text-zinc-300={activeTab !== 'config'}
            onclick={() => (activeTab = 'config')}
            onkeydown={(e) => {
              const next = e.key === 'ArrowRight'
                ? (selectedEnv && platform === 'esp32' ? 'audio' : 'build')
                : e.key === 'ArrowLeft' ? 'build' : null;
              if (next) { activeTab = next; document.getElementById(`tab-${next}`)?.focus(); }
            }}
            tabindex={activeTab === 'config' ? 0 : -1}
          >
            Config
          </button>
          {#if selectedEnv && platform === 'esp32'}
            <button
              type="button"
              role="tab"
              id="tab-audio"
              aria-selected={activeTab === 'audio'}
              aria-controls="tabpanel"
              class="flex-1 border-b-2 px-4 py-3 text-sm font-medium transition-colors"
              class:border-primary-500={activeTab === 'audio'}
              class:text-primary-400={activeTab === 'audio'}
              class:border-transparent={activeTab !== 'audio'}
              class:text-zinc-400={activeTab !== 'audio'}
              class:hover:text-zinc-300={activeTab !== 'audio'}
              onclick={() => (activeTab = 'audio')}
              onkeydown={(e) => {
                const next = e.key === 'ArrowRight' ? 'build' : e.key === 'ArrowLeft' ? 'config' : null;
                if (next) { activeTab = next; document.getElementById(`tab-${next}`)?.focus(); }
              }}
              tabindex={activeTab === 'audio' ? 0 : -1}
            >
              Audio
            </button>
          {/if}
          <button
            type="button"
            role="tab"
            id="tab-build"
            aria-selected={activeTab === 'build'}
            aria-controls="tabpanel"
            class="flex-1 border-b-2 px-4 py-3 text-sm font-medium transition-colors"
            class:border-primary-500={activeTab === 'build'}
            class:text-primary-400={activeTab === 'build'}
            class:border-transparent={activeTab !== 'build'}
            class:text-zinc-400={activeTab !== 'build'}
            class:hover:text-zinc-300={activeTab !== 'build'}
            onclick={() => (activeTab = 'build')}
            onkeydown={(e) => {
              const next = e.key === 'ArrowRight' ? 'config'
                : e.key === 'ArrowLeft' ? (selectedEnv && platform === 'esp32' ? 'audio' : 'config') : null;
              if (next) { activeTab = next; document.getElementById(`tab-${next}`)?.focus(); }
            }}
            tabindex={activeTab === 'build' ? 0 : -1}
          >
            Build
          </button>
        </div>
        <!-- Tab description -->
        <div class="border-b border-zinc-800 bg-zinc-950/30 px-4 py-2">
          <p class="text-xs text-zinc-500">{TAB_DESCRIPTIONS[activeTab]}</p>
        </div>
      </div>

      <!-- Tab content -->
      <div
        class="p-4"
        role="tabpanel"
        id="tabpanel"
        aria-labelledby="tab-{activeTab}"
      >
        {#if activeTab === 'config'}
          {#if app.has_config && selectedEnv}
            <ConfigEditor
              schema={app.config_schema}
              environment={selectedEnv}
              excludeAudioFields={platform === 'esp32'}
            />
            <div class="mt-4">
              <ProfileManager appName={app.name} environments={app.environments} />
            </div>
          {:else}
            <p class="text-zinc-400">
              This app has no configurable options. Select an environment and build.
            </p>
          {/if}
        {:else if activeTab === 'audio'}
          <AudioConfig {platform} schema={app.config_schema} />
        {:else if activeTab === 'build'}
          <div class="space-y-4">
            <div class="card card-bordered p-3">
              <h4 class="mb-2 text-sm font-medium text-zinc-300">Build Flags</h4>
              <div class="max-h-40 overflow-auto text-xs">
                {#each buildFlags as flag}
                  <div class="text-zinc-400">{flag}</div>
                {:else}
                  <div class="text-zinc-500">No custom flags</div>
                {/each}
              </div>
            </div>
          </div>
        {/if}
      </div>
    </div>

    <!-- Main panel: Build output -->
    <div class="flex-1 bg-zinc-950">
      {#if selectedEnv}
        <BuildPanel appName={app.name} environment={selectedEnv.name} {canUpload} {isTestEnv} />
      {:else}
        <div class="flex h-full items-center justify-center">
          <p class="text-zinc-400">Select an environment to build</p>
        </div>
      {/if}
    </div>
  </div>
{/if}

<style>
  .sidebar {
    box-shadow: 4px 0 6px -1px rgb(0 0 0 / 0.3);
  }
</style>
