<script lang="ts">
  import { autoScroll } from '$lib/utils/scroll';

  let {
    lines = [],
    hasConnection = false,
    resetKey = null
  }: {
    lines?: string[];
    hasConnection?: boolean;
    resetKey?: string | number | null;
  } = $props();
</script>

<div
  use:autoScroll={{ enabled: lines.length > 0, resetKey }}
  class="scrollbar-thin terminal-output flex-1 overflow-auto bg-zinc-950 p-3"
  aria-live="polite"
>
  {#if !hasConnection}
    <div class="flex h-full items-center justify-center" role="status">
      <p class="text-zinc-500">Select a port and click "Connect" to see serial output.</p>
    </div>
  {:else if lines.length === 0}
    <div class="flex h-full items-center justify-center" role="status">
      <p class="text-zinc-500">Connected. Waiting for data...</p>
    </div>
  {:else}
    {#each lines as line}
      <div class="text-zinc-300">{line}</div>
    {/each}
  {/if}
</div>
