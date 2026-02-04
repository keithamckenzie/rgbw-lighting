<script lang="ts">
	import { goto } from '$app/navigation';
	import { monorepoInfo, selectedApp } from '$lib/stores/apps';
	import AppSelector from '$lib/components/common/AppSelector.svelte';

	function navigateToApp(appName: string, envName: string) {
		goto(`/app/${appName}?env=${encodeURIComponent(envName)}`);
	}
</script>

<div class="h-full overflow-auto p-6">
	<div class="mx-auto max-w-4xl">
		<h2 class="mb-6 text-2xl font-bold text-white">Select Application</h2>

		<div class="mb-8">
			<AppSelector />
		</div>

		{#if $selectedApp}
			<div class="rounded-lg border border-slate-700 bg-slate-800">
				<div class="border-b border-slate-700 p-4">
					<h3 class="text-lg font-medium text-white">{$selectedApp.name}</h3>
					<p class="mt-1 text-sm text-slate-400">{$selectedApp.path}</p>
					{#if $selectedApp.has_config}
						<span class="mt-2 inline-block rounded bg-primary-600 px-2 py-0.5 text-xs text-white">
							Configurable
						</span>
					{:else}
						<span class="mt-2 inline-block rounded bg-slate-600 px-2 py-0.5 text-xs text-slate-300">
							Build only (no config.h)
						</span>
					{/if}
				</div>

				<div class="p-4">
					<h4 class="mb-3 text-sm font-medium text-slate-300">Available Environments</h4>
					<div class="grid gap-3 sm:grid-cols-2">
						{#each $selectedApp.environments as env}
							<button
								type="button"
								class="flex flex-col items-start rounded-lg border border-slate-600 bg-slate-700 p-4 text-left transition-colors hover:border-primary-500 hover:bg-slate-600"
								on:click={() => navigateToApp($selectedApp.name, env.name)}
							>
								<div class="flex items-center gap-2">
									<span class="font-medium text-white">{env.name}</span>
									{#if !env.is_hardware_target}
										<span class="rounded bg-slate-600 px-1.5 py-0.5 text-xs text-slate-300">
											Tests only
										</span>
									{/if}
								</div>

								{#if env.board}
									<span class="mt-1 text-sm text-slate-400">Board: {env.board}</span>
								{/if}

								<div class="mt-2 flex flex-wrap gap-1">
									{#if env.platform}
										<span class="rounded bg-slate-600 px-1.5 py-0.5 text-xs text-slate-300">
											{env.platform}
										</span>
									{/if}
									{#if env.framework}
										<span class="rounded bg-slate-600 px-1.5 py-0.5 text-xs text-slate-300">
											{env.framework}
										</span>
									{/if}
								</div>

								<div class="mt-3 text-sm text-primary-400">
									{env.can_upload ? 'Configure & Build' : 'Build & Test'}
									<span class="ml-1">&rarr;</span>
								</div>
							</button>
						{/each}
					</div>
				</div>
			</div>
		{:else if $monorepoInfo && $monorepoInfo.apps.length === 0}
			<div class="rounded-lg border border-slate-700 bg-slate-800 p-8 text-center">
				<p class="text-slate-400">No PlatformIO apps found in the apps/ directory.</p>
				<p class="mt-2 text-sm text-slate-500">
					Create a new app by copying apps/example-rgbw/ to apps/your-app-name/
				</p>
			</div>
		{/if}

		{#if $monorepoInfo}
			<div class="mt-8 rounded-lg border border-slate-700 bg-slate-800 p-4">
				<h4 class="mb-2 text-sm font-medium text-slate-300">PlatformIO</h4>
				<p class="text-sm text-slate-400">{$monorepoInfo.pio_path}</p>
			</div>
		{/if}
	</div>
</div>
