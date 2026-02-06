<script lang="ts">
  import type { PinValidation, PinPurpose } from '$lib/types/config';
  import {
    validatePin,
    getSafePins,
    getSeverityClass,
    getSeverityIcon
  } from '$lib/validation/pins';
  import { debounce } from '$lib/utils/debounce';

  let {
    value,
    purpose = 'output',
    platform = 'esp32',
    module = undefined,
    label = 'Pin',
    disabled = false,
    id = undefined,
    onChange
  }: {
    value: number | string;
    purpose?: PinPurpose;
    platform?: string;
    module?: string | undefined;
    label?: string;
    disabled?: boolean;
    id?: string | undefined;
    onChange: (value: number | string) => void;
  } = $props();

  const fallbackId = `pin-input-${Math.random().toString(36).slice(2, 9)}`;
  const inputId = $derived(id ?? fallbackId);

  let validation: PinValidation | null = $state(null);
  let safePins: number[] = $state([]);
  let isValidating = $state(false);
  let validationRequestId = $state(0);
  let safePinsRequestId = $state(0);
  let safePinsKey = $state('');

  const debouncedValidate = debounce(
    (
      pinValue: number | string,
      pinPurpose: PinPurpose,
      pinPlatform: string,
      pinModule?: string
    ) => {
      void validate(pinValue, pinPurpose, pinPlatform, pinModule);
    },
    250
  );

  // Cleanup debounce on destroy
  $effect(() => {
    return () => {
      debouncedValidate.cancel();
    };
  });

  // Reactive validation
  $effect(() => {
    const hasValue = value !== undefined && value !== null && value !== '';
    if (hasValue) {
      debouncedValidate(value, purpose, platform, module);
    } else {
      debouncedValidate.cancel();
      validationRequestId += 1;
      isValidating = false;
      validation = null;
    }
  });

  // Reactive safe pins refresh
  $effect(() => {
    const nextKey = `${platform}:${module ?? ''}`;
    if (nextKey !== safePinsKey) {
      safePinsKey = nextKey;
      void refreshSafePins(platform, module);
    }
  });

  async function validate(
    pinValue: number | string,
    pinPurpose: PinPurpose,
    pinPlatform: string,
    pinModule?: string
  ) {
    const requestId = ++validationRequestId;
    const expectedValue = normalizePinValue(pinValue);
    isValidating = true;
    try {
      const result = await validatePin(pinValue, pinPurpose, pinPlatform, pinModule);
      if (requestId !== validationRequestId) return;
      if (normalizePinValue(value) !== expectedValue) return;
      validation = result;
    } finally {
      if (requestId === validationRequestId) {
        isValidating = false;
      }
    }
  }

  function normalizePinValue(pin: number | string | null | undefined): string {
    if (pin === null || pin === undefined) return '';
    if (typeof pin === 'number') return String(pin);
    const trimmed = pin.trim();
    if (!trimmed) return '';
    if (trimmed.toUpperCase() === 'A0') return 'A0';
    if (/^\d+$/.test(trimmed)) return String(parseInt(trimmed, 10));
    return trimmed;
  }

  async function refreshSafePins(pinPlatform: string, pinModule?: string) {
    const requestId = ++safePinsRequestId;
    safePins = [];
    const pins = await getSafePins(pinPlatform, pinModule);
    if (requestId !== safePinsRequestId) return;
    safePins = pins;
  }

  function handleInput(event: Event) {
    const target = event.target as HTMLInputElement;
    const raw = target.value;
    const trimmed = raw.trim();

    let newValue: number | string;
    // Allow 'A0' for ESP8266 ADC
    if (trimmed.toUpperCase() === 'A0' || trimmed.toLowerCase() === 'a') {
      newValue = trimmed.toUpperCase();
    } else if (trimmed === '') {
      newValue = '';
    } else if (/^\d+$/.test(trimmed)) {
      newValue = parseInt(trimmed, 10);
    } else {
      newValue = raw;
    }

    onChange(newValue);
  }

  function handleSelectChange(event: Event) {
    const target = event.target;
    if (!(target instanceof HTMLSelectElement)) return;
    const v = target.value;
    if (!v) return;
    const newValue = parseInt(v, 10);
    onChange(newValue);
  }
</script>

<div class="flex flex-col gap-1">
  {#if label}
    <label for={inputId} class="text-sm font-medium text-zinc-300">
      {label}
    </label>
  {/if}

  <div class="flex items-center gap-2">
    <div class="relative flex-1">
      <input
        id={inputId}
        type="text"
        {value}
        oninput={handleInput}
        disabled={disabled || isValidating}
        class="input-base pr-8"
        class:input-error={validation?.severity === 'error'}
        class:border-yellow-500={validation?.severity === 'warning'}
        class:border-blue-500={validation?.severity === 'info'}
        placeholder="GPIO number"
        aria-invalid={validation?.severity === 'error' ? true : undefined}
        aria-describedby={validation?.message ? `${inputId}-validation` : undefined}
      />

      {#if isValidating}
        <span class="absolute right-3 top-1/2 -translate-y-1/2 text-zinc-400">...</span>
      {:else if validation && validation.severity !== 'ok'}
        <!-- Only show icon for warnings/errors/info, not for successful validation -->
        <span
          class="absolute right-3 top-1/2 -translate-y-1/2 {getSeverityClass(validation.severity)}"
        >
          {getSeverityIcon(validation.severity)}
        </span>
      {/if}
    </div>

    {#if safePins.length > 0}
      <select
        class="input-base w-auto"
        onchange={handleSelectChange}
        disabled={disabled || isValidating}
      >
        <option value="">Suggested pins</option>
        {#each safePins as pin}
          <option value={pin}>GPIO {pin}</option>
        {/each}
      </select>
    {/if}
  </div>

  {#if validation && validation.message}
    <p id="{inputId}-validation" class="text-sm {getSeverityClass(validation.severity)}">
      {validation.message}
    </p>
  {/if}
</div>
