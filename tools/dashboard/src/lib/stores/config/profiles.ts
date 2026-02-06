import { writable, get } from 'svelte/store';
import type { SavedProfile } from '$lib/types/config';
import { invokeWithTimeout } from '$lib/utils/invoke';
import { pushToast } from '$lib/stores/toast';
import { configState, resetDirty } from './state';

export const availableProfiles = writable<string[]>([]);
export const profileLoading = writable(false);

const PROFILE_TIMEOUT_MS = 10000;

// Profile actions throw so the UI can decide how to notify and handle state.
export async function loadProfiles(appName: string): Promise<void> {
  try {
    const profiles = await invokeWithTimeout<string[]>(
      'list_profiles',
      { appName },
      PROFILE_TIMEOUT_MS
    );
    availableProfiles.set(profiles);
  } catch (error) {
    console.error('Failed to load profiles:', error);
    pushToast('Failed to load profiles', 'error');
    availableProfiles.set([]);
  }
}

export async function saveProfile(name: string): Promise<void> {
  const state = get(configState);
  const now = new Date().toISOString();

  const profile: SavedProfile = {
    name,
    app_name: state.appName,
    environment: state.environment,
    defines: state.defines,
    created_at: now,
    updated_at: now
  };

  try {
    await invokeWithTimeout('save_profile', { profile }, PROFILE_TIMEOUT_MS);
    // loadProfiles handles its own errors; guard against unexpected throws.
    try {
      await loadProfiles(state.appName);
    } catch {
      // ignore
    }
    resetDirty();
  } catch (error) {
    throw new Error(`Failed to save profile: ${String(error)}`);
  }
}

export async function loadProfile(appName: string, profileName: string): Promise<SavedProfile> {
  try {
    const profile = await invokeWithTimeout<SavedProfile>(
      'load_profile',
      { appName, profileName },
      PROFILE_TIMEOUT_MS
    );
    return profile;
  } catch (error) {
    throw new Error(`Failed to load profile: ${String(error)}`);
  }
}

export async function deleteProfile(appName: string, profileName: string): Promise<void> {
  try {
    await invokeWithTimeout('delete_profile', { appName, profileName }, PROFILE_TIMEOUT_MS);
    // loadProfiles handles its own errors; guard against unexpected throws.
    try {
      await loadProfiles(appName);
    } catch {
      // ignore
    }
  } catch (error) {
    throw new Error(`Failed to delete profile: ${String(error)}`);
  }
}
