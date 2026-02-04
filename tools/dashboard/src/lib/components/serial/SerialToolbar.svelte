<script lang="ts">
	import type { PortInfo } from '$lib/types/config';
	import { VALID_BAUD_RATES, type SerialConnection } from '$lib/stores/serial';

	export let connectionList: SerialConnection[] = [];
	export let activeConnection: SerialConnection | null = null;
	export let selectedPort = '';
	export let baudRate = 115200;
	export let availablePorts: PortInfo[] = [];
	export let isConnecting = false;
	export let isDisconnecting = false;
	export let isInitialized = true;

	export let onConnect: () => void;
	export let onDisconnect: () => void;
	export let onRefreshPorts: () => void;
	export let onClear: () => void;
	export let onPortChange: (value: string) => void;
	export let onBaudChange: (value: number) => void;
	export let onConnectionChange: (value: string) => void;

	const baudRates = VALID_BAUD_RATES;
</script>

<div class="flex flex-wrap items-center gap-2 border-b border-slate-700 bg-slate-800 p-3">
	{#if connectionList.length === 0}
		<select
			bind:value={selectedPort}
			class="rounded border border-slate-600 bg-slate-700 px-3 py-2 text-white"
			disabled={!isInitialized}
			aria-label="Serial port"
			on:change={(e) => {
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
			class="rounded border border-slate-600 bg-slate-700 px-3 py-2 text-white"
			disabled={!isInitialized}
			aria-label="Baud rate"
			on:change={(e) => {
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
			class="rounded bg-green-600 px-4 py-2 font-medium text-white hover:bg-green-500 disabled:opacity-50"
			disabled={!isInitialized || !selectedPort || isConnecting}
			on:click={onConnect}
		>
			{isConnecting ? 'Connecting...' : 'Connect'}
		</button>

		<button
			type="button"
			class="rounded border border-slate-600 px-2 py-2 text-slate-300 hover:bg-slate-700 disabled:opacity-60"
			title="Refresh ports"
			aria-label="Refresh ports"
			disabled={!isInitialized}
			on:click={onRefreshPorts}
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
			<span class="flex items-center gap-2 text-green-400">
				<span class="h-2 w-2 rounded-full bg-green-400" aria-hidden="true"></span>
				Connected
			</span>

			{#if connectionList.length > 1}
				<select
					value={activeConnection?.connectionId}
					aria-label="Active connection"
					on:change={(e) => {
						const target = e.target;
						if (!(target instanceof HTMLSelectElement)) return;
						onConnectionChange(target.value);
					}}
					class="rounded border border-slate-600 bg-slate-700 px-2 py-1 text-sm text-white"
				>
					{#each connectionList as conn}
						<option value={conn.connectionId}>{conn.portPath}</option>
					{/each}
				</select>
			{:else if activeConnection}
				<span class="text-slate-300">{activeConnection.portPath} @ {activeConnection.baudRate}</span>
			{/if}
		</div>

		<button
			type="button"
			class="rounded bg-red-600 px-4 py-2 font-medium text-white hover:bg-red-500 disabled:opacity-60"
			disabled={!isInitialized || isDisconnecting}
			on:click={onDisconnect}
		>
			{isDisconnecting ? 'Disconnecting...' : 'Disconnect'}
		</button>

		<button
			type="button"
			class="rounded border border-slate-600 px-2 py-2 text-slate-300 hover:bg-slate-700 disabled:opacity-60"
			title="Refresh ports"
			aria-label="Refresh ports"
			disabled={!isInitialized}
			on:click={onRefreshPorts}
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
			class="rounded border border-slate-600 px-4 py-2 text-slate-300 hover:bg-slate-700"
			on:click={onClear}
		>
			Clear
		</button>
	{/if}
</div>
