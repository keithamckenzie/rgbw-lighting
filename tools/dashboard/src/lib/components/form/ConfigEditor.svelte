<script lang="ts">
  import type {
    AppConfigSchema,
    ConfigDefine,
    DiscoveredEnvironment,
    EnhancedDefine,
    PinPurpose
  } from '$lib/types/config';
  import { configState, updateDefine, profileLoading } from '$lib/stores/config';
  import { getPlatformFromEnvironment } from '$lib/types/config';
  import { getPlatformKey } from '$lib/utils/platform';
  import { enhanceDefines, groupDefines, filterAdvanced } from '$lib/utils/configEnhancer';
  import PinSelector from './PinSelector.svelte';
  import HelpTooltip from '$lib/components/common/HelpTooltip.svelte';
  import CollapsibleSection from '$lib/components/common/CollapsibleSection.svelte';

  let {
    schema,
    environment,
    excludeAudioFields = false
  }: {
    schema: AppConfigSchema;
    environment: DiscoveredEnvironment;
    excludeAudioFields?: boolean;
  } = $props();

  let showAdvanced = $state(false);

  // Audio field prefixes to filter out when Audio tab is present
  const AUDIO_FIELD_PREFIXES = ['AUDIO_', 'PIN_I2S_', 'BEAT_', 'BPM_'];

  const platform = $derived(getPlatformFromEnvironment(environment));
  const platformKey = $derived(getPlatformKey(platform));

  // Combine global defines with platform-specific ones
  const rawDefines = $derived([
    ...schema.defines,
    ...(schema.platform_conditional[platformKey] || []),
    ...(schema.platform_conditional[platform] || [])
  ] as ConfigDefine[]);

  // Enhance with metadata
  const enhancedDefines = $derived(enhanceDefines(rawDefines));

  // Filter out audio fields when Audio tab handles them separately
  const audioFilteredDefines = $derived(
    excludeAudioFields
      ? enhancedDefines.filter((d) => !AUDIO_FIELD_PREFIXES.some((p) => d.name.startsWith(p)))
      : enhancedDefines
  );

  // Filter by advanced toggle
  const filteredDefines = $derived(filterAdvanced(audioFilteredDefines, showAdvanced));

  // Group by category
  const groupedDefines = $derived(groupDefines(filteredDefines));

  // Count advanced fields (from audio-filtered list)
  const advancedCount = $derived(audioFilteredDefines.filter((d) => d.advanced).length);

  function getValue(name: string, defaultValue: string): string {
    return $configState.defines[name] ?? defaultValue;
  }

  function handleChange(define: EnhancedDefine, value: string) {
    updateDefine(define.name, value);
  }

  function handleSelectChange(define: EnhancedDefine, event: Event) {
    const target = event.target;
    if (!(target instanceof HTMLSelectElement)) return;
    handleChange(define, target.value);
  }

  function inferPinPurpose(define: EnhancedDefine): PinPurpose {
    const context =
      `${define.name} ${define.description ?? ''} ${define.default_value ?? ''}`.toUpperCase();

    if (context.includes('ADC') || context.includes('ANALOG') || context.includes('A0')) {
      return 'adc';
    }
    if (context.includes('I2C') || context.includes('SCL') || context.includes('SDA')) {
      return 'i2c';
    }
    if (
      context.includes('I2S') ||
      context.includes('BCLK') ||
      context.includes('LRCLK') ||
      context.includes('WS') ||
      context.includes('MCLK') ||
      context.includes('DIN') ||
      context.includes('DOUT')
    ) {
      return 'i2s';
    }
    if (
      context.includes('SPI') ||
      context.includes('MOSI') ||
      context.includes('MISO') ||
      context.includes('SCK') ||
      context.includes('CS') ||
      context.includes('SS')
    ) {
      return 'spi';
    }
    if (context.includes('PWM')) {
      return 'pwm';
    }
    if (
      context.includes('INPUT') ||
      context.includes('BUTTON') ||
      context.includes('BTN') ||
      context.includes('SWITCH') ||
      context.includes('SENSE') ||
      context.includes('SENSOR') ||
      context.includes('RX') ||
      context.includes('MIC') ||
      context.includes('AUDIO_IN') ||
      context.includes('LINE_IN')
    ) {
      return 'input';
    }
    if (context.includes('TX')) {
      return 'output';
    }

    return 'output';
  }

  function getEnumDescription(define: EnhancedDefine, value: string): string | undefined {
    return define.enumDescriptions?.[value];
  }

  // Generate unique ID for label/input association
  function getFieldId(name: string): string {
    return `field-${name}`;
  }

  // Validation for numeric fields
  interface ValidationResult {
    valid: boolean;
    message: string;
  }

  function validateValue(value: string, define: EnhancedDefine): ValidationResult {
    if (define.value_type !== 'integer' && define.value_type !== 'float') {
      return { valid: true, message: '' };
    }

    const num = parseFloat(value);
    if (isNaN(num)) {
      return { valid: false, message: 'Must be a number' };
    }

    if (define.value_type === 'integer' && !Number.isInteger(num)) {
      return { valid: false, message: 'Must be a whole number' };
    }

    if (define.min !== undefined && num < define.min) {
      return { valid: false, message: `Minimum is ${define.min}` };
    }

    if (define.max !== undefined && num > define.max) {
      return { valid: false, message: `Maximum is ${define.max}` };
    }

    return { valid: true, message: '' };
  }
