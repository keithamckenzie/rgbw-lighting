<script lang="ts">
  import type { DiscoveredEnvironment } from '$lib/types/config';
  import { getPlatformLabel } from '$lib/utils/platform';

  let {
    env,
    size = 'md',
    selected = false
  }: {
    env: DiscoveredEnvironment;
    size?: 'sm' | 'md';
    selected?: boolean;
  } = $props();

  const isNative = $derived(env.platform === 'native');
  const platformLabel = $derived(getPlatformLabel(env.platform));

  const classes = $derived(
    [
      'inline-flex items-center gap-1 rounded-md border',
      size === 'sm' ? 'px-1.5 py-0.5 text-xs' : 'px-2 py-1 text-sm',
      isNative
        ? 'border-zinc-600 bg-zinc-800 text-zinc-300'
        : selected
          ? 'border-primary-500 bg-primary-600 text-white'
          : 'border-zinc-600 bg-zinc-800 text-zinc-200'
    ].join(' ')
  );
</script>

<span class={classes}>
  {env.name}
  {#if isNative}
    <span class="text-zinc-400">(Tests)</span>
  {:else if env.board}
    <span class="text-zinc-400">({platformLabel})</span>
  {/if}
</span>
