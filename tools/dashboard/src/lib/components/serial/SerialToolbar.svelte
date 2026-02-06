<script lang="ts">
  import type { PortInfo } from '$lib/types/config';
  import { VALID_BAUD_RATES, type SerialConnection } from '$lib/stores/serial';

  let {
    connectionList = [],
    activeConnection = null,
    selectedPort = $bindable(''),
    baudRate = $bindable(115200),
    availablePorts = [],
    isConnecting = false,
    isDisconnecting = false,
    isInitialized = true,
    onConnect,
    onDisconnect,
    onRefreshPorts,
    onClear,
    onPortChange,
    onBaudChange,
    onConnectionChange
  }: {
    connectionList?: SerialConnection[];
    activeConnection?: SerialConnection | null;
    selectedPort?: string;
    baudRate?: number;
    availablePorts?: PortInfo[];
    isConnecting?: boolean;
    isDisconnecting?: boolean;
    isInitialized?: boolean;
    onConnect: () => void;
    onDisconnect: () => void;
    onRefreshPorts: () => void;
    onClear: () => void;
    onPortChange: (value: string) => void;
    onBaudChange: (value: number) => void;
    onConnectionChange: (value: string) => void;
  } = $props();

  const baudRates = VALID_BAUD_RATES;
</script>

<div class="flex flex-wrap items-center gap-2 border-b border-zinc-800 bg-zinc-900 p-3">
  {#if connectionList.length === 0}
    <select
      bind:value={selectedPort}
      class="input-base w-auto"
      disabled={!isInitialized}
      aria-label="Serial port"
      onchange={(e) => {
        const target = e.target;
        if (!(target instanceof HTMLSelectElement)) return;
        onPortChange(target.value);
      }}
    >
      <option value="">Select port...</option>
      {#each availablePorts as port}
        <option value={port.path}>
          {port.path}
          {#if port.product}({port.product}){/if}
        </option>
      {/each}
    </select>

    <select
      bind:value={baudRate}
      class="input-base w-auto"
      disabled={!isInitialized}
      aria-label="Baud rate"
      onchange={(e) => {
        const target = e.target;
        if (!(target instanceof HTMLSelectElement)) return;
        onBaudChange(Number(target.value));
      }}
    >
      {#each baudRates as rate}
        <option value={rate}>{rate}</option>
      {/each}
    </select>

    <button
      type="button"
      class="btn btn-success"
      disabled={!isInitialized || !selectedPort || isConnecting}
      onclick={onConnect}
    >
      {isConnecting ? 'Connecting...' : 'Connect'}
    </button>

    <button
      type="button"
      class="btn btn-secondary px-2"
      title="Refresh ports"
      aria-label="Refresh ports"
      disabled={!isInitialized}
      onclick={onRefreshPorts}
    >
      <svg class="h-5 w-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
        <path
          stroke-linecap="round"
          stroke-linejoin="round"
          stroke-width="2"
          d="M4 4v5h.582m15.356 2A8.001 8.001 0 004.582 9m0 0H9m11 11v-5h-.581m0 0a8.003 8.003 0 01-15.357-2m15.357 2H15"
        />
      </svg>
    </button>
  {:else}
    <div class="flex items-center gap-2">
      <span class="flex items-center gap-2 text-green-400" role="status" aria-live="polite">
        <span class="h-2 w-2 rounded-full bg-green-400" aria-hidden="true"></span>
        Connected
      </span>

      {#if connectionList.length > 1}
        <select
          value={activeConnection?.connectionId}
          aria-label="Active connection"
          onchange={(e) => {
            const target = e.target;
            if (!(target instanceof HTMLSelectElement)) return;
            onConnectionChange(target.value);
          }}
          class="input-base w-auto py-1 text-sm"
        >
          {#each connectionList as conn}
            <option value={conn.connectionId}>{conn.portPath}</option>
          {/each}
        </select>
      {:else if activeConnection}
        <span class="text-zinc-300">{activeConnection.portPath} @ {activeConnection.baudRate}</span
        >
      {/if}
    </div>

    <button
      type="button"
      class="btn btn-danger"
      disabled={!isInitialized || isDisconnecting}
      onclick={onDisconnect}
    >
      {isDisconnecting ? 'Disconnecting...' : 'Disconnect'}
    </button>

    <button
      type="button"
      class="btn btn-secondary px-2"
      title="Refresh ports"
      aria-label="Refresh ports"
      disabled={!isInitialized}
      onclick={onRefreshPorts}
    >
      <svg class="h-5 w-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
        <path
          stroke-linecap="round"
          stroke-linejoin="round"
          stroke-width="2"
          d="M4 4v5h.582m15.356 2A8.001 8.001 0 004.582 9m0 0H9m11 11v-5h-.581m0 0a8.003 8.003 0 01-15.357-2m15.357 2H15"
        />
      </svg>
    </button>

    <div class="flex-1"></div>

    <button
      type="button"
      class="btn btn-secondary"
      onclick={onClear}
    >
      Clear
    </button>
  {/if}
</div>
