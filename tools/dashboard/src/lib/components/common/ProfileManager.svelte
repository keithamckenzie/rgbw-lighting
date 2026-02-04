<script lang="ts">
	import { goto } from '$app/navigation';
	import {
		configState,
		availableProfiles,
		loadProfiles,
		saveProfile,
		loadProfile,
		applyProfile,
		deleteProfile,
		profileLoading
	} from '$lib/stores/config';
	import type { DiscoveredEnvironment } from '$lib/types/config';
	import { pushToast } from '$lib/stores/toast';
	import ConfirmDialog from './ConfirmDialog.svelte';

	export let appName: string;
	export let environments: DiscoveredEnvironment[] = [];

	let newProfileName: string = '';
	let showSaveDialog = false;
	let isSaving = false;
	let isLoading = false;
	let lastAppName = '';
	let nameError: string | null = null;
	let pendingDelete: string | null = null;

	$: {
		if (appName && appName !== lastAppName) {
			lastAppName = appName;
			loadProfiles(appName);
		}
	}

	$: {
		nameError = newProfileName.trim() ? validateProfileName(newProfileName) : null;
	}

	function validateProfileName(name: string): string | null {
		const trimmed = name.trim();
		if (!trimmed) return 'Profile name cannot be empty';
		if (trimmed.length > 50) return 'Profile name too long (max 50 characters)';
		if (!/^[a-zA-Z0-9 _.-]+$/.test(trimmed)) {
			return 'Profile name can only use letters, numbers, spaces, ., _, and -';
		}
		if (trimmed.includes('..') || trimmed.includes('/') || trimmed.includes('\\') || trimmed.includes('\0')) {
			return 'Profile name cannot contain path characters';
		}
		return null;
	}

	async function handleSave() {
		if (!newProfileName.trim()) {
			pushToast('Profile name cannot be empty', 'warning');
			return;
		}
		if (nameError) {
			pushToast(nameError, 'warning');
			return;
		}

		isSaving = true;
		try {
			await saveProfile(newProfileName.trim());
			newProfileName = '';
			showSaveDialog = false;
		} catch (e) {
			pushToast(`Failed to save profile: ${String(e)}`, 'error');
		} finally {
			isSaving = false;
		}
	}

	async function handleLoad(profileName: string) {
		isLoading = true;
		profileLoading.set(true);
		try {
			const profile = await loadProfile(appName, profileName);
			const envExists = environments.some((env) => env.name === profile.environment);

			if (!envExists) {
				pushToast(
					`Profile "${profileName}" targets missing environment "${profile.environment}".`,
					'warning'
				);
				return;
			}

			await goto(`/app/${appName}?env=${encodeURIComponent(profile.environment)}`, {
				replaceState: true,
				keepfocus: true,
				noScroll: true
			});

			// Apply after navigation so initConfig doesn't override the loaded profile.
			applyProfile(profile);
		} catch (e) {
			pushToast(`Failed to load profile: ${String(e)}`, 'error');
		} finally {
			isLoading = false;
			profileLoading.set(false);
		}
	}

	async function handleDelete(profileName: string) {
		try {
			await deleteProfile(appName, profileName);
		} catch (e) {
			pushToast(`Failed to delete profile: ${String(e)}`, 'error');
		}
	}
</script>

	<div class="rounded-lg border border-slate-700 bg-slate-800 p-4" aria-busy={isLoading || isSaving}>
	<div class="mb-3 flex items-center justify-between">
		<h3 class="font-medium text-white">Configuration Profiles</h3>
		<button
			type="button"
			class="rounded bg-primary-600 px-3 py-1 text-sm font-medium text-white hover:bg-primary-500"
			on:click={() => (showSaveDialog = true)}
		>
			Save As...
		</button>
	</div>

	{#if showSaveDialog}
		<div class="mb-4 flex items-center gap-2 rounded border border-slate-600 bg-slate-700 p-3">
			<input
				type="text"
				bind:value={newProfileName}
				placeholder="Profile name..."
				class="flex-1 rounded border border-slate-500 bg-slate-600 px-3 py-2 text-white placeholder-slate-400 focus:outline-none focus:ring-2 focus:ring-primary-500"
				class:border-red-500={!!nameError}
				on:keydown={(e) => e.key === 'Enter' && handleSave()}
			/>
			<button
				type="button"
				class="rounded bg-green-600 px-3 py-2 text-sm font-medium text-white hover:bg-green-500 disabled:opacity-50"
				disabled={isSaving || !newProfileName.trim() || !!nameError}
				on:click={handleSave}
			>
				{isSaving ? 'Saving...' : 'Save'}
			</button>
			<button
				type="button"
				class="rounded border border-slate-500 px-3 py-2 text-sm text-slate-300 hover:bg-slate-600"
				on:click={() => {
					showSaveDialog = false;
					newProfileName = '';
				}}
			>
				Cancel
			</button>
		</div>
		{#if nameError}
			<p class="mb-4 text-sm text-yellow-400">{nameError}</p>
		{/if}
	{/if}

	{#if $availableProfiles.length === 0}
		<p class="text-sm text-slate-400">No saved profiles yet.</p>
	{:else}
		<div class="space-y-2">
			{#each $availableProfiles as profile}
				<div
					class="flex items-center justify-between rounded border border-slate-600 bg-slate-700 px-3 py-2"
				>
					<span class="text-slate-200">{profile}</span>
					<div class="flex items-center gap-2">
						<button
							type="button"
							class="rounded bg-primary-600 px-3 py-1 text-sm font-medium text-white hover:bg-primary-500 disabled:opacity-50"
							disabled={isLoading}
							on:click={() => handleLoad(profile)}
						>
							Load
						</button>
						<button
							type="button"
							class="rounded border border-red-600 px-3 py-1 text-sm font-medium text-red-400 hover:bg-red-600 hover:text-white"
							on:click={() => (pendingDelete = profile)}
						>
							Delete
						</button>
					</div>
				</div>
			{/each}
		</div>
	{/if}

	{#if isLoading}
		<div class="mt-3 flex items-center gap-2 text-sm text-slate-400">
			<span class="h-3 w-3 animate-spin rounded-full border-2 border-slate-500 border-t-transparent"></span>
			<span>Loading profile...</span>
		</div>
	{/if}

	{#if $configState.isDirty}
		<p class="mt-3 text-sm text-yellow-400">
			Unsaved changes. Save a profile to keep these settings.
		</p>
	{/if}
</div>

<ConfirmDialog
	open={pendingDelete !== null}
	title="Delete profile?"
	message={`This will permanently delete "${pendingDelete ?? ''}".`}
	confirmLabel="Delete"
	cancelLabel="Cancel"
	on:confirm={() => {
		if (pendingDelete) {
			handleDelete(pendingDelete);
		}
		pendingDelete = null;
	}}
	on:cancel={() => (pendingDelete = null)}
/>
