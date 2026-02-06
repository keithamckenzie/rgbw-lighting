<script lang="ts">
  let {
    inputText = $bindable(''),
    isSending = false,
    disabled = false,
    onSend,
    onKeyDown
  }: {
    inputText?: string;
    isSending?: boolean;
    disabled?: boolean;
    onSend: () => void;
    onKeyDown: (event: KeyboardEvent) => void;
  } = $props();
</script>

<div class="flex items-center gap-2 border-t border-zinc-800 bg-zinc-900 p-3">
  <input
    type="text"
    bind:value={inputText}
    onkeydown={onKeyDown}
    placeholder="Type a command and press Enter..."
    aria-label="Serial command input"
    {disabled}
    class="input-base flex-1"
  />
  <button
    type="button"
    class="btn btn-primary"
    disabled={disabled || isSending || !inputText}
    onclick={onSend}
  >
    {isSending ? 'Sending...' : 'Send'}
  </button>
</div>
