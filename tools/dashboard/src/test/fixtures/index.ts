import type {
	AppInfo,
	MonorepoInfo,
	DiscoveredEnvironment,
	AppConfigSchema,
	ConfigDefine,
	BuildEvent,
	SerialEvent,
	PortInfo,
	SavedProfile
} from '$lib/types/config';

// Environments
export const mockEsp32Env: DiscoveredEnvironment = {
	name: 'esp32',
	extends: null,
	platform: 'espressif32',
	board: 'esp32dev',
	framework: 'arduino',
	build_flags: ['-DCORE_DEBUG_LEVEL=5'],
	lib_deps: ['NeoPixelBus'],
	is_hardware_target: true,
	can_upload: true
};

export const mockEsp8266Env: DiscoveredEnvironment = {
	name: 'esp8266',
	extends: null,
	platform: 'espressif8266',
	board: 'nodemcuv2',
	framework: 'arduino',
	build_flags: [],
	lib_deps: [],
	is_hardware_target: true,
	can_upload: true
};

export const mockNativeEnv: DiscoveredEnvironment = {
	name: 'native',
	extends: null,
	platform: 'native',
	board: null,
	framework: null,
	build_flags: [],
	lib_deps: [],
	is_hardware_target: false,
	can_upload: false
};

// Config defines
export const mockConfigDefine: ConfigDefine = {
	name: 'PANEL_WIDTH',
	default_value: '24',
	value_type: 'integer',
	enum_values: null,
	platform: null,
	description: 'Number of LEDs in the horizontal direction'
};

export const mockEnumDefine: ConfigDefine = {
	name: 'STRIP_TYPE',
	default_value: '0',
	value_type: 'enum',
	enum_values: [
		{ value: '0', label: 'SK6812 RGBW' },
		{ value: '1', label: 'WS2815B RGB' }
	],
	platform: null,
	description: 'LED strip type'
};

export const mockBoolDefine: ConfigDefine = {
	name: 'ENABLE_WIFI',
	default_value: '1',
	value_type: 'boolean',
	enum_values: null,
	platform: 'ESP32',
	description: 'Enable WiFi connectivity'
};

export const mockPinDefine: ConfigDefine = {
	name: 'PIN_LED_DATA',
	default_value: '16',
	value_type: 'pin',
	enum_values: null,
	platform: null,
	description: 'Data pin for LED strip'
};

// Config schema
export const mockConfigSchema: AppConfigSchema = {
	has_config: true,
	defines: [mockConfigDefine, mockEnumDefine, mockBoolDefine],
	platform_conditional: {
		ESP32: [mockPinDefine]
	}
};

// App info
export const mockAppInfo: AppInfo = {
	name: 'led-panel',
	path: '/home/user/projects/rgbw-lighting/apps/led-panel',
	has_config: true,
	environments: [mockEsp32Env, mockEsp8266Env, mockNativeEnv],
	config_schema: mockConfigSchema
};

// Monorepo info
export const mockMonorepoInfo: MonorepoInfo = {
	path: '/home/user/projects/rgbw-lighting',
	pio_path: '/usr/local/bin/pio',
	apps: [mockAppInfo]
};

// Build events
export const mockBuildStarted: BuildEvent = {
	type: 'started',
	app_name: 'led-panel',
	environment: 'esp32'
};

export const mockBuildOutput: BuildEvent = {
	type: 'output',
	line: 'Compiling .pio/build/esp32/src/main.cpp.o'
};

export const mockBuildError: BuildEvent = {
	type: 'error',
	message: 'src/main.cpp:42: error: expected ; before } token'
};

export const mockBuildComplete: BuildEvent = {
	type: 'complete',
	success: true,
	duration_ms: 12345
};

export const mockBuildFailed: BuildEvent = {
	type: 'complete',
	success: false,
	duration_ms: 5000
};

// Serial events
export const mockSerialData: SerialEvent = {
	type: 'data',
	connection_id: 'conn-1',
	text: 'Hello from ESP32\n'
};

export const mockSerialError: SerialEvent = {
	type: 'error',
	connection_id: 'conn-1',
	message: 'Port disconnected'
};

export const mockSerialClosed: SerialEvent = {
	type: 'closed',
	connection_id: 'conn-1'
};

// Port info
export const mockPort: PortInfo = {
	path: '/dev/ttyUSB0',
	port_type: 'USB',
	manufacturer: 'Silicon Labs',
	product: 'CP2102',
	serial_number: '001234',
	vid: 0x10c4,
	pid: 0xea60
};

export const mockPort2: PortInfo = {
	path: '/dev/ttyACM0',
	port_type: 'USB',
	manufacturer: 'Espressif',
	product: 'ESP32-S3',
	serial_number: null,
	vid: 0x303a,
	pid: 0x1001
};

// Saved profile
export const mockSavedProfile: SavedProfile = {
	name: 'test-profile',
	app_name: 'led-panel',
	environment: 'esp32',
	defines: {
		PANEL_WIDTH: '24',
		PANEL_HEIGHT: '36',
		STRIP_TYPE: '0'
	},
	created_at: '2025-01-01T00:00:00Z',
	updated_at: '2025-01-01T00:00:00Z'
};
