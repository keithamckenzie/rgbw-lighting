<script lang="ts">
	import '../app.css';
	import { onMount } from 'svelte';
	import { page } from '$app/stores';
	import { loadApps, monorepoInfo, monorepoError, isLoading } from '$lib/stores/apps';
	import ToastHost from '$lib/components/common/ToastHost.svelte';

	onMount(async () => {
		await loadApps();
	});

	$: currentPath = $page.url.pathname;
</script>

<div class="flex h-screen flex-col bg-slate-900 text-white">
	<!-- Header -->
	<header class="flex items-center justify-between border-b border-slate-700 bg-slate-800 px-4 py-3">
		<div class="flex items-center gap-4">
			<h1 class="text-xl font-bold text-white">RGBW Dashboard</h1>
			{#if $monorepoInfo}
				<span class="text-sm text-slate-400">{$monorepoInfo.path}</span>
			{/if}
		</div>

		<nav class="flex items-center gap-2">
			<a
				href="/"
				class="rounded px-3 py-2 text-sm font-medium transition-colors"
				class:bg-primary-600={currentPath === '/'}
				class:text-white={currentPath === '/'}
				class:text-slate-300={currentPath !== '/'}
				class:hover:bg-slate-700={currentPath !== '/'}
			>
				Apps
			</a>
			<a
				href="/serial"
				class="rounded px-3 py-2 text-sm font-medium transition-colors"
				class:bg-primary-600={currentPath === '/serial'}
				class:text-white={currentPath === '/serial'}
				class:text-slate-300={currentPath !== '/serial'}
				class:hover:bg-slate-700={currentPath !== '/serial'}
			>
				Serial Monitor
			</a>
			<button
				type="button"
				class="rounded px-3 py-2 text-sm font-medium transition-colors text-slate-300 hover:bg-slate-700 disabled:cursor-not-allowed disabled:opacity-60"
				disabled={$isLoading}
				on:click={() => loadApps()}
				title="Rescan apps and config"
			>
				{$isLoading ? 'Rescanning...' : 'Rescan'}
			</button>
		</nav>
	</header>

	<!-- Main content -->
	<main class="flex-1 overflow-hidden">
		{#if $isLoading}
			<div class="flex h-full items-center justify-center">
				<div class="text-center">
					<div class="mb-4 h-12 w-12 animate-spin rounded-full border-4 border-primary-500 border-t-transparent mx-auto"></div>
					<p class="text-slate-400">Loading project...</p>
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
			<slot />
		{/if}
	</main>
</div>

<ToastHost />
