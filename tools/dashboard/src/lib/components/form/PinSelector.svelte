<script lang="ts">
	import { createEventDispatcher, onDestroy } from 'svelte';
	import type { PinValidation, PinPurpose } from '$lib/types/config';
	import { validatePin, getSafePins, getSeverityClass, getSeverityIcon } from '$lib/validation/pins';
	import { debounce } from '$lib/utils/debounce';

	export let value: number | string;
	export let purpose: PinPurpose = 'output';
	export let platform = 'esp32';
	export let module: string | undefined = undefined;
	export let label = 'Pin';
	export let disabled = false;

	const dispatch = createEventDispatcher<{ change: number | string }>();

	let validation: PinValidation | null = null;
	let safePins: number[] = [];
	let isValidating = false;
	let validationRequestId = 0;
	let safePinsRequestId = 0;
	let safePinsKey = '';

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

	onDestroy(() => {
		debouncedValidate.cancel();
	});

	// Reactive validation
	$: {
		const hasValue = value !== undefined && value !== null && value !== '';
		if (hasValue) {
			debouncedValidate(value, purpose, platform, module);
		} else {
			debouncedValidate.cancel();
			validationRequestId += 1;
			isValidating = false;
			validation = null;
		}
	}

	$: {
		const nextKey = `${platform}:${module ?? ''}`;
		if (nextKey !== safePinsKey) {
			safePinsKey = nextKey;
			void refreshSafePins(platform, module);
		}
	}

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

		value = newValue;
		dispatch('change', newValue);
	}

	function handleSelectChange(event: Event) {
		const target = event.target;
		if (!(target instanceof HTMLSelectElement)) return;
		const v = target.value;
		if (!v) return;
		const newValue = parseInt(v, 10);
		value = newValue;
		dispatch('change', newValue);
	}
</script>

<div class="flex flex-col gap-1">
	<label class="text-sm font-medium text-slate-300">
		{label}
	</label>

	<div class="flex items-center gap-2">
		<div class="relative flex-1">
			<input
				type="text"
				{value}
				on:input={handleInput}
				disabled={disabled || isValidating}
				class="w-full rounded border bg-slate-700 px-3 py-2 text-white placeholder-slate-400 focus:outline-none focus:ring-2 focus:ring-primary-500"
				class:border-slate-600={!validation || validation.severity === 'ok'}
				class:border-red-500={validation?.severity === 'error'}
				class:border-yellow-500={validation?.severity === 'warning'}
				class:border-blue-500={validation?.severity === 'info'}
				placeholder="GPIO number"
			/>

			{#if isValidating}
				<span class="absolute right-3 top-1/2 -translate-y-1/2 text-slate-400">...</span>
			{:else if validation}
				<span
					class="absolute right-3 top-1/2 -translate-y-1/2 {getSeverityClass(validation.severity)}"
				>
					{getSeverityIcon(validation.severity)}
				</span>
			{/if}
		</div>

		{#if safePins.length > 0}
			<select
				class="rounded border border-slate-600 bg-slate-700 px-2 py-2 text-sm text-white"
				on:change={handleSelectChange}
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
		<p class="text-sm {getSeverityClass(validation.severity)}">
			{validation.message}
		</p>
	{/if}
</div>
