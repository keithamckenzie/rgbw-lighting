import { writable } from 'svelte/store';
import type { BuildEvent } from '$lib/types/config';
import { invokeWithTimeout } from '$lib/utils/invoke';

// Store mutations are funneled through exported action functions to centralize side effects.
const MAX_LOG_LINES = 5000;
const BUILD_TIMEOUT_MS = 10 * 60 * 1000;
const CLEAN_TIMEOUT_MS = 5 * 60 * 1000;

export interface BuildState {
  isBuilding: boolean;
  appName: string | null;
  environment: string | null;
  success: boolean | null;
  durationMs: number | null;
  lines: string[];
}

export const buildState = writable<BuildState>({
  isBuilding: false,
  appName: null,
  environment: null,
  success: null,
  durationMs: null,
  lines: []
});

// Actions
export function resetBuildState(): void {
  buildState.update((state) => ({
    ...state,
    isBuilding: false,
    success: null,
    durationMs: null,
    lines: []
  }));
}

export function clearBuildLog(): void {
  buildState.update((state) => ({
    ...state,
    lines: []
  }));
}

export function clearBuildStatus(): void {
  buildState.update((state) => ({
    ...state,
    success: null,
    durationMs: null
  }));
}

export function addBuildLine(line: string): void {
  buildState.update((state) => {
    const newLines = [...state.lines, line];
    // Keep only last MAX_LOG_LINES
    if (newLines.length > MAX_LOG_LINES) {
      return { ...state, lines: newLines.slice(-MAX_LOG_LINES) };
    }
    return { ...state, lines: newLines };
  });
}

export function handleBuildEvent(event: BuildEvent): void {
  switch (event.type) {
    case 'started':
      buildState.update((state) => ({
        ...state,
        isBuilding: true,
        appName: event.app_name ?? null,
        environment: event.environment ?? null,
        success: null,
        durationMs: null,
        lines: []
      }));
      break;

    case 'output':
      if (event.line) {
        addBuildLine(event.line);
      }
      break;

    case 'error':
      if (event.message) {
        addBuildLine(`ERROR: ${event.message}`);
      }
      break;

    case 'complete':
      buildState.update((state) => ({
        ...state,
        isBuilding: false,
        success: event.success ?? false,
        durationMs: event.duration_ms ?? null
      }));
      break;
  }
}

// Build/Upload operations
// Build failures are surfaced in the output panel (not toasts) to keep all logs in one place.
export async function runBuild(
  appName: string,
  environment: string,
  buildFlags: string[]
): Promise<boolean> {
  try {
    const success = await invokeWithTimeout<boolean>(
      'run_build',
      {
        appName,
        environment,
        buildFlags
      },
      BUILD_TIMEOUT_MS
    );
    return success;
  } catch (error) {
    addBuildLine(`Build failed: ${error}`);
    buildState.update((state) => ({ ...state, isBuilding: false, success: false }));
    return false;
  }
}

export async function runUpload(
  appName: string,
  environment: string,
  buildFlags: string[],
  uploadPort?: string
): Promise<boolean> {
  try {
    const success = await invokeWithTimeout<boolean>(
      'run_upload',
      {
        appName,
        environment,
        buildFlags,
        uploadPort: uploadPort ?? null
      },
      BUILD_TIMEOUT_MS
    );
    return success;
  } catch (error) {
    addBuildLine(`Upload failed: ${error}`);
    buildState.update((state) => ({ ...state, isBuilding: false, success: false }));
    return false;
  }
}

export async function runTests(appName: string, environment: string): Promise<boolean> {
  try {
    const success = await invokeWithTimeout<boolean>(
      'run_tests',
      {
        appName,
        environment
      },
      BUILD_TIMEOUT_MS
    );
    return success;
  } catch (error) {
    addBuildLine(`Tests failed: ${error}`);
    buildState.update((state) => ({ ...state, isBuilding: false, success: false }));
    return false;
  }
}

export async function cleanBuild(appName: string, environment?: string): Promise<boolean> {
  buildState.update((state) => ({
    ...state,
    isBuilding: true
  }));
  try {
    const success = await invokeWithTimeout<boolean>(
      'clean_build',
      {
        appName,
        environment: environment ?? null
      },
      CLEAN_TIMEOUT_MS
    );
    return success;
  } catch (error) {
    addBuildLine(`Clean failed: ${error}`);
    return false;
  } finally {
    buildState.update((state) => ({ ...state, isBuilding: false }));
  }
}
