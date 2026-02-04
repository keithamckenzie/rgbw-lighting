<script lang="ts">
	import { onDestroy } from 'svelte';
	import { toasts, clearToastTimeouts } from '$lib/stores/toast';

	function kindClasses(kind: string) {
		switch (kind) {
			case 'success':
				return 'border-green-600 bg-green-900 text-green-100';
			case 'error':
				return 'border-red-600 bg-red-900 text-red-100';
			case 'warning':
				return 'border-yellow-600 bg-yellow-900 text-yellow-100';
			default:
				return 'border-slate-600 bg-slate-800 text-slate-100';
		}
	}

	onDestroy(() => {
		clearToastTimeouts();
	});
</script>

<div class="pointer-events-none fixed right-4 top-4 z-50 space-y-2" aria-live="polite">
	{#each $toasts as toast (toast.id)}
		<div
			class={`pointer-events-auto rounded border px-4 py-2 shadow-lg ${kindClasses(toast.kind)}`}
		>
			{toast.message}
		</div>
	{/each}
</div>
