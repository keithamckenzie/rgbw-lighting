<script lang="ts">
	export let inputText = '';
	export let isSending = false;
	export let disabled = false;
	export let onSend: () => void;
	export let onKeyDown: (event: KeyboardEvent) => void;
</script>

<div class="flex items-center gap-2 border-t border-slate-700 bg-slate-800 p-3">
	<input
		type="text"
		bind:value={inputText}
		on:keydown={onKeyDown}
		placeholder="Type a command and press Enter..."
		aria-label="Serial command input"
		disabled={disabled}
		class="flex-1 rounded border border-slate-600 bg-slate-700 px-3 py-2 text-white placeholder-slate-400 focus:outline-none focus:ring-2 focus:ring-primary-500"
	/>
	<button
		type="button"
		class="rounded bg-primary-600 px-4 py-2 font-medium text-white hover:bg-primary-500 disabled:opacity-60"
		disabled={disabled || isSending || !inputText}
		on:click={onSend}
	>
		{isSending ? 'Sending...' : 'Send'}
	</button>
</div>
