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

  let { appName, environments = [] }: { appName: string; environments?: DiscoveredEnvironment[] } =
    $props();

  let newProfileName = $state('');
  let showSaveDialog = $state(false);
  let isSaving = $state(false);
  let isLoading = $state(false);
  let lastAppName = $state('');
  let pendingDelete: string | null = $state(null);

  const nameError = $derived(
    newProfileName.trim() ? validateProfileName(newProfileName) : null
  );

  $effect(() => {
    if (appName && appName !== lastAppName) {
      lastAppName = appName;
      loadProfiles(appName);
    }
  });

  function validateProfileName(name: string): string | null {
    const trimmed = name.trim();
    if (!trimmed) return 'Profile name cannot be empty';
    if (trimmed.length > 50) return 'Profile name too long (max 50 characters)';
    if (!/^[a-zA-Z0-9 _.-]+$/.test(trimmed)) {
      return 'Profile name can only use letters, numbers, spaces, ., _, and -';
    }
    if (
      trimmed.includes('..') ||
      trimmed.includes('/') ||
      trimmed.includes('\\') ||
      trimmed.includes('\0')
    ) {
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
      pushToast('Profile saved', 'success');
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
        keepFocus: true,
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

<div class="card card-bordered p-4" aria-busy={isLoading || isSaving}>
  <div class="mb-3 flex items-center justify-between">
    <h3 class="font-medium text-white">Configuration Profiles</h3>
    <button
      type="button"
      class="btn btn-primary"
      onclick={() => (showSaveDialog = true)}
    >
      Save As...
    </button>
  </div>

  {#if showSaveDialog}
    <div class="mb-4 flex items-center gap-2 rounded-md border border-zinc-700 bg-zinc-800 p-3">
      <input
        type="text"
        bind:value={newProfileName}
        placeholder="Profile name..."
        class="input-base flex-1"
        class:input-error={!!nameError}
        aria-invalid={!!nameError}
        aria-describedby={nameError ? 'profile-name-error' : undefined}
        onkeydown={(e) => e.key === 'Enter' && handleSave()}
      />
      <button
        type="button"
        class="btn btn-success"
        disabled={isSaving || !newProfileName.trim() || !!nameError}
        onclick={handleSave}
      >
        {isSaving ? 'Saving...' : 'Save'}
      </button>
      <button
        type="button"
        class="btn btn-secondary"
        onclick={() => {
          showSaveDialog = false;
          newProfileName = '';
        }}
      >
        Cancel
      </button>
    </div>
    {#if nameError}
      <p id="profile-name-error" class="mb-4 text-sm text-yellow-400">{nameError}</p>
    {/if}
  {/if}

  {#if $availableProfiles.length === 0}
    <div class="text-center py-3" role="status">
      <p class="text-sm text-zinc-400">No saved profiles yet.</p>
      <p class="mt-1 text-xs text-zinc-500">Click "Save As..." to save your current configuration.</p>
    </div>
  {:else}
    <div class="space-y-2">
      {#each $availableProfiles as profile}
        <div
          class="flex items-center justify-between rounded-md border border-zinc-700 bg-zinc-800 px-3 py-2"
        >
          <span class="text-zinc-200">{profile}</span>
          <div class="flex items-center gap-2">
            <button
              type="button"
              class="btn btn-primary"
              disabled={isLoading}
              onclick={() => handleLoad(profile)}
            >
              Load
            </button>
            <button
              type="button"
              class="btn btn-secondary text-red-400 hover:bg-red-600 hover:text-white hover:border-red-600"
              onclick={() => (pendingDelete = profile)}
            >
              Delete
            </button>
          </div>
        </div>
      {/each}
    </div>
  {/if}

  {#if isLoading}
    <div class="mt-3 flex items-center gap-2 text-sm text-zinc-400">
      <span class="h-3 w-3 animate-spin rounded-full border-2 border-zinc-500 border-t-transparent"
      ></span>
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
  onConfirm={() => {
    if (pendingDelete) {
      handleDelete(pendingDelete);
    }
    pendingDelete = null;
  }}
  onCancel={() => (pendingDelete = null)}
/>
