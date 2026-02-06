<script lang="ts">
  import type { Snippet } from 'svelte';

  let {
    title,
    defaultOpen = true,
    count = undefined,
    children
  }: {
    title: string;
    defaultOpen?: boolean;
    count?: number | undefined;
    children: Snippet;
  } = $props();

  // Track user's explicit toggle separately; fall back to prop until user interacts
  let userToggle: boolean | undefined = $state(undefined);
  const open = $derived(userToggle ?? defaultOpen);
</script>

<div class="collapsible-section">
  <button
    type="button"
    class="section-header"
    aria-expanded={open}
    onclick={() => (userToggle = !open)}
  >
    <span class="section-title">
      {title}
      {#if count !== undefined}
        <span class="section-count">({count})</span>
      {/if}
    </span>
    <svg
      xmlns="http://www.w3.org/2000/svg"
      viewBox="0 0 20 20"
      fill="currentColor"
      class="chevron"
      class:open
    >
      <path
        fill-rule="evenodd"
        d="M5.23 7.21a.75.75 0 011.06.02L10 11.168l3.71-3.938a.75.75 0 111.08 1.04l-4.25 4.5a.75.75 0 01-1.08 0l-4.25-4.5a.75.75 0 01.02-1.06z"
        clip-rule="evenodd"
      />
    </svg>
  </button>

  {#if open}
    <div class="section-content">
      {@render children()}
    </div>
  {/if}
</div>

<style>
  .collapsible-section {
    background: rgb(24 24 27); /* zinc-900 */
    border-radius: 0.5rem;
    box-shadow: 0 4px 6px -1px rgb(0 0 0 / 0.4), 0 2px 4px -2px rgb(0 0 0 / 0.3);
    overflow: hidden;
  }

  .section-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    width: 100%;
    padding: 0.75rem 1rem;
    background: transparent;
    border: none;
    cursor: pointer;
    text-align: left;
    transition: background-color 100ms ease-out;
  }

  .section-header:hover {
    background: rgb(39 39 42); /* zinc-800 */
  }

  .section-header:focus {
    outline: none;
    background: rgb(39 39 42); /* zinc-800 */
  }

  .section-title {
    font-size: 0.875rem;
    font-weight: 600;
    color: rgb(228 228 231); /* zinc-200 */
  }

  .section-count {
    font-weight: 400;
    color: rgb(161 161 170); /* zinc-400 */
    margin-left: 0.25rem;
  }

  .chevron {
    width: 1.25rem;
    height: 1.25rem;
    color: rgb(161 161 170); /* zinc-400 */
    transition: transform 200ms ease-out;
    flex-shrink: 0;
  }

  .chevron.open {
    transform: rotate(180deg);
  }

  .section-content {
    padding: 0.75rem 1rem 1rem;
    border-top: 1px solid rgb(39 39 42); /* zinc-800 */
  }
</style>
