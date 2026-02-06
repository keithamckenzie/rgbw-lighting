<script lang="ts">
  import { monorepoInfo, selectedAppName, selectedApp, selectApp } from '$lib/stores/apps';
  import EnvironmentBadge from './EnvironmentBadge.svelte';

  let isOpen = $state(false);
  let container: HTMLDivElement | null = $state(null);
  const listboxId = 'app-selector-listbox';

  function handleSelect(appName: string) {
    selectApp(appName);
    isOpen = false;
  }

  function handleDocumentClick(event: MouseEvent) {
    if (!container) return;
    const target = event.target as Node | null;
    if (target && !container.contains(target)) {
      isOpen = false;
    }
  }

  function handleDocumentKeyDown(event: KeyboardEvent) {
    if (event.key === 'Escape' && isOpen) {
      isOpen = false;
    }
  }

  $effect(() => {
    document.addEventListener('click', handleDocumentClick);
    document.addEventListener('keydown', handleDocumentKeyDown);
    return () => {
      document.removeEventListener('click', handleDocumentClick);
      document.removeEventListener('keydown', handleDocumentKeyDown);
    };
  });
</script>

<div class="relative" bind:this={container}>
  <button
    type="button"
    aria-haspopup="listbox"
    aria-expanded={isOpen}
    aria-controls={listboxId}
    class="input-base flex items-center justify-between px-4 py-2 text-left hover:border-zinc-600"
    onclick={() => (isOpen = !isOpen)}
  >
    <span class="flex items-center gap-2">
      {#if $selectedApp}
        <span class="font-medium">{$selectedApp.name}</span>
        {#if $selectedApp.has_config}
          <span class="badge badge-primary">Configurable</span>
        {/if}
      {:else}
        <span class="text-zinc-400">Select an app...</span>
      {/if}
    </span>
    <svg
      class="h-5 w-5 text-zinc-400 transition-transform"
      class:rotate-180={isOpen}
      fill="none"
      stroke="currentColor"
      viewBox="0 0 24 24"
    >
      <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M19 9l-7 7-7-7" />
    </svg>
  </button>

  {#if isOpen && $monorepoInfo}
    <div
      id={listboxId}
      role="listbox"
      class="card absolute z-50 mt-1 max-h-80 w-full overflow-auto shadow-lg"
    >
      {#each $monorepoInfo.apps as app}
        <button
          type="button"
          role="option"
          aria-selected={$selectedAppName === app.name}
          class="flex w-full flex-col gap-1 px-4 py-3 text-left transition-colors hover:bg-zinc-800"
          class:bg-zinc-800={$selectedAppName === app.name}
          onclick={() => handleSelect(app.name)}
        >
          <div class="flex items-center gap-2">
            <span class="font-medium text-white">{app.name}</span>
            {#if app.has_config}
              <span class="badge badge-primary">Configurable</span>
            {:else}
              <span class="badge badge-neutral">Build only</span>
            {/if}
          </div>

          <div class="flex flex-wrap gap-1">
            {#each app.environments as env}
              <EnvironmentBadge {env} size="sm" />
            {/each}
          </div>
        </button>
      {/each}
    </div>
  {/if}
</div>