</script>

<div class="config-editor" class:opacity-50={$profileLoading} aria-busy={$profileLoading}>
  {#if $profileLoading}
    <div class="flex items-center gap-2 py-2 text-sm text-zinc-400" role="status">
      <span class="h-3 w-3 animate-spin rounded-full border-2 border-zinc-500 border-t-transparent" aria-hidden="true"></span>
      Loading profile...
    </div>
  {:else if enhancedDefines.length === 0}
    <p class="text-zinc-400">No configurable options for this app.</p>
  {:else}
    <!-- Advanced toggle at top -->
    {#if advancedCount > 0}
      <div class="advanced-toggle-container">
        <button
          type="button"
          class="advanced-toggle"
          onclick={() => (showAdvanced = !showAdvanced)}
        >
          <svg
            xmlns="http://www.w3.org/2000/svg"
            viewBox="0 0 20 20"
            fill="currentColor"
            class="toggle-icon"
            class:open={showAdvanced}
          >
            <path
              fill-rule="evenodd"
              d="M5.23 7.21a.75.75 0 011.06.02L10 11.168l3.71-3.938a.75.75 0 111.08 1.04l-4.25 4.5a.75.75 0 01-1.08 0l-4.25-4.5a.75.75 0 01.02-1.06z"
              clip-rule="evenodd"
            />
          </svg>
          {showAdvanced ? 'Hide' : 'Show'} Advanced Options ({advancedCount})
        </button>
      </div>
    {/if}

    <!-- Groups -->
    {#each [...groupedDefines] as [groupName, defines] (groupName)}
      <CollapsibleSection title={groupName} count={defines.length}>
        <div class="space-y-3">
          {#each defines as define (define.name)}
            <div class="field-row">
              <!-- Read-only / derived fields -->
              {#if define.readOnly}
                <div class="field-header">
                  <span class="field-label">{define.label}</span>
                  {#if define.description}
                    <HelpTooltip text={define.description} url={define.helpUrl} />
                  {/if}
                </div>
                <div class="field-readonly">
                  <span class="field-readonly-value">
                    {getValue(define.name, define.default_value)}
                  </span>
                  {#if define.unit}
                    <span class="field-unit">{define.unit}</span>
                  {/if}
                  {#if define.derivedFrom && define.derivedFrom.length > 0}
                    <span class="field-derived">
                      Computed from {define.derivedFrom.join(', ')}
                    </span>
                  {/if}
                </div>

              <!-- Pin selector -->
              {:else if define.value_type === 'pin'}
                <div class="field-header">
                  <label for={getFieldId(define.name)} class="field-label">{define.label}</label>
                  {#if define.description}
                    <HelpTooltip text={define.description} url={define.helpUrl} />
                  {/if}
                </div>
                <PinSelector
                  id={getFieldId(define.name)}
                  label=""
                  value={getValue(define.name, define.default_value)}
                  purpose={inferPinPurpose(define)}
                  {platform}
                  module={environment.board ?? undefined}
                  disabled={$profileLoading}
                  onChange={(val) => handleChange(define, String(val))}
                />

              <!-- Enum dropdown -->
              {:else if define.value_type === 'enum' && define.enum_values}
                <div class="field-header">
                  <label for={getFieldId(define.name)} class="field-label">{define.label}</label>
                  {#if define.description}
                    <HelpTooltip text={define.description} url={define.helpUrl} />
                  {/if}
                </div>
                <select
                  id={getFieldId(define.name)}
                  class="field-select"
                  value={getValue(define.name, define.default_value)}
                  disabled={$profileLoading}
                  onchange={(e) => handleSelectChange(define, e)}
                >
                  {#each define.enum_values as opt}
                    <option value={opt.value}>{opt.label}</option>
                  {/each}
                </select>
                {#if getEnumDescription(define, getValue(define.name, define.default_value))}
                  <p class="field-hint">
                    {getEnumDescription(define, getValue(define.name, define.default_value))}
                  </p>
                {/if}

              <!-- Boolean toggle -->
              {:else if define.value_type === 'boolean'}
                <label class="boolean-field" for={getFieldId(define.name)}>
                  <input
                    id={getFieldId(define.name)}
                    type="checkbox"
                    checked={getValue(define.name, define.default_value) === '1' ||
                      getValue(define.name, define.default_value) === 'true'}
                    disabled={$profileLoading}
                    onchange={(e) =>
                      handleChange(define, (e.target as HTMLInputElement).checked ? '1' : '0')}
                    class="field-checkbox"
                  />
                  <span class="boolean-label">{define.label}</span>
                  {#if define.description}
                    <HelpTooltip text={define.description} url={define.helpUrl} />
                  {/if}
                </label>

              <!-- Float input -->
              {:else if define.value_type === 'float'}
                {@const validation = validateValue(getValue(define.name, define.default_value), define)}
                <div class="field-header">
                  <label for={getFieldId(define.name)} class="field-label">{define.label}</label>
                  {#if define.description}
                    <HelpTooltip text={define.description} url={define.helpUrl} />
                  {/if}
                </div>
                <input
                  id={getFieldId(define.name)}
                  type="number"
                  step="0.01"
                  min={define.min}
                  max={define.max}
                  value={getValue(define.name, define.default_value)}
                  disabled={$profileLoading}
                  onchange={(e) => handleChange(define, (e.target as HTMLInputElement).value)}
                  class="field-input"
                  class:field-error={!validation.valid}
                  aria-invalid={!validation.valid}
                  aria-describedby={!validation.valid ? `error-${define.name}` : undefined}
                />
                {#if !validation.valid}
                  <p class="validation-error" id="error-{define.name}">{validation.message}</p>
                {:else if define.min !== undefined || define.max !== undefined}
                  <p class="field-hint">
                    {#if define.min !== undefined && define.max !== undefined}
                      Range: {define.min} - {define.max}
                    {:else if define.min !== undefined}
                      Minimum: {define.min}
                    {:else if define.max !== undefined}
                      Maximum: {define.max}
                    {/if}
                  </p>
                {/if}

              <!-- Integer input -->
              {:else if define.value_type === 'integer'}
                {@const validation = validateValue(getValue(define.name, define.default_value), define)}
                <div class="field-header">
                  <label for={getFieldId(define.name)} class="field-label">{define.label}</label>
                  {#if define.description}
                    <HelpTooltip text={define.description} url={define.helpUrl} />
                  {/if}
                </div>
                <input
                  id={getFieldId(define.name)}
                  type="number"
                  step="1"
                  min={define.min}
                  max={define.max}
                  value={getValue(define.name, define.default_value)}
                  disabled={$profileLoading}
                  onchange={(e) => handleChange(define, (e.target as HTMLInputElement).value)}
                  class="field-input"
                  class:field-error={!validation.valid}
                  aria-invalid={!validation.valid}
                  aria-describedby={!validation.valid ? `error-${define.name}` : undefined}
                />
                {#if !validation.valid}
                  <p class="validation-error" id="error-{define.name}">{validation.message}</p>
                {:else if define.min !== undefined || define.max !== undefined}
                  <p class="field-hint">
                    {#if define.min !== undefined && define.max !== undefined}
                      Range: {define.min} - {define.max}
                    {:else if define.min !== undefined}
                      Minimum: {define.min}
                    {:else if define.max !== undefined}
                      Maximum: {define.max}
                    {/if}
                  </p>
                {/if}

              <!-- String/text input -->
              {:else}
                <div class="field-header">
                  <label for={getFieldId(define.name)} class="field-label">{define.label}</label>
                  {#if define.description}
                    <HelpTooltip text={define.description} url={define.helpUrl} />
                  {/if}
                </div>
                <input
                  id={getFieldId(define.name)}
                  type="text"
                  value={getValue(define.name, define.default_value)}
                  disabled={$profileLoading}
                  onchange={(e) => handleChange(define, (e.target as HTMLInputElement).value)}
                  class="field-input"
                />
              {/if}

              <!-- Platform badge -->
              {#if define.platform}
                <span class="platform-badge">
                  {define.platform} only
                </span>
              {/if}
            </div>
          {/each}
        </div>
      </CollapsibleSection>
    {/each}
  {/if}
</div>

<style>
  .config-editor {
    display: flex;
    flex-direction: column;
    gap: 0.75rem;
  }

  .advanced-toggle-container {
    position: sticky;
    top: 0;
    z-index: 10;
    margin: -1rem -1rem 0.5rem -1rem;
    padding: 0.5rem 1rem;
    background: rgb(24 24 27 / 0.95); /* zinc-900 */
    backdrop-filter: blur(4px);
    border-bottom: 1px solid rgb(39 39 42); /* zinc-800 */
  }

  .field-row {
    display: flex;
    flex-direction: column;
    gap: 0.375rem;
  }

  .field-header {
    display: flex;
    align-items: center;
    gap: 0.5rem;
  }

  .field-label {
    font-size: 0.875rem;
    font-weight: 500;
    color: rgb(212 212 216); /* zinc-300 */
  }

  .field-input,
  .field-select {
    width: 100%;
    height: 2.5rem;
    padding: 0.5rem 0.75rem;
    background: rgb(39 39 42); /* zinc-800 */
    border: 1px solid rgb(39 39 42); /* zinc-800 */
    border-radius: 0.375rem;
    color: rgb(250 250 250); /* zinc-50 */
    font-size: 0.875rem;
    transition:
      border-color 100ms ease-out,
      box-shadow 100ms ease-out;
  }

  .field-input:hover:not(:disabled),
  .field-select:hover:not(:disabled) {
    border-color: rgb(63 63 70); /* zinc-700 */
  }

  .field-input:focus,
  .field-select:focus {
    outline: none;
    border-color: rgb(59 130 246); /* accent-500 */
    box-shadow: 0 0 0 3px rgb(59 130 246 / 0.3);
  }

  .field-input:disabled,
  .field-select:disabled {
    opacity: 0.5;
    cursor: not-allowed;
  }

  .field-error {
    border-color: rgb(239 68 68) !important; /* error-500 */
  }

  .field-error:focus {
    box-shadow: 0 0 0 3px rgb(239 68 68 / 0.3);
  }

  .validation-error {
    margin: 0;
    font-size: 0.75rem;
    color: rgb(248 113 113); /* error-400 */
  }

  .field-hint {
    margin: 0;
    font-size: 0.75rem;
    color: rgb(113 113 122); /* zinc-500 */
  }

  .field-readonly {
    display: flex;
    align-items: baseline;
    gap: 0.5rem;
    padding: 0.5rem 0.75rem;
    background: rgb(24 24 27); /* zinc-900 */
    border: 1px dashed rgb(63 63 70); /* zinc-700 */
    border-radius: 0.375rem;
  }

  .field-readonly-value {
    font-size: 0.875rem;
    font-weight: 500;
    color: rgb(161 161 170); /* zinc-400 */
    font-family: ui-monospace, monospace;
  }

  .field-unit {
    font-size: 0.75rem;
    color: rgb(113 113 122); /* zinc-500 */
  }

  .field-derived {
    margin-left: auto;
    font-size: 0.625rem;
    color: rgb(113 113 122); /* zinc-500 */
    font-style: italic;
  }

  .field-checkbox {
    width: 1.25rem;
    height: 1.25rem;
    border-radius: 0.25rem;
    border: 1px solid rgb(63 63 70); /* zinc-700 */
    background: rgb(39 39 42); /* zinc-800 */
    cursor: pointer;
    transition:
      border-color 100ms ease-out,
      box-shadow 100ms ease-out;
  }

  .field-checkbox:checked {
    background: rgb(59 130 246); /* accent-500 */
    border-color: rgb(59 130 246);
  }

  .field-checkbox:focus {
    outline: none;
    box-shadow: 0 0 0 3px rgb(59 130 246 / 0.3);
  }

  .boolean-field {
    display: flex;
    align-items: center;
    gap: 0.75rem;
    cursor: pointer;
  }

  .boolean-label {
    font-size: 0.875rem;
    font-weight: 500;
    color: rgb(212 212 216); /* zinc-300 */
  }

  .platform-badge {
    display: inline-block;
    margin-top: 0.25rem;
    padding: 0.125rem 0.5rem;
    background: rgb(39 39 42); /* zinc-800 */
    border-radius: 0.375rem;
    font-size: 0.75rem;
    color: rgb(161 161 170); /* zinc-400 */
  }

  .advanced-toggle {
    display: flex;
    align-items: center;
    gap: 0.5rem;
    width: 100%;
    padding: 0.5rem 0.75rem;
    background: transparent;
    border: 1px dashed rgb(63 63 70); /* zinc-700 */
    border-radius: 0.375rem;
    color: rgb(161 161 170); /* zinc-400 */
    font-size: 0.875rem;
    cursor: pointer;
    transition: all 150ms ease-out;
  }

  .advanced-toggle:hover {
    border-color: rgb(59 130 246); /* accent-500 */
    color: rgb(96 165 250); /* accent-400 */
  }

  .toggle-icon {
    width: 1rem;
    height: 1rem;
    transition: transform 200ms ease-out;
  }

  .toggle-icon.open {
    transform: rotate(180deg);
  }
</style>
