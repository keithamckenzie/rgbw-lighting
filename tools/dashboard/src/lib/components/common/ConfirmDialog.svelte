<script lang="ts">
	import { createEventDispatcher, onDestroy, tick } from 'svelte';

	export let open = false;
	export let title = 'Confirm';
	export let message = 'Are you sure?';
	export let confirmLabel = 'Confirm';
	export let cancelLabel = 'Cancel';

	const dispatch = createEventDispatcher<{ confirm: void; cancel: void }>();
	let dialogEl: HTMLDivElement | null = null;
	let cancelButton: HTMLButtonElement | null = null;
	let previouslyFocused: Element | null = null;
	// Math.random is sufficient here; IDs are only used for aria attributes, not security.
	const titleId = `confirm-dialog-title-${Math.random().toString(36).slice(2)}`;
	const messageId = `confirm-dialog-message-${Math.random().toString(36).slice(2)}`;

	function handleConfirm() {
		dispatch('confirm');
	}

	function handleCancel() {
		dispatch('cancel');
	}

	async function focusDialog() {
		if (!open) return;
		previouslyFocused = document.activeElement;
		await tick();
		cancelButton?.focus();
	}

	function handleKeyDown(event: KeyboardEvent) {
		if (event.key === 'Escape') {
			event.stopPropagation();
			handleCancel();
			return;
		}
		if (event.key !== 'Tab' || !dialogEl) return;
		const focusable = Array.from(
			dialogEl.querySelectorAll<HTMLElement>(
				'button, [href], input, select, textarea, [tabindex]:not([tabindex="-1"])'
			)
		).filter((el) => !el.hasAttribute('disabled'));

		if (focusable.length === 0) return;
		const first = focusable[0];
		const last = focusable[focusable.length - 1];
		const active = document.activeElement;

		if (event.shiftKey && active === first) {
			event.preventDefault();
			last.focus();
		} else if (!event.shiftKey && active === last) {
			event.preventDefault();
			first.focus();
		}
	}

	onDestroy(() => {
		if (previouslyFocused instanceof HTMLElement) {
			try {
				previouslyFocused.focus?.();
			} catch {
				// Ignore focus failures if the element was detached.
			}
		}
	});

	$: if (open) {
		void focusDialog();
	}
</script>

{#if open}
	<div class="fixed inset-0 z-50 flex items-center justify-center bg-black/50 p-4">
		<div
			bind:this={dialogEl}
			role="alertdialog"
			aria-modal="true"
			aria-labelledby={titleId}
			aria-describedby={messageId}
			tabindex="-1"
			class="w-full max-w-md rounded-lg border border-slate-700 bg-slate-900 p-6 shadow-lg"
			on:keydown={handleKeyDown}
		>
			<h3 id={titleId} class="text-lg font-semibold text-white">{title}</h3>
			<p id={messageId} class="mt-2 text-sm text-slate-300">{message}</p>
			<div class="mt-6 flex justify-end gap-2">
				<button
					type="button"
					class="rounded border border-slate-600 px-4 py-2 text-slate-200 hover:bg-slate-800 focus:outline-none focus:ring-2 focus:ring-primary-500"
					on:click={handleCancel}
					bind:this={cancelButton}
				>
					{cancelLabel}
				</button>
				<button
					type="button"
					class="rounded bg-red-600 px-4 py-2 font-medium text-white hover:bg-red-500 focus:outline-none focus:ring-2 focus:ring-primary-500"
					on:click={handleConfirm}
				>
					{confirmLabel}
				</button>
			</div>
		</div>
	</div>
{/if}
