use serde::{Deserialize, Serialize};
use serialport::{SerialPort, SerialPortType};
use std::collections::HashMap;
use std::io::{Read, Write};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc;
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::{Duration, Instant};
use tauri::{AppHandle, Emitter, State};
use tracing::{info, warn};
use uuid::Uuid;

const VALID_BAUD_RATES: &[u32] = &[
    300, 1200, 2400, 4800, 9600, 14400, 19200, 38400, 57600, 115200, 230400, 460800, 921600,
];
const SERIAL_EMIT_INTERVAL_MS: u64 = 10;
const SERIAL_BUFFER_MAX_BYTES: usize = 4096;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PortInfo {
    pub path: String,
    pub port_type: String,
    pub manufacturer: Option<String>,
    pub product: Option<String>,
    pub serial_number: Option<String>,
    pub vid: Option<u16>,
    pub pid: Option<u16>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "type")]
pub enum SerialEvent {
    #[serde(rename = "data")]
    Data { connection_id: String, text: String },
    #[serde(rename = "error")]
    Error {
        connection_id: String,
        message: String,
    },
    #[serde(rename = "closed")]
    Closed { connection_id: String },
}

enum SerialCommand {
    Write(Vec<u8>),
    Shutdown,
}

struct SerialConnection {
    port_path: String,
    command_tx: mpsc::Sender<SerialCommand>,
    thread_handle: Option<thread::JoinHandle<()>>,
    alive: Arc<AtomicBool>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub enum PortLock {
    Monitor(String),   // connection_id
    Upload(u64),       // timestamp
}

pub struct SerialState {
    connections: Mutex<HashMap<String, SerialConnection>>,
    port_locks: Mutex<HashMap<String, PortLock>>,
}

impl Default for SerialState {
    fn default() -> Self {
        Self {
            connections: Mutex::new(HashMap::new()),
            port_locks: Mutex::new(HashMap::new()),
        }
    }
}

fn validate_port_path(port_path: &str) -> Result<(), String> {
    if port_path.trim().is_empty() {
        return Err("Port path cannot be empty".to_string());
    }
    if port_path.contains('\0') || port_path.contains("..") {
        return Err("Port path contains invalid characters".to_string());
    }

    let ports = serialport::available_ports()
        .map_err(|e| format!("Failed to list ports: {}", e))?;
    if !ports.iter().any(|p| p.port_name == port_path) {
        return Err(format!(
            "Invalid serial port path: {}. Please refresh ports and select a valid device.",
            port_path
        ));
    }

    Ok(())
}

fn validate_baud_rate(baud_rate: u32) -> Result<(), String> {
    if !VALID_BAUD_RATES.contains(&baud_rate) {
        return Err(format!(
            "Invalid baud rate: {}. Valid rates: {:?}",
            baud_rate, VALID_BAUD_RATES
        ));
    }
    Ok(())
}

fn lock_recover<'a, T>(lock: &'a Mutex<T>, label: &str) -> Result<std::sync::MutexGuard<'a, T>, String> {
    match lock.lock() {
        Ok(guard) => Ok(guard),
        Err(poisoned) => {
            warn!("Recovered poisoned lock: {}", label);
            Ok(poisoned.into_inner())
        }
    }
}

fn emit_serial_event(app_handle: &AppHandle, event: SerialEvent) {
    if let Err(e) = app_handle.emit("serial-event", event) {
        warn!("Failed to emit serial event: {}", e);
    }
}

/// Lists available serial ports.
#[tauri::command]
pub fn list_serial_ports() -> Result<Vec<PortInfo>, String> {
    let ports = serialport::available_ports().map_err(|e| format!("Failed to list ports: {}", e))?;

    let port_infos: Vec<PortInfo> = ports
        .into_iter()
        .map(|p| {
            let (port_type, manufacturer, product, serial_number, vid, pid) = match &p.port_type {
                SerialPortType::UsbPort(info) => (
                    "USB".to_string(),
                    info.manufacturer.clone(),
                    info.product.clone(),
                    info.serial_number.clone(),
                    Some(info.vid),
                    Some(info.pid),
                ),
                SerialPortType::BluetoothPort => {
                    ("Bluetooth".to_string(), None, None, None, None, None)
                }
                SerialPortType::PciPort => ("PCI".to_string(), None, None, None, None, None),
                SerialPortType::Unknown => ("Unknown".to_string(), None, None, None, None, None),
            };

            PortInfo {
                path: p.port_name,
                port_type,
                manufacturer,
                product,
                serial_number,
                vid,
                pid,
            }
        })
        .collect();

    Ok(port_infos)
}

