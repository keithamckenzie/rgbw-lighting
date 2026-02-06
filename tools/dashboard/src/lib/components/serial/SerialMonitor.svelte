<script lang="ts">
  import {
    serialState,
    serialBuffers,
    VALID_BAUD_RATES,
    refreshPorts,
    openSerialPort,
    closeSerialPort,
    writeSerial,
    handleSerialEvent,
    clearSerialBuffer,
    setActiveConnection
  } from '$lib/stores/serial';
  import type { SerialEvent } from '$lib/types/config';
  import {
    loadStoredString,
    persistStoredString,
    loadStoredNumber,
    persistStoredNumber
  } from '$lib/utils/localStorage';
  import { pushToast } from '$lib/stores/toast';
  import ConfirmDialog from '$lib/components/common/ConfirmDialog.svelte';
  import SerialToolbar from '$lib/components/serial/SerialToolbar.svelte';
  import SerialOutput from '$lib/components/serial/SerialOutput.svelte';
  import SerialInput from '$lib/components/serial/SerialInput.svelte';

  let selectedPort = $state('');
  let baudRate = $state(115200);
  let inputText = $state('');
  let _unlisten: (() => void) | null = $state(null);
  let lastActiveConnectionId: string | null = $state(null);
  let isConnecting = $state(false);
  let isDisconnecting = $state(false);
  let isSending = $state(false);
  let showDisconnectConfirm = $state(false);
  let showClearConfirm = $state(false);
  let autoScrollResetKey = $state(0);
  let isInitialized = $state(false);
  let initError: string | null = $state(null);

  const PORT_STORAGE_KEY = 'rgbw-dashboard:last-serial-port';
  const BAUD_STORAGE_KEY = 'rgbw-dashboard:last-serial-baud';

  const activeConnection = $derived(
    $serialState.activeConnectionId
      ? ($serialState.connections.get($serialState.activeConnectionId) ?? null)
      : null
  );

  const activeBuffer = $derived(
    $serialState.activeConnectionId && $serialBuffers.has($serialState.activeConnectionId)
      ? ($serialBuffers.get($serialState.activeConnectionId) ?? null)
      : null
  );

  const connectionList = $derived(Array.from($serialState.connections.values()));
  const activeLines = $derived(
    activeBuffer
      ? activeBuffer.partialLine
        ? [...activeBuffer.lines, activeBuffer.partialLine]
        : activeBuffer.lines
      : []
  );

  // Clear port if no longer available
  $effect(() => {
    if (selectedPort && !$serialState.availablePorts.some((port) => port.path === selectedPort)) {
      selectedPort = '';
    }
  });

  // Restore stored port
  $effect(() => {
    if (!selectedPort) {
      const storedPort = loadStoredPort();
      if (storedPort && $serialState.availablePorts.some((port) => port.path === storedPort)) {
        selectedPort = storedPort;
      }
    }
  });

  // Initialize on mount
  $effect(() => {
    let unlistenFn: (() => void) | null = null;

    (async () => {
      try {
        // Listen for serial events
        const { listen } = await import('@tauri-apps/api/event');
        unlistenFn = await listen<SerialEvent>('serial-event', (event) => {
          handleSerialEvent(event.payload);
        });
        _unlisten = unlistenFn;

        await refreshPorts();
        const storedBaud = loadStoredBaudRate();
        if (storedBaud && (VALID_BAUD_RATES as readonly number[]).includes(storedBaud)) {
          baudRate = storedBaud;
        }
        isInitialized = true;
        initError = null;
      } catch (e) {
        console.error('Failed to initialize serial monitor:', e);
        initError = 'Failed to initialize serial monitor. Try restarting the app.';
        pushToast(initError, 'error');
        isInitialized = false;
      }
    })();

    return () => {
      if (unlistenFn) unlistenFn();
    };
  });

  // Reset scroll when active connection changes
  $effect(() => {
    if ($serialState.activeConnectionId !== lastActiveConnectionId) {
      lastActiveConnectionId = $serialState.activeConnectionId;
      autoScrollResetKey += 1;
    }
  });

  async function handleConnect() {
    if (!isInitialized) {
      pushToast('Serial monitor is not ready yet.', 'warning');
      return;
    }
    if (!selectedPort) return;

    try {
      isConnecting = true;
      persistSelectedPort(selectedPort);
      persistSelectedBaud(baudRate);
      autoScrollResetKey += 1;
      await openSerialPort(selectedPort, baudRate);
      pushToast(`Connected to ${selectedPort}`, 'success');
    } catch (e) {
      pushToast(`Failed to connect: ${String(e)}`, 'error');
    } finally {
      isConnecting = false;
    }
  }

  async function handleDisconnect() {
    if (!isInitialized) {
      pushToast('Serial monitor is not ready yet.', 'warning');
      return;
    }
    if (!$serialState.activeConnectionId) return;

    try {
      isDisconnecting = true;
      await closeSerialPort($serialState.activeConnectionId);
    } catch (e) {
      console.error('Failed to disconnect:', e);
      pushToast(`Failed to disconnect: ${String(e)}`, 'error');
    } finally {
      isDisconnecting = false;
    }
  }

  async function handleSend() {
    if (!isInitialized) {
      pushToast('Serial monitor is not ready yet.', 'warning');
      return;
    }
    if (!$serialState.activeConnectionId || !inputText) return;

    try {
      isSending = true;
      await writeSerial($serialState.activeConnectionId, inputText + '\n');
      inputText = '';
    } catch (e) {
      console.error('Failed to send:', e);
      pushToast(`Failed to send: ${String(e)}`, 'error');
    } finally {
      isSending = false;
    }
  }

  function handleKeyDown(event: KeyboardEvent) {
    if (event.key === 'Enter' && !event.shiftKey) {
      event.preventDefault();
      handleSend();
    }
  }

  function handleClear() {
    if (!activeConnection) return;
    autoScrollResetKey += 1;
    clearSerialBuffer(activeConnection.connectionId);
  }

  function loadStoredPort(): string | null {
    return loadStoredString(PORT_STORAGE_KEY);
  }

  function persistSelectedPort(port: string): void {
    if (!port) return;
    persistStoredString(PORT_STORAGE_KEY, port);
  }

  function loadStoredBaudRate(): number | null {
    return loadStoredNumber(BAUD_STORAGE_KEY);
  }

  function persistSelectedBaud(rate: number): void {
    persistStoredNumber(BAUD_STORAGE_KEY, rate);
  }
