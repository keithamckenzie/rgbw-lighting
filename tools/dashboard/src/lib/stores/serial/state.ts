import { writable } from 'svelte/store';
import type { PortInfo } from '$lib/types/config';

export interface SerialConnection {
  connectionId: string;
  portPath: string;
  baudRate: number;
}

export interface SerialBuffer {
  lines: string[];
  partialLine: string;
}

export interface SerialState {
  availablePorts: PortInfo[];
  connections: Map<string, SerialConnection>;
  activeConnectionId: string | null;
}

// Keep in sync with the backend baud validation list (src-tauri/src/commands/serial.rs).
export const VALID_BAUD_RATES = [
  300, 1200, 2400, 4800, 9600, 14400, 19200, 38400, 57600, 115200, 230400, 460800, 921600
] as const;

export const serialState = writable<SerialState>({
  availablePorts: [],
  connections: new Map(),
  activeConnectionId: null
});

// Store serial output separately so high-frequency line updates don't force full state re-renders.
export const serialBuffers = writable<Map<string, SerialBuffer>>(new Map());
