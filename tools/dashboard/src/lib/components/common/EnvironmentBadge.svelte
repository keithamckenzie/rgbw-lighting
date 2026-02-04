<script lang="ts">
	import type { DiscoveredEnvironment } from '$lib/types/config';
	import { getPlatformLabel } from '$lib/utils/platform';

	export let env: DiscoveredEnvironment;
	export let size: 'sm' | 'md' = 'md';
	export let selected = false;

	$: isNative = env.platform === 'native';
	$: platformLabel = getPlatformLabel(env.platform);

	$: classes = [
		'inline-flex items-center gap-1 rounded border',
		size === 'sm' ? 'px-1.5 py-0.5 text-xs' : 'px-2 py-1 text-sm',
		isNative
			? 'border-slate-500 bg-slate-700 text-slate-300'
			: selected
				? 'border-primary-500 bg-primary-600 text-white'
				: 'border-slate-500 bg-slate-700 text-slate-200'
	].join(' ');
</script>

<span class={classes}>
	{env.name}
	{#if isNative}
		<span class="text-slate-400">(Tests)</span>
	{:else if env.board}
		<span class="text-slate-400">({platformLabel})</span>
	{/if}
</span>