</script>

<div class="flex h-full flex-col" aria-busy={isConnecting || isDisconnecting || isSending}>
  <SerialToolbar
    {connectionList}
    {activeConnection}
    {selectedPort}
    {baudRate}
    availablePorts={$serialState.availablePorts}
    {isConnecting}
    {isDisconnecting}
    {isInitialized}
    onConnect={handleConnect}
    onDisconnect={() => (showDisconnectConfirm = true)}
    onRefreshPorts={() => refreshPorts()}
    onClear={() => (showClearConfirm = true)}
    onPortChange={(value) => {
      selectedPort = value;
      persistSelectedPort(value);
    }}
    onBaudChange={(value) => {
      baudRate = value;
      persistSelectedBaud(value);
    }}
    onConnectionChange={(value) => setActiveConnection(value)}
  />

  <SerialOutput
    lines={activeLines}
    hasConnection={!!activeConnection}
    resetKey={autoScrollResetKey}
  />

  {#if activeConnection}
    <SerialInput
      bind:inputText
      {isSending}
      disabled={!isInitialized}
      onSend={handleSend}
      onKeyDown={handleKeyDown}
    />
  {/if}
</div>

{#if initError}
  <p class="mt-2 text-sm text-red-400">{initError}</p>
{/if}

<ConfirmDialog
  open={showDisconnectConfirm}
  title="Disconnect serial port?"
  message="This will close the active serial connection."
  confirmLabel="Disconnect"
  cancelLabel="Cancel"
  onConfirm={() => {
    showDisconnectConfirm = false;
    handleDisconnect();
  }}
  onCancel={() => (showDisconnectConfirm = false)}
/>

<ConfirmDialog
  open={showClearConfirm}
  title="Clear serial output?"
  message="This will remove all serial output from the panel."
  confirmLabel="Clear"
  cancelLabel="Cancel"
  onConfirm={() => {
    showClearConfirm = false;
    handleClear();
  }}
  onCancel={() => (showClearConfirm = false)}
/>
