<script lang="ts">
  import { fly } from 'svelte/transition';
  import { toasts, removeToast, clearToastTimeouts } from '$lib/stores/toast';

  function kindClasses(kind: string) {
    switch (kind) {
      case 'success':
        return 'border-green-600 bg-green-900 text-green-100';
      case 'error':
        return 'border-red-600 bg-red-900 text-red-100';
      case 'warning':
        return 'border-yellow-600 bg-yellow-900 text-yellow-100';
      default:
        return 'border-zinc-700 bg-zinc-800 text-zinc-100';
    }
  }

  // Cleanup effect runs only in browser
  $effect(() => {
    return () => {
      clearToastTimeouts();
    };
  });
</script>

<div class="pointer-events-none fixed right-4 top-4 z-50 space-y-2" aria-live="polite">
  {#each $toasts as toast (toast.id)}
    <div
      class={`pointer-events-auto flex items-center gap-2 rounded-md border px-4 py-2 shadow-lg ${kindClasses(toast.kind)}`}
      transition:fly={{ x: 100, duration: 200 }}
    >
      <span class="flex-1">{toast.message}</span>
      <button
        type="button"
        class="ml-2 rounded p-0.5 opacity-70 hover:opacity-100 focus-visible:opacity-100 focus-visible:outline-none focus-visible:ring-1 focus-visible:ring-white/50"
        aria-label="Dismiss"
        onclick={() => removeToast(toast.id)}
      >
        <svg class="h-4 w-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
          <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M6 18L18 6M6 6l12 12" />
        </svg>
      </button>
    </div>
  {/each}
</div>
