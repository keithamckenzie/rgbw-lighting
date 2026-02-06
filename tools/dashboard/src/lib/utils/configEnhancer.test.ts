import { describe, it, expect } from 'vitest';
import { enhanceDefine, enhanceDefines, groupDefines, filterAdvanced } from './configEnhancer';
import type { ConfigDefine, EnhancedDefine } from '$lib/types/config';

// Simple test define without metadata in the registry
const makeDefine = (overrides: Partial<ConfigDefine> = {}): ConfigDefine => ({
	name: 'TEST_FIELD',
	default_value: '42',
	value_type: 'integer',
	enum_values: null,
	platform: null,
	description: null,
	...overrides
});

describe('enhanceDefine', () => {
	it('returns enhanced define with default label when no metadata', () => {
		const define = makeDefine({ name: 'MY_CUSTOM_FIELD' });
		const enhanced = enhanceDefine(define);

		// Should generate human-readable label from name
		expect(enhanced.label).toBeTruthy();
		expect(enhanced.group).toBe('Advanced'); // Default group for unknown fields
		expect(enhanced.advanced).toBe(true); // Advanced group defaults to advanced
		expect(enhanced.readOnly).toBe(false);
	});

	it('preserves original define properties', () => {
		const define = makeDefine({ name: 'FOO', default_value: '99', value_type: 'float' });
		const enhanced = enhanceDefine(define);

		expect(enhanced.name).toBe('FOO');
		expect(enhanced.default_value).toBe('99');
		expect(enhanced.value_type).toBe('float');
	});

	it('applies metadata for known fields like PANEL_WIDTH', () => {
		const define = makeDefine({ name: 'PANEL_WIDTH' });
		const enhanced = enhanceDefine(define);

		expect(enhanced.label).toBe('Panel Width');
		expect(enhanced.group).toBe('Display Settings');
		expect(enhanced.min).toBe(1);
		expect(enhanced.max).toBe(500);
	});

	it('converts enumDescriptions to enum_values when backend has none', () => {
		const define = makeDefine({ name: 'STRIP_TYPE', value_type: 'integer' });
		const enhanced = enhanceDefine(define);

		// STRIP_TYPE has enumDescriptions in metadata
		expect(enhanced.value_type).toBe('enum');
		expect(enhanced.enum_values).toBeTruthy();
		expect(enhanced.enum_values!.length).toBeGreaterThan(0);
	});

	it('preserves backend enum_values when present', () => {
		const define = makeDefine({
			name: 'STRIP_TYPE',
			value_type: 'enum',
			enum_values: [{ value: '0', label: 'Custom Label' }]
		});
		const enhanced = enhanceDefine(define);

		expect(enhanced.enum_values).toEqual([{ value: '0', label: 'Custom Label' }]);
	});
});

describe('enhanceDefines', () => {
	it('enhances an array of defines', () => {
		const defines = [
			makeDefine({ name: 'PANEL_WIDTH' }),
			makeDefine({ name: 'PANEL_HEIGHT' })
		];
		const enhanced = enhanceDefines(defines);

		expect(enhanced).toHaveLength(2);
		expect(enhanced[0].label).toBe('Panel Width');
		expect(enhanced[1].label).toBe('Panel Height');
	});
});

describe('groupDefines', () => {
	it('groups defines by their group property', () => {
		const defines: EnhancedDefine[] = [
			{ ...makeDefine({ name: 'PANEL_WIDTH' }), label: 'Panel Width', group: 'Display Settings', advanced: false, readOnly: false },
			{ ...makeDefine({ name: 'PIN_LED' }), label: 'LED Pin', group: 'Pin Assignments', advanced: false, readOnly: false },
			{ ...makeDefine({ name: 'PANEL_HEIGHT' }), label: 'Panel Height', group: 'Display Settings', advanced: false, readOnly: false }
		];

		const groups = groupDefines(defines);

		expect(groups.get('Display Settings')).toHaveLength(2);
		expect(groups.get('Pin Assignments')).toHaveLength(1);
	});

	it('removes empty groups', () => {
		const defines: EnhancedDefine[] = [
			{ ...makeDefine({ name: 'FOO' }), label: 'Foo', group: 'Advanced', advanced: true, readOnly: false }
		];

		const groups = groupDefines(defines);

		// Only Advanced should exist, other standard groups should be removed
		expect(groups.has('Display Settings')).toBe(false);
		expect(groups.has('Advanced')).toBe(true);
	});
});

describe('filterAdvanced', () => {
	const defines: EnhancedDefine[] = [
		{ ...makeDefine({ name: 'BASIC' }), label: 'Basic', group: 'Display Settings', advanced: false, readOnly: false },
		{ ...makeDefine({ name: 'ADV' }), label: 'Advanced', group: 'Advanced', advanced: true, readOnly: false }
	];

	it('shows all defines when showAdvanced is true', () => {
		const result = filterAdvanced(defines, true);
		expect(result).toHaveLength(2);
	});

	it('hides advanced defines when showAdvanced is false', () => {
		const result = filterAdvanced(defines, false);
		expect(result).toHaveLength(1);
		expect(result[0].name).toBe('BASIC');
	});
});
