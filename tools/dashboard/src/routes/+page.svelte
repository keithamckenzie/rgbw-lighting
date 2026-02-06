<script lang="ts">
  import { goto } from '$app/navigation';
  import { monorepoInfo, selectedApp } from '$lib/stores/apps';
  import { getEnvMetadata, FEATURE_LABELS } from '$lib/config/environmentMetadata';
  import AppSelector from '$lib/components/common/AppSelector.svelte';
  import FeatureIcon from '$lib/components/common/FeatureIcon.svelte';
  import type { DiscoveredEnvironment } from '$lib/types/config';

  function navigateToApp(appName: string, envName: string) {
    goto(`/app/${appName}?env=${encodeURIComponent(envName)}`);
  }

  // Separate hardware targets from test environments
  const hardwareEnvs = $derived(
    $selectedApp?.environments.filter((env) => env.is_hardware_target) ?? []
  );
  const testEnvs = $derived(
    $selectedApp?.environments.filter((env) => !env.is_hardware_target) ?? []
  );

  function getEnvMeta(env: DiscoveredEnvironment) {
    return getEnvMetadata(env.name, env.platform);
  }
</script>

<div class="h-full overflow-auto p-6">
  <div class="mx-auto max-w-4xl">
    <!-- Onboarding header -->
    <div class="card mb-8 bg-gradient-to-br from-zinc-850 to-zinc-900 p-6">
      <h1 class="mb-2 text-2xl font-bold text-white">RGBW Dashboard</h1>
      <p class="mb-4 text-zinc-300">
        Configure and upload firmware to your LED lighting controllers.
      </p>

      <div class="rounded-lg bg-zinc-900/50 p-4">
        <h2 class="mb-3 text-sm font-semibold uppercase tracking-wide text-zinc-400">
          Getting Started
        </h2>
        <ol class="space-y-2 text-sm text-zinc-300">
          <li class="flex items-start gap-3">
            <span class="flex h-5 w-5 flex-shrink-0 items-center justify-center rounded-full bg-primary-600 text-xs font-bold text-white">
              1
            </span>
            <span>Select an application (project) below</span>
          </li>
          <li class="flex items-start gap-3">
            <span class="flex h-5 w-5 flex-shrink-0 items-center justify-center rounded-full bg-primary-600 text-xs font-bold text-white">
              2
            </span>
            <span>Choose your target hardware (ESP32, ESP8266, etc.)</span>
          </li>
          <li class="flex items-start gap-3">
            <span class="flex h-5 w-5 flex-shrink-0 items-center justify-center rounded-full bg-primary-600 text-xs font-bold text-white">
              3
            </span>
            <span>Configure settings for your LED panel or strip</span>
          </li>
          <li class="flex items-start gap-3">
            <span class="flex h-5 w-5 flex-shrink-0 items-center justify-center rounded-full bg-primary-600 text-xs font-bold text-white">
              4
            </span>
            <span>Build and upload to your device</span>
          </li>
        </ol>
      </div>
    </div>

    <!-- App selector -->
    <h2 class="mb-4 text-lg font-semibold text-white">Select Application</h2>
    <div class="mb-8">
      <AppSelector />
    </div>

    {#if $selectedApp}
      <!-- App info header -->
      <div class="card card-bordered mb-6 p-4">
        <div class="flex items-start justify-between">
          <div>
            <h3 class="text-lg font-medium text-white">{$selectedApp.name}</h3>
            <p class="mt-1 text-sm text-zinc-400">{$selectedApp.path}</p>
          </div>
          {#if $selectedApp.has_config}
            <span class="badge badge-primary">
              Configurable
            </span>
          {:else}
            <span class="badge badge-neutral">
              Build only
            </span>
          {/if}
        </div>
      </div>

      <!-- Hardware targets -->
      {#if hardwareEnvs.length > 0}
        <div class="mb-6">
          <h4 class="mb-3 text-sm font-semibold uppercase tracking-wide text-zinc-400">
            Hardware Targets
          </h4>
          <div class="grid gap-4 sm:grid-cols-2">
            {#each hardwareEnvs as env}
              {@const meta = getEnvMeta(env)}
              <button
                type="button"
                class="card group flex flex-col p-4 text-left transition-all hover:shadow-lg hover:ring-1 hover:ring-primary-500/50"
                onclick={() => navigateToApp($selectedApp.name, env.name)}
              >
                <div class="mb-2 flex items-center justify-between">
                  <span class="text-lg font-semibold text-white">{meta.displayName}</span>
                  <span class="badge badge-neutral">
                    {env.name}
                  </span>
                </div>

                <p class="mb-3 text-sm text-zinc-400">{meta.description}</p>

                <!-- Features -->
                {#if meta.features && meta.features.length > 0}
                  <div class="mb-3 flex flex-wrap gap-1.5">
                    {#each meta.features as feature}
                      {@const featureInfo = FEATURE_LABELS[feature]}
                      {#if featureInfo}
                        <span class="badge badge-success">
                          <FeatureIcon icon={featureInfo.icon} size="sm" />
                          {featureInfo.label}
                        </span>
                      {/if}
                    {/each}
                  </div>
                {:else}
                  <div class="mb-3 flex flex-wrap gap-1.5">
                    <span class="badge badge-neutral">
                      Basic (no wireless)
                    </span>
                  </div>
                {/if}

                <!-- Board info -->
                {#if env.board}
                  <div class="text-xs text-zinc-500">
                    Board: {env.board}
                  </div>
                {/if}

                <!-- Action link -->
                <div class="mt-auto pt-3 text-sm font-medium text-primary-400 group-hover:text-primary-300">
                  Configure &amp; Build
                  <span class="ml-1 inline-block transition-transform group-hover:translate-x-1">&rarr;</span>
                </div>
              </button>
            {/each}
          </div>
        </div>
      {/if}

      <!-- Test environments -->
      {#if testEnvs.length > 0}
        <div class="card card-bordered p-4">
          <h4 class="mb-2 text-sm font-semibold uppercase tracking-wide text-zinc-400">
            Test Environments
          </h4>
          <p class="mb-3 text-xs text-zinc-500">
            Run unit tests on your computer without hardware connected.
          </p>
          <div class="flex flex-wrap gap-2">
            {#each testEnvs as env}
              <button
                type="button"
                class="btn btn-secondary"
                onclick={() => navigateToApp($selectedApp.name, env.name)}
              >
                {env.name}
              </button>
            {/each}
          </div>
        </div>
      {/if}
    {:else if $monorepoInfo && $monorepoInfo.apps.length === 0}
      <div class="card card-bordered p-8 text-center">
        <div class="mb-4 text-4xl">
          <svg class="mx-auto h-12 w-12 text-zinc-500" fill="none" viewBox="0 0 24 24" stroke="currentColor">
            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="1.5" d="M3 7v10a2 2 0 002 2h14a2 2 0 002-2V9a2 2 0 00-2-2h-6l-2-2H5a2 2 0 00-2 2z" />
          </svg>
        </div>
        <p class="text-zinc-400">No PlatformIO applications found in the apps/ directory.</p>
        <p class="mt-2 text-sm text-zinc-500">
          Create a new app by copying <code class="rounded bg-zinc-800 px-1.5 py-0.5">apps/example-rgbw/</code> to <code class="rounded bg-zinc-800 px-1.5 py-0.5">apps/your-app-name/</code>
        </p>
      </div>
    {/if}

    <!-- PlatformIO info -->
    {#if $monorepoInfo}
      <div class="card card-bordered mt-8 p-4">
        <h4 class="mb-2 text-xs font-semibold uppercase tracking-wide text-zinc-500">
          PlatformIO Path
        </h4>
        <code class="text-sm text-zinc-400">{$monorepoInfo.pio_path}</code>
      </div>
    {/if}
  </div>
</div>