/// Opens a serial connection.
#[tauri::command]
pub fn open_serial(
    app_handle: AppHandle,
    state: State<'_, SerialState>,
    port_path: String,
    baud_rate: u32,
) -> Result<String, String> {
    info!(port = %port_path, baud = baud_rate, "Opening serial port");
    validate_port_path(&port_path)?;
    validate_baud_rate(baud_rate)?;

    // Check if port is locked for upload
    {
        let locks = lock_recover(&state.port_locks, "port_locks")?;
        if let Some(PortLock::Upload(_)) = locks.get(&port_path) {
            return Err("Port is currently locked for upload".to_string());
        }
    }

    // Check if already connected
    {
        let connections = lock_recover(&state.connections, "connections")?;
        if connections.values().any(|c| c.port_path == port_path) {
            return Err(format!("Port {} is already open", port_path));
        }
    }

    let connection_id = Uuid::new_v4().to_string();

    // Open the serial port
    let port = serialport::new(&port_path, baud_rate)
        .timeout(Duration::from_millis(100))
        .open()
        .map_err(|e| format!("Failed to open port: {}", e))?;

    let (command_tx, command_rx) = mpsc::channel::<SerialCommand>();

    // Clone values for the thread
    let port_path_clone = port_path.clone();
    let connection_id_clone = connection_id.clone();
    let app_handle_clone = app_handle.clone();

    // Spawn reader thread
    let alive = Arc::new(AtomicBool::new(true));
    let alive_thread = Arc::clone(&alive);
    let thread_handle = thread::spawn(move || {
        serial_reader_thread(
            port,
            command_rx,
            app_handle_clone,
            connection_id_clone,
            port_path_clone,
            alive_thread,
        );
    });

    // Store connection
    {
        let mut connections = lock_recover(&state.connections, "connections")?;
        connections.insert(
            connection_id.clone(),
            SerialConnection {
                port_path: port_path.clone(),
                command_tx,
                thread_handle: Some(thread_handle),
                alive,
            },
        );
    }

    // Set port lock after connection insertion; connection_id isn't exposed until open_serial returns.
    {
        let mut locks = lock_recover(&state.port_locks, "port_locks")?;
        locks.insert(port_path, PortLock::Monitor(connection_id.clone()));
    }

    Ok(connection_id)
}

fn serial_reader_thread(
    mut port: Box<dyn SerialPort>,
    command_rx: mpsc::Receiver<SerialCommand>,
    app_handle: AppHandle,
    connection_id: String,
    _port_path: String,
    alive: Arc<AtomicBool>,
) {
    let mut buf = [0u8; 1024];
    let mut pending = String::new();
    let mut last_emit = Instant::now();
    let emit_interval = Duration::from_millis(SERIAL_EMIT_INTERVAL_MS);

    loop {
        // Check for commands
        match command_rx.try_recv() {
            Ok(SerialCommand::Shutdown) => {
                if !pending.is_empty() {
                    if alive.load(Ordering::Relaxed) {
                        emit_serial_event(
                            &app_handle,
                            SerialEvent::Data {
                                connection_id: connection_id.clone(),
                                text: std::mem::take(&mut pending),
                            },
                        );
                    }
                }
                break;
            }
            Ok(SerialCommand::Write(data)) => {
                if let Err(e) = port.write_all(&data) {
                    emit_serial_event(
                        &app_handle,
                        SerialEvent::Error {
                            connection_id: connection_id.clone(),
                            message: format!("Write error: {}", e),
                        },
                    );
                }
            }
            Err(mpsc::TryRecvError::Empty) => {}
            Err(mpsc::TryRecvError::Disconnected) => {
                break;
            }
        }

        if !alive.load(Ordering::Relaxed) {
            break;
        }

        // Read available data
        match port.read(&mut buf) {
            Ok(n) if n > 0 => {
                let text = String::from_utf8_lossy(&buf[..n]);
                pending.push_str(&text);

                let should_emit = pending.len() > SERIAL_BUFFER_MAX_BYTES
                    || last_emit.elapsed() >= emit_interval;
                if should_emit {
                    if alive.load(Ordering::Relaxed) {
                        emit_serial_event(
                            &app_handle,
                            SerialEvent::Data {
                                connection_id: connection_id.clone(),
                                text: std::mem::take(&mut pending),
                            },
                        );
                    }
                    last_emit = Instant::now();
                }
            }
            Err(ref e) if e.kind() == std::io::ErrorKind::TimedOut => {
                // Normal timeout, continue
            }
            Err(e) => {
                emit_serial_event(
                    &app_handle,
                    SerialEvent::Error {
                        connection_id: connection_id.clone(),
                        message: format!("Read error: {}", e),
                    },
                );
                break;
            }
            _ => {}
        }

        if !pending.is_empty() && last_emit.elapsed() >= emit_interval {
            if alive.load(Ordering::Relaxed) {
                emit_serial_event(
                    &app_handle,
                    SerialEvent::Data {
                        connection_id: connection_id.clone(),
                        text: std::mem::take(&mut pending),
                    },
                );
            }
            last_emit = Instant::now();
        }
    }

    // Emit closed event
    emit_serial_event(
        &app_handle,
        SerialEvent::Closed {
            connection_id: connection_id.clone(),
        },
    );
}

