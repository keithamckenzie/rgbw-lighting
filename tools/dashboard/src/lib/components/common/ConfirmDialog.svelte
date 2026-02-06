<script lang="ts">
  import { tick } from 'svelte';

  let {
    open = false,
    title = 'Confirm',
    message = 'Are you sure?',
    confirmLabel = 'Confirm',
    cancelLabel = 'Cancel',
    onConfirm,
    onCancel
  }: {
    open?: boolean;
    title?: string;
    message?: string;
    confirmLabel?: string;
    cancelLabel?: string;
    onConfirm: () => void;
    onCancel: () => void;
  } = $props();

  let dialogEl: HTMLDivElement | null = $state(null);
  let cancelButton: HTMLButtonElement | null = $state(null);
  let previouslyFocused: Element | null = null;
  // Math.random is sufficient here; IDs are only used for aria attributes, not security.
  const titleId = `confirm-dialog-title-${Math.random().toString(36).slice(2)}`;
  const messageId = `confirm-dialog-message-${Math.random().toString(36).slice(2)}`;

  function handleConfirm() {
    onConfirm();
  }

  function handleCancel() {
    onCancel();
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

  // Focus dialog when opened
  $effect(() => {
    if (open) {
      void focusDialog();
    }
  });

  // Restore focus on cleanup
  $effect(() => {
    return () => {
      if (previouslyFocused instanceof HTMLElement) {
        try {
          previouslyFocused.focus?.();
        } catch {
          // Ignore focus failures if the element was detached.
        }
      }
    };
  });
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
      class="card w-full max-w-md p-6 shadow-lg"
      onkeydown={handleKeyDown}
    >
      <h3 id={titleId} class="text-lg font-semibold text-white">{title}</h3>
      <p id={messageId} class="mt-2 text-sm text-zinc-300">{message}</p>
      <div class="mt-6 flex justify-end gap-2">
        <button
          type="button"
          class="btn btn-secondary"
          onclick={handleCancel}
          bind:this={cancelButton}
        >
          {cancelLabel}
        </button>
        <button
          type="button"
          class="btn btn-danger"
          onclick={handleConfirm}
        >
          {confirmLabel}
        </button>
      </div>
    </div>
  </div>
{/if}
