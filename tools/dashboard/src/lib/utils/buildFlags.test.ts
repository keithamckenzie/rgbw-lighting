import { describe, it, expect } from 'vitest';
import { buildFlagsFromDefines, quoteBuildFlagValue } from './buildFlags';

describe('quoteBuildFlagValue', () => {
	it('should return quoted empty string for empty value', () => {
		expect(quoteBuildFlagValue('')).toBe('""');
	});

	it('should return unquoted value if no spaces', () => {
		expect(quoteBuildFlagValue('hello')).toBe('hello');
	});

	it('should quote values with spaces', () => {
		expect(quoteBuildFlagValue('hello world')).toBe('"hello world"');
	});

	it('should preserve already quoted values', () => {
		expect(quoteBuildFlagValue('"already quoted"')).toBe('"already quoted"');
		expect(quoteBuildFlagValue("'single quoted'")).toBe("'single quoted'");
	});

	it('should escape quotes in values', () => {
		expect(quoteBuildFlagValue('say "hello"')).toBe('"say \\"hello\\""');
	});
});

describe('buildFlagsFromDefines', () => {
	it('should return empty array for empty defines', () => {
		expect(buildFlagsFromDefines({})).toEqual([]);
	});

	it('should convert string defines to flags', () => {
		const defines = { DEBUG: '1', VERSION: '"1.0.0"' };
		const flags = buildFlagsFromDefines(defines);

		expect(flags).toContain('-DDEBUG=1');
		expect(flags).toContain('-DVERSION="1.0.0"');
	});

	it('should handle numeric string values', () => {
		const defines = { PIN_LED: '13', BAUD_RATE: '115200' };
		const flags = buildFlagsFromDefines(defines);

		expect(flags).toContain('-DPIN_LED=13');
		expect(flags).toContain('-DBAUD_RATE=115200');
	});

	it('should handle boolean-like string values', () => {
		const defines = { ENABLED: '1', DISABLED: '0' };
		const flags = buildFlagsFromDefines(defines);

		expect(flags).toContain('-DENABLED=1');
		expect(flags).toContain('-DDISABLED=0');
	});

	it('should handle values with spaces', () => {
		const defines = { MESSAGE: 'hello world' };
		const flags = buildFlagsFromDefines(defines);

		expect(flags).toContain('-DMESSAGE="hello world"');
	});
});