/// Writes data to a serial connection.
#[tauri::command]
pub fn write_serial(
    state: State<'_, SerialState>,
    connection_id: String,
    data: String,
) -> Result<(), String> {
    let connections = lock_recover(&state.connections, "connections")?;

    let conn = connections
        .get(&connection_id)
        .ok_or("Connection not found")?;

    conn.command_tx
        .send(SerialCommand::Write(data.into_bytes()))
        .map_err(|e| format!("Failed to send data: {}", e))?;

    Ok(())
}

/// Closes a serial connection.
#[tauri::command]
pub fn close_serial(state: State<'_, SerialState>, connection_id: String) -> Result<(), String> {
    info!(connection_id = %connection_id, "Closing serial connection");
    let conn = {
        let mut connections = lock_recover(&state.connections, "connections")?;
        if let Some(conn) = connections.get_mut(&connection_id) {
            // Mark dead before removal to prevent dangling emits in the reader thread.
            conn.alive.store(false, Ordering::Relaxed);
        }
        connections.remove(&connection_id)
    };

    if let Some(mut conn) = conn {
        let _ = conn.command_tx.send(SerialCommand::Shutdown);

        if let Some(handle) = conn.thread_handle.take() {
            if let Err(err) = handle.join() {
                warn!("Serial reader thread join failed: {:?}", err);
            }
        }

        let mut locks = lock_recover(&state.port_locks, "port_locks")?;
        locks.retain(|_, v| !matches!(v, PortLock::Monitor(ref id) if id == &connection_id));
    }

    Ok(())
}

/// Acquires a port lock for upload (closes any monitor connection first).
#[tauri::command]
pub fn acquire_port_for_upload(
    state: State<'_, SerialState>,
    port_path: String,
) -> Result<(), String> {
    validate_port_path(&port_path)?;

    let (_conn_id, conn) = {
        // Hold both locks while checking and inserting to avoid TOCTOU races.
        let mut connections = lock_recover(&state.connections, "connections")?;
        let mut locks = lock_recover(&state.port_locks, "port_locks")?;

        let conn_id = match locks.get(&port_path) {
            Some(PortLock::Monitor(conn_id)) => Some(conn_id.clone()),
            _ => None,
        };

        let now = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .map(|d| d.as_secs())
            .unwrap_or(0);
        locks.insert(port_path.clone(), PortLock::Upload(now));

        let conn = match conn_id.as_ref() {
            Some(id) => {
                if let Some(conn) = connections.get_mut(id) {
                    // Mark dead before removal to avoid emitting for a now-closed connection.
                    conn.alive.store(false, Ordering::Relaxed);
                }
                connections.remove(id)
            }
            None => None,
        };

        (conn_id, conn)
    };

    if let Some(mut conn) = conn {
        let _ = conn.command_tx.send(SerialCommand::Shutdown);
        if let Some(handle) = conn.thread_handle.take() {
            if let Err(err) = handle.join() {
                warn!("Serial reader thread join failed: {:?}", err);
            }
        }
    }

    Ok(())
}

/// Releases a port lock after upload.
#[tauri::command]
pub fn release_upload_lock(state: State<'_, SerialState>, port_path: String) -> Result<(), String> {
    let mut locks = lock_recover(&state.port_locks, "port_locks")?;

    if matches!(locks.get(&port_path), Some(PortLock::Upload(_))) {
        locks.remove(&port_path);
    }

    Ok(())
}

/// Gets the current lock status for a port.
#[tauri::command]
pub fn get_port_lock_status(
    state: State<'_, SerialState>,
    port_path: String,
) -> Result<Option<String>, String> {
    let connections = lock_recover(&state.connections, "connections")?;
    let mut locks = lock_recover(&state.port_locks, "port_locks")?;

    match locks.get(&port_path) {
        Some(PortLock::Monitor(conn_id)) => {
            if connections.contains_key(conn_id) {
                Ok(Some(format!("monitor:{}", conn_id)))
            } else {
                locks.remove(&port_path);
                Ok(None)
            }
        }
        Some(PortLock::Upload(_)) => Ok(Some("upload".to_string())),
        None => Ok(None),
    }
}

/// Gets all active serial connections.
#[tauri::command]
pub fn list_serial_connections(state: State<'_, SerialState>) -> Result<Vec<String>, String> {
    let connections = lock_recover(&state.connections, "connections")?;
    Ok(connections.keys().cloned().collect())
}
