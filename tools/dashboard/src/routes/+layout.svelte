<script lang="ts">
  import '../app.css';
  import type { Snippet } from 'svelte';
  import { page } from '$app/stores';
  import { loadApps, monorepoInfo, monorepoError, isLoading } from '$lib/stores/apps';
  import ToastHost from '$lib/components/common/ToastHost.svelte';

  let { children }: { children: Snippet } = $props();

  // Load apps on mount
  $effect(() => {
    loadApps();
  });

  const currentPath = $derived($page.url.pathname);
</script>

<div class="flex h-screen flex-col bg-zinc-950 text-white">
  <a href="#main" class="sr-only focus:not-sr-only focus:absolute focus:z-[100] focus:rounded-md focus:bg-primary-600 focus:px-4 focus:py-2 focus:text-white">
    Skip to main content
  </a>
  <!-- Header -->
  <header
    class="header flex items-center justify-between px-4 py-3"
  >
    <div class="flex items-center gap-4">
      <h1 class="text-xl font-bold text-white">RGBW Dashboard</h1>
      {#if $monorepoInfo}
        <span class="text-sm text-zinc-400">{$monorepoInfo.path}</span>
      {/if}
    </div>

    <nav class="flex items-center gap-2">
      <a
        href="/"
        class="rounded-md px-3 py-2 text-sm font-medium transition-colors"
        class:bg-primary-600={currentPath === '/'}
        class:text-white={currentPath === '/'}
        class:text-zinc-300={currentPath !== '/'}
        class:hover:bg-zinc-800={currentPath !== '/'}
      >
        Apps
      </a>
      <a
        href="/serial"
        class="rounded-md px-3 py-2 text-sm font-medium transition-colors"
        class:bg-primary-600={currentPath === '/serial'}
        class:text-white={currentPath === '/serial'}
        class:text-zinc-300={currentPath !== '/serial'}
        class:hover:bg-zinc-800={currentPath !== '/serial'}
      >
        Serial Monitor
      </a>
      <button
        type="button"
        class="btn btn-secondary"
        disabled={$isLoading}
        onclick={() => loadApps()}
        title="Rescan apps and config"
      >
        {$isLoading ? 'Rescanning...' : 'Rescan'}
      </button>
    </nav>
  </header>

  <!-- Main content -->
  <main id="main" class="flex-1 overflow-hidden">
    {#if $isLoading}
      <div class="flex h-full items-center justify-center" role="status" aria-label="Loading project">
        <div class="text-center">
          <div
            class="mb-4 h-12 w-12 animate-spin rounded-full border-4 border-primary-500 border-t-transparent mx-auto"
            aria-hidden="true"
          ></div>
          <p class="text-zinc-400">Loading project...</p>
        </div>
      </div>
    {:else if $monorepoError}
      <div class="flex h-full items-center justify-center p-8">
        <div class="max-w-2xl rounded-lg border border-red-800 bg-red-950 p-6">
          <h2 class="mb-4 text-xl font-bold text-red-400">Failed to Load Project</h2>
          <pre class="whitespace-pre-wrap text-sm text-red-300">{$monorepoError}</pre>
        </div>
      </div>
    {:else}
      {@render children()}
    {/if}
  </main>
</div>

<ToastHost />

<style>
  .header {
    background: rgb(24 24 27); /* zinc-900 */
    border-bottom: 1px solid rgb(39 39 42); /* zinc-800 */
    box-shadow: 0 1px 3px 0 rgb(0 0 0 / 0.3);
  }
</style>
