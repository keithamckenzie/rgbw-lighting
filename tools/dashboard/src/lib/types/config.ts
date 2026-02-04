// Type definitions matching the Rust backend

export interface PortInfo {
	path: string;
	port_type: string;
	manufacturer: string | null;
	product: string | null;
	serial_number: string | null;
	vid: number | null;
	pid: number | null;
}

export interface DiscoveredEnvironment {
	name: string;
	extends: string | null;
	platform: string;
	board: string | null;
	framework: string | null;
	build_flags: string[];
	lib_deps: string[];
	is_hardware_target: boolean;
	can_upload: boolean;
}

export interface EnumValue {
	value: string;
	label: string;
}

export type ValueType = 'integer' | 'float' | 'boolean' | 'string' | 'enum' | 'pin';

export interface ConfigDefine {
	name: string;
	default_value: string;
	value_type: ValueType;
	enum_values: EnumValue[] | null;
	platform: string | null;
	description: string | null;
}

export interface AppConfigSchema {
	has_config: boolean;
	defines: ConfigDefine[];
	platform_conditional: Record<string, ConfigDefine[]>;
}

export interface AppInfo {
	name: string;
	path: string;
	has_config: boolean;
	environments: DiscoveredEnvironment[];
	config_schema: AppConfigSchema;
}

export interface MonorepoInfo {
	path: string;
	pio_path: string;
	apps: AppInfo[];
}

export interface SavedProfile {
	name: string;
	app_name: string;
	environment: string;
	defines: Record<string, string>;
	created_at: string;
	updated_at: string;
}

export interface PinValidation {
	valid: boolean;
	severity: 'ok' | 'info' | 'warning' | 'error';
	message: string;
}

export type PinPurpose = 'output' | 'input' | 'adc' | 'i2c' | 'spi' | 'i2s' | 'pwm';

export interface BuildEvent {
	type: 'output' | 'error' | 'complete' | 'started';
	line?: string;
	message?: string;
	success?: boolean;
	duration_ms?: number;
	app_name?: string;
	environment?: string;
}

export interface SerialEvent {
	type: 'data' | 'error' | 'closed';
	connection_id: string;
	text?: string;
	message?: string;
}

// App configuration state
export interface AppConfigState {
	appName: string;
	environment: string;
	defines: Record<string, string>;
	isDirty: boolean;
}

// Platform detection from environment
export function getPlatformFromEnvironment(env: DiscoveredEnvironment): string {
	if (env.platform.includes('espressif32') || env.platform === 'esp32') {
		return 'esp32';
	}
	if (env.platform.includes('espressif8266') || env.platform === 'esp8266') {
		return 'esp8266';
	}
	if (env.platform.includes('atmelavr') || env.platform === 'avr') {
		return 'avr';
	}
	return env.platform;
}
