<script lang="ts">
	import { configState, updateDefine, profileLoading } from '$lib/stores/config';
	import type { AppConfigSchema, ConfigDefine } from '$lib/types/config';
	import { getPlatformKey } from '$lib/utils/platform';
	import PinSelector from './PinSelector.svelte';

	// Explicit props keep this component stateless and avoid hidden coupling via context.
	export let platform = 'esp32';
	export let schema: AppConfigSchema = { has_config: false, defines: [], platform_conditional: {} };

	const audioDefineNames = [
		'AUDIO_ENABLED',
		'AUDIO_INPUT_MODE',
		'PIN_I2S_SCK',
		'PIN_I2S_WS',
		'PIN_I2S_SD',
		'PIN_I2S_MCLK',
		'BEAT_THRESHOLD',
		'BEAT_COOLDOWN_MS',
		'BPM_EMA_ALPHA'
	];

	// Audio is ESP32-only
	$: isEsp32 = platform.toLowerCase().includes('esp32');
	$: platformKey = getPlatformKey(platform);
	$: visibleDefines = [
		...schema.defines,
		...(schema.platform_conditional[platformKey] || []),
		...(schema.platform_conditional[platform] || [])
	];
	$: defineLookup = new Map<string, ConfigDefine>(visibleDefines.map((define) => [define.name, define]));
	$: hasAudioOptions = audioDefineNames.some((name) => defineLookup.has(name));

	$: audioEnabledDefine = defineLookup.get('AUDIO_ENABLED') ?? null;
	$: inputModeDefine = defineLookup.get('AUDIO_INPUT_MODE') ?? null;
	$: isAudioEnabled = audioEnabledDefine
		? ['1', 'true'].includes(getValue('AUDIO_ENABLED', audioEnabledDefine.default_value))
		: true;
	$: inputModeValue = inputModeDefine
		? getValue('AUDIO_INPUT_MODE', inputModeDefine.default_value)
		: null;
	$: isAdcMode = inputModeValue === '1';

	function hasDefine(name: string): boolean {
		return defineLookup.has(name);
	}

	function getDefine(name: string): ConfigDefine | null {
		return defineLookup.get(name) ?? null;
	}

	function getValue(name: string, defaultValue: string): string {
		return $configState.defines[name] ?? defaultValue;
	}

	function setBool(name: string, checked: boolean) {
		updateDefine(name, checked ? '1' : '0');
	}

	function setNumber(name: string, value: string) {
		updateDefine(name, value);
	}

	function handleInputModeChange(event: Event) {
		const target = event.target;
		if (!(target instanceof HTMLSelectElement)) return;
		setNumber('AUDIO_INPUT_MODE', target.value);
	}
</script>

