<script lang="ts">
	import { onMount, onDestroy } from 'svelte';
	import { monorepoInfo, selectedAppName, selectedApp, selectApp } from '$lib/stores/apps';
	import EnvironmentBadge from './EnvironmentBadge.svelte';

	let isOpen = false;
	let container: HTMLDivElement | null = null;
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

	onMount(() => {
		// Listeners are tied to this component's lifecycle; closures always see current state.
		document.addEventListener('click', handleDocumentClick);
		document.addEventListener('keydown', handleDocumentKeyDown);
	});

	onDestroy(() => {
		document.removeEventListener('click', handleDocumentClick);
		document.removeEventListener('keydown', handleDocumentKeyDown);
	});
</script>

<div class="relative" bind:this={container}>
	<button
		type="button"
		aria-haspopup="listbox"
		aria-expanded={isOpen}
		aria-controls={listboxId}
		class="flex w-full items-center justify-between rounded-lg border border-slate-600 bg-slate-700 px-4 py-2 text-left text-white hover:bg-slate-600"
		on:click={() => (isOpen = !isOpen)}
	>
		<span class="flex items-center gap-2">
			{#if $selectedApp}
				<span class="font-medium">{$selectedApp.name}</span>
				{#if $selectedApp.has_config}
					<span class="rounded bg-primary-600 px-1.5 py-0.5 text-xs">Configurable</span>
				{/if}
			{:else}
				<span class="text-slate-400">Select an app...</span>
			{/if}
		</span>
		<svg
			class="h-5 w-5 text-slate-400 transition-transform"
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
			class="absolute z-50 mt-1 max-h-80 w-full overflow-auto rounded-lg border border-slate-600 bg-slate-800 shadow-lg"
		>
			{#each $monorepoInfo.apps as app}
				<button
					type="button"
					role="option"
					aria-selected={$selectedAppName === app.name}
					class="flex w-full flex-col gap-1 px-4 py-3 text-left hover:bg-slate-700"
					class:bg-slate-700={$selectedAppName === app.name}
					on:click={() => handleSelect(app.name)}
				>
					<div class="flex items-center gap-2">
						<span class="font-medium text-white">{app.name}</span>
						{#if app.has_config}
							<span class="rounded bg-primary-600 px-1.5 py-0.5 text-xs text-white">Configurable</span>
						{:else}
							<span class="rounded bg-slate-600 px-1.5 py-0.5 text-xs text-slate-300">Build only</span>
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
