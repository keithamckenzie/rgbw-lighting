<script lang="ts">
  let { text, url = undefined }: { text: string; url?: string } = $props();

  let showTooltip = $state(false);
  let wrapperEl: HTMLElement | undefined = $state();

  // Generate unique ID for ARIA association
  const tooltipId = `tooltip-${Math.random().toString(36).slice(2, 9)}`;

  // Toggle tooltip on click (for touch devices)
  function handleClick() {
    showTooltip = !showTooltip;
  }

  // Click-outside handler to dismiss tooltip
  function handleClickOutside(event: MouseEvent) {
    if (showTooltip && wrapperEl && !wrapperEl.contains(event.target as Node)) {
      showTooltip = false;
    }
  }

  $effect(() => {
    document.addEventListener('click', handleClickOutside);
    return () => {
      document.removeEventListener('click', handleClickOutside);
    };
  });
</script>

<span class="help-tooltip-wrapper" bind:this={wrapperEl}>
  <button
    type="button"
    class="help-icon"
    aria-label="Help"
    aria-describedby={showTooltip ? tooltipId : undefined}
    aria-expanded={showTooltip}
    onclick={handleClick}
    onmouseenter={() => (showTooltip = true)}
    onmouseleave={() => (showTooltip = false)}
    onfocus={() => (showTooltip = true)}
    onblur={() => (showTooltip = false)}
  >
    <svg
      xmlns="http://www.w3.org/2000/svg"
      viewBox="0 0 20 20"
      fill="currentColor"
      class="h-4 w-4"
    >
      <path
        fill-rule="evenodd"
        d="M18 10a8 8 0 11-16 0 8 8 0 0116 0zM8.94 6.94a.75.75 0 11-1.061-1.061 3 3 0 112.871 5.026v.345a.75.75 0 01-1.5 0v-.5c0-.72.57-1.172 1.081-1.287A1.5 1.5 0 108.94 6.94zM10 15a1 1 0 100-2 1 1 0 000 2z"
        clip-rule="evenodd"
      />
    </svg>
  </button>

  {#if showTooltip}
    <div id={tooltipId} class="tooltip" role="tooltip">
      <p>{text}</p>
      {#if url}
        <a href={url} target="_blank" rel="noopener noreferrer" class="tooltip-link">
          Learn more
          <svg
            xmlns="http://www.w3.org/2000/svg"
            viewBox="0 0 20 20"
            fill="currentColor"
            class="inline h-3 w-3"
          >
            <path
              fill-rule="evenodd"
              d="M4.25 5.5a.75.75 0 00-.75.75v8.5c0 .414.336.75.75.75h8.5a.75.75 0 00.75-.75v-4a.75.75 0 011.5 0v4A2.25 2.25 0 0112.75 17h-8.5A2.25 2.25 0 012 14.75v-8.5A2.25 2.25 0 014.25 4h5a.75.75 0 010 1.5h-5z"
              clip-rule="evenodd"
            />
            <path
              fill-rule="evenodd"
              d="M6.194 12.753a.75.75 0 001.06.053L16.5 4.44v2.81a.75.75 0 001.5 0v-4.5a.75.75 0 00-.75-.75h-4.5a.75.75 0 000 1.5h2.553l-9.056 8.194a.75.75 0 00-.053 1.06z"
              clip-rule="evenodd"
            />
          </svg>
        </a>
      {/if}
    </div>
  {/if}
</span>

<style>
  .help-tooltip-wrapper {
    position: relative;
    display: inline-flex;
    align-items: center;
  }

  .help-icon {
    display: inline-flex;
    align-items: center;
    justify-content: center;
    padding: 0;
    background: transparent;
    border: none;
    color: rgb(161 161 170); /* zinc-400 */
    cursor: help;
    transition: color 100ms ease-out;
  }

  .help-icon:hover,
  .help-icon:focus {
    color: rgb(59 130 246); /* accent-500 */
    outline: none;
  }

  .tooltip {
    position: absolute;
    left: 50%;
    bottom: calc(100% + 8px);
    transform: translateX(-50%);
    z-index: 50;
    width: max-content;
    max-width: 280px;
    padding: 0.5rem 0.75rem;
    background: rgb(39 39 42); /* zinc-800 */
    border: 1px solid rgb(63 63 70); /* zinc-700 */
    border-radius: 0.375rem;
    box-shadow: 0 10px 15px -3px rgb(0 0 0 / 0.3);
    font-size: 0.75rem;
    line-height: 1.5;
    color: rgb(228 228 231); /* zinc-200 */
  }

  .tooltip::after {
    content: '';
    position: absolute;
    top: 100%;
    left: 50%;
    transform: translateX(-50%);
    border: 6px solid transparent;
    border-top-color: rgb(63 63 70); /* zinc-700 */
  }

  .tooltip p {
    margin: 0;
  }

  .tooltip-link {
    display: inline-flex;
    align-items: center;
    gap: 0.25rem;
    margin-top: 0.5rem;
    color: rgb(96 165 250); /* accent-400 */
    text-decoration: none;
  }

  .tooltip-link:hover {
    text-decoration: underline;
  }
</style>