{#if !isEsp32}
	<div class="rounded-lg border border-slate-700 bg-slate-800 p-4">
		<p class="text-slate-400">Audio input is only available on ESP32 platforms.</p>
	</div>
{:else if !hasAudioOptions}
	<div class="rounded-lg border border-slate-700 bg-slate-800 p-4">
		<p class="text-slate-400">No audio configuration options are available for this app.</p>
	</div>
{:else}
	<div class="space-y-4">
		{#if audioEnabledDefine}
			<!-- Enable Audio -->
			<div class="rounded-lg border border-slate-700 bg-slate-800 p-4">
				<label class="flex items-center gap-3">
					<input
						type="checkbox"
						checked={isAudioEnabled}
						disabled={$profileLoading}
						on:change={(e) => setBool('AUDIO_ENABLED', (e.target as HTMLInputElement).checked)}
						class="h-5 w-5 rounded border-slate-600 bg-slate-700 text-primary-500 focus:ring-primary-500"
					/>
					<span class="font-medium text-white">Enable Audio Input</span>
				</label>
				<p class="mt-2 text-sm text-slate-400">
					Enable sound-reactive lighting with I2S microphone or ADC input.
				</p>
			</div>
		{/if}

		{#if isAudioEnabled}
			{#if inputModeDefine}
				<!-- Input Mode -->
				<div class="rounded-lg border border-slate-700 bg-slate-800 p-4">
					<label class="block">
						<span class="text-sm font-medium text-slate-300">Audio Input Mode</span>
						<select
							class="mt-1 block w-full rounded border border-slate-600 bg-slate-700 px-3 py-2 text-white focus:outline-none focus:ring-2 focus:ring-primary-500"
							value={getValue('AUDIO_INPUT_MODE', inputModeDefine.default_value)}
							disabled={$profileLoading}
							on:change={handleInputModeChange}
						>
							<option value="0">I2S Microphone (INMP441/SPH0645)</option>
							<option value="1">I2S ADC (PCM1808)</option>
						</select>
					</label>
					<p class="mt-2 text-sm text-slate-400">
						{inputModeValue === '0'
							? 'Digital MEMS microphone via I2S (recommended for ambient audio)'
							: 'External ADC via I2S for line-level audio input (requires MCLK)'}
					</p>
				</div>
			{/if}

			{#if hasDefine('PIN_I2S_SCK') || hasDefine('PIN_I2S_WS') || hasDefine('PIN_I2S_SD') || hasDefine('PIN_I2S_MCLK')}
				<!-- I2S Pins -->
				<div class="rounded-lg border border-slate-700 bg-slate-800 p-4">
					<h3 class="mb-4 font-medium text-white">I2S Pins</h3>
					<div class="grid grid-cols-2 gap-4">
						{#if hasDefine('PIN_I2S_SCK')}
							<PinSelector
								label="SCK (Serial Clock)"
								value={getValue('PIN_I2S_SCK', getDefine('PIN_I2S_SCK')?.default_value ?? '0')}
								purpose="output"
								{platform}
								disabled={$profileLoading}
								on:change={(e) => setNumber('PIN_I2S_SCK', String(e.detail))}
							/>
						{/if}
						{#if hasDefine('PIN_I2S_WS')}
							<PinSelector
								label="WS (Word Select / LRCK)"
								value={getValue('PIN_I2S_WS', getDefine('PIN_I2S_WS')?.default_value ?? '0')}
								purpose="output"
								{platform}
								disabled={$profileLoading}
								on:change={(e) => setNumber('PIN_I2S_WS', String(e.detail))}
							/>
						{/if}
						{#if hasDefine('PIN_I2S_SD')}
							<PinSelector
								label="SD (Serial Data)"
								value={getValue('PIN_I2S_SD', getDefine('PIN_I2S_SD')?.default_value ?? '0')}
								purpose="input"
								{platform}
								disabled={$profileLoading}
								on:change={(e) => setNumber('PIN_I2S_SD', String(e.detail))}
							/>
						{/if}
						{#if hasDefine('PIN_I2S_MCLK') && (!inputModeDefine || isAdcMode)}
							<PinSelector
								label="MCLK (Master Clock)"
								value={getValue('PIN_I2S_MCLK', getDefine('PIN_I2S_MCLK')?.default_value ?? '0')}
								purpose="output"
								{platform}
								disabled={$profileLoading}
								on:change={(e) => setNumber('PIN_I2S_MCLK', String(e.detail))}
							/>
						{/if}
					</div>
					{#if hasDefine('PIN_I2S_MCLK') && inputModeDefine && isAdcMode}
						<p class="mt-2 text-sm text-slate-400">
							PCM1808 requires MCLK. GPIO 0 provides hardware clock output, but conflicts with boot
							mode. Consider external oscillator for production.
						</p>
					{/if}
				</div>
			{/if}

			{#if hasDefine('BEAT_THRESHOLD') || hasDefine('BEAT_COOLDOWN_MS') || hasDefine('BPM_EMA_ALPHA')}
				<!-- Beat Detection -->
				<div class="rounded-lg border border-slate-700 bg-slate-800 p-4">
					<h3 class="mb-4 font-medium text-white">Beat Detection</h3>
					<div class="grid grid-cols-2 gap-4">
						{#if hasDefine('BEAT_THRESHOLD')}
							<label class="block">
								<span class="text-sm font-medium text-slate-300">Beat Threshold</span>
								<input
									type="number"
									step="0.1"
									min="1.0"
									max="3.0"
									value={getValue(
										'BEAT_THRESHOLD',
										getDefine('BEAT_THRESHOLD')?.default_value ?? '1.5'
									)}
									disabled={$profileLoading}
									on:change={(e) => setNumber('BEAT_THRESHOLD', (e.target as HTMLInputElement).value)}
									class="mt-1 block w-full rounded border border-slate-600 bg-slate-700 px-3 py-2 text-white"
								/>
								<p class="mt-1 text-xs text-slate-400">Energy ratio for beat detection (1.0-3.0)</p>
							</label>
						{/if}

						{#if hasDefine('BEAT_COOLDOWN_MS')}
							<label class="block">
								<span class="text-sm font-medium text-slate-300">Beat Cooldown (ms)</span>
								<input
									type="number"
									step="10"
									min="50"
									max="500"
									value={getValue(
										'BEAT_COOLDOWN_MS',
										getDefine('BEAT_COOLDOWN_MS')?.default_value ?? '200'
									)}
									disabled={$profileLoading}
									on:change={(e) =>
										setNumber('BEAT_COOLDOWN_MS', (e.target as HTMLInputElement).value)
									}
									class="mt-1 block w-full rounded border border-slate-600 bg-slate-700 px-3 py-2 text-white"
								/>
								<p class="mt-1 text-xs text-slate-400">Minimum time between beats</p>
							</label>
						{/if}

						{#if hasDefine('BPM_EMA_ALPHA')}
							<label class="block">
								<span class="text-sm font-medium text-slate-300">BPM EMA Alpha</span>
								<input
									type="number"
									step="0.01"
									min="0.05"
									max="0.5"
									value={getValue(
										'BPM_EMA_ALPHA',
										getDefine('BPM_EMA_ALPHA')?.default_value ?? '0.15'
									)}
									disabled={$profileLoading}
									on:change={(e) => setNumber('BPM_EMA_ALPHA', (e.target as HTMLInputElement).value)}
									class="mt-1 block w-full rounded border border-slate-600 bg-slate-700 px-3 py-2 text-white"
								/>
								<p class="mt-1 text-xs text-slate-400">BPM smoothing factor (lower = smoother)</p>
							</label>
						{/if}
					</div>
				</div>
			{/if}
		{/if}
	</div>
{/if}
