export { serialState, serialBuffers, VALID_BAUD_RATES } from './state';
export type { SerialBuffer, SerialConnection, SerialState } from './state';
export { refreshPorts } from './ports';
export {
	openSerialPort,
	closeSerialPort,
	writeSerial,
	handleSerialEvent,
	clearSerialBuffer,
	setActiveConnection
} from './connections';
export { acquirePortForUpload, releaseUploadLock } from './upload';
