<script lang="ts">
	import type { AppConfigSchema, ConfigDefine, DiscoveredEnvironment, PinPurpose } from '$lib/types/config';
	import { configState, updateDefine, profileLoading } from '$lib/stores/config';
	import { getPlatformFromEnvironment } from '$lib/types/config';
	import { getPlatformKey } from '$lib/utils/platform';
	import PinSelector from './PinSelector.svelte';

	export let schema: AppConfigSchema;
	export let environment: DiscoveredEnvironment;

	$: platform = getPlatformFromEnvironment(environment);
	$: platformKey = getPlatformKey(platform);

	// Combine global defines with platform-specific ones
	$: visibleDefines = [
		...schema.defines,
		...(schema.platform_conditional[platformKey] || []),
		...(schema.platform_conditional[platform] || [])
	];

	function getValue(name: string, defaultValue: string): string {
		return $configState.defines[name] ?? defaultValue;
	}

	function handleChange(define: ConfigDefine, value: string) {
		updateDefine(define.name, value);
	}

	function handleSelectChange(define: ConfigDefine, event: Event) {
		const target = event.target;
		if (!(target instanceof HTMLSelectElement)) return;
		handleChange(define, target.value);
	}

	function inferPinPurpose(define: ConfigDefine): PinPurpose {
		const context = `${define.name} ${define.description ?? ''} ${define.default_value ?? ''}`.toUpperCase();

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

</script>

<div class="space-y-4">
	{#if visibleDefines.length === 0}
		<p class="text-slate-400">No configurable options for this app.</p>
	{:else}
		{#each visibleDefines as define (define.name)}
			<div class="rounded-lg border border-slate-700 bg-slate-800 p-4">
				{#if define.value_type === 'pin'}
					<PinSelector
						label={define.name}
						value={getValue(define.name, define.default_value)}
						purpose={inferPinPurpose(define)}
						{platform}
						module={environment.board ?? undefined}
						disabled={$profileLoading}
						on:change={(e) => handleChange(define, String(e.detail))}
					/>
				{:else if define.value_type === 'enum' && define.enum_values}
					<label class="block">
						<span class="text-sm font-medium text-slate-300">{define.name}</span>
						<select
							class="mt-1 block w-full rounded border border-slate-600 bg-slate-700 px-3 py-2 text-white focus:outline-none focus:ring-2 focus:ring-primary-500"
							value={getValue(define.name, define.default_value)}
							disabled={$profileLoading}
							on:change={(e) => handleSelectChange(define, e)}
						>
							{#each define.enum_values as opt}
								<option value={opt.value}>{opt.label} ({opt.value})</option>
							{/each}
						</select>
					</label>
				{:else if define.value_type === 'boolean'}
					<label class="flex items-center gap-3">
						<input
							type="checkbox"
							checked={getValue(define.name, define.default_value) === '1' ||
								getValue(define.name, define.default_value) === 'true'}
							disabled={$profileLoading}
							on:change={(e) => handleChange(define, (e.target as HTMLInputElement).checked ? '1' : '0')}
							class="h-5 w-5 rounded border-slate-600 bg-slate-700 text-primary-500 focus:ring-primary-500"
						/>
						<span class="font-medium text-slate-300">{define.name}</span>
					</label>
				{:else if define.value_type === 'float'}
					<label class="block">
						<span class="text-sm font-medium text-slate-300">{define.name}</span>
						<input
							type="number"
							step="0.01"
							value={getValue(define.name, define.default_value)}
							disabled={$profileLoading}
							on:change={(e) => handleChange(define, (e.target as HTMLInputElement).value)}
							class="mt-1 block w-full rounded border border-slate-600 bg-slate-700 px-3 py-2 text-white focus:outline-none focus:ring-2 focus:ring-primary-500"
						/>
					</label>
				{:else if define.value_type === 'integer'}
					<label class="block">
						<span class="text-sm font-medium text-slate-300">{define.name}</span>
						<input
							type="number"
							step="1"
							value={getValue(define.name, define.default_value)}
							disabled={$profileLoading}
							on:change={(e) => handleChange(define, (e.target as HTMLInputElement).value)}
							class="mt-1 block w-full rounded border border-slate-600 bg-slate-700 px-3 py-2 text-white focus:outline-none focus:ring-2 focus:ring-primary-500"
						/>
					</label>
				{:else}
					<label class="block">
						<span class="text-sm font-medium text-slate-300">{define.name}</span>
						<input
							type="text"
							value={getValue(define.name, define.default_value)}
							disabled={$profileLoading}
							on:change={(e) => handleChange(define, (e.target as HTMLInputElement).value)}
							class="mt-1 block w-full rounded border border-slate-600 bg-slate-700 px-3 py-2 text-white focus:outline-none focus:ring-2 focus:ring-primary-500"
						/>
					</label>
				{/if}

				{#if define.description}
					<p class="mt-2 text-sm text-slate-400">{define.description}</p>
				{/if}

				{#if define.platform}
					<span class="mt-2 inline-block rounded bg-slate-700 px-2 py-0.5 text-xs text-slate-400">
						{define.platform} only
					</span>
				{/if}
			</div>
		{/each}
	{/if}
</div>
