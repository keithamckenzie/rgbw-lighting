<script lang="ts">
	import { autoScroll } from '$lib/utils/scroll';
	export let lines: string[] = [];
	export let hasConnection = false;
	export let resetKey: string | number | null = null;
</script>

<div
	use:autoScroll={{ enabled: lines.length > 0, resetKey }}
	class="scrollbar-thin terminal-output flex-1 overflow-auto bg-slate-900 p-3"
	aria-live="polite"
>
	{#if !hasConnection}
		<p class="text-slate-500">Connect to a serial port to see output...</p>
	{:else if lines.length === 0}
		<p class="text-slate-500">Waiting for data...</p>
	{:else}
		{#each lines as line}
			<div class="text-slate-300">{line}</div>
		{/each}
	{/if}
</div>
