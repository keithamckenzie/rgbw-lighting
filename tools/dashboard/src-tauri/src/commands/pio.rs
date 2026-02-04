use crate::utils::{monorepo, path_security, pio_path};
use serde::{Deserialize, Serialize};
use serialport::available_ports;
use std::collections::HashMap;
use std::process::Stdio;
use tauri::{AppHandle, Emitter};
use tokio::io::{AsyncBufReadExt, BufReader};
use tokio::process::Command;
use tokio::time::{timeout, Duration};
use tracing::{info, warn};

const PIO_COMMAND_TIMEOUT_SECS: u64 = 600;

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "type")]
pub enum BuildEvent {
    #[serde(rename = "output")]
    Output { line: String },
    #[serde(rename = "error")]
    Error { message: String },
    #[serde(rename = "complete")]
    Complete { success: bool, duration_ms: u64 },
    #[serde(rename = "started")]
    Started { app_name: String, environment: String },
}

fn validate_environment_name(name: &str) -> Result<(), String> {
    if name.trim().is_empty() {
        return Err("Environment name cannot be empty".to_string());
    }
    if name.len() > 64 {
        return Err("Environment name too long (max 64 chars)".to_string());
    }
    if !name
        .chars()
        .all(|c| c.is_ascii_alphanumeric() || c == '_' || c == '-')
    {
        return Err(format!(
            "Environment name '{}' contains invalid characters. Use only letters, numbers, underscore, hyphen.",
            name
        ));
    }
    Ok(())
}

fn validate_build_flags(build_flags: &[String]) -> Result<(), String> {
    for flag in build_flags {
        if !flag.starts_with("-D") {
            return Err(format!("Invalid build flag (expected -D...): {}", flag));
        }

        let mut parts = flag[2..].splitn(2, '=');
        let name = parts.next().unwrap_or("");
        if name.is_empty() {
            return Err(format!("Invalid build flag (missing name): {}", flag));
        }
        let mut chars = name.chars();
        let first = chars.next().unwrap_or('_');
        if !(first.is_ascii_alphabetic() || first == '_')
            || !chars.all(|c| c.is_ascii_alphanumeric() || c == '_')
        {
            return Err(format!("Invalid build flag name: {}", flag));
        }

        if let Some(value) = parts.next() {
            if value.contains('\0') || value.contains('\n') || value.contains('\r') {
                return Err(format!("Build flag contains invalid characters: {}", flag));
            }
            if value
                .chars()
                .any(|c| matches!(c, '`' | '$' | ';' | '|' | '&' | '<' | '>'))
            {
                return Err(format!("Build flag contains forbidden characters: {}", flag));
            }
        }
    }
    Ok(())
}

fn validate_upload_port(port: &str) -> Result<(), String> {
    if port.trim().is_empty() {
        return Err("Upload port cannot be empty".to_string());
    }
    if port.contains('\0') || port.contains('\n') || port.contains('\r') || port.contains("..") {
        return Err("Upload port contains invalid characters".to_string());
    }

    let ports = available_ports().map_err(|e| format!("Failed to list serial ports: {}", e))?;
    if !ports.iter().any(|p| p.port_name == port) {
        return Err(format!(
            "Invalid upload port: {}. Please refresh ports and select a valid device.",
            port
        ));
    }
    Ok(())
}

fn emit_build_event(app_handle: &AppHandle, event: BuildEvent) {
    if let Err(e) = app_handle.emit("build-event", event) {
        warn!("Failed to emit build event: {}", e);
    }
}

async fn wait_with_timeout(
    child: &mut tokio::process::Child,
    timeout_duration: Duration,
) -> Result<Option<std::process::ExitStatus>, String> {
    match timeout(timeout_duration, child.wait()).await {
        Ok(status) => status
            .map(Some)
            .map_err(|e| format!("Failed to wait for PlatformIO: {}", e)),
        Err(_) => {
            let _ = child.kill().await;
            let _ = child.wait().await;
            Ok(None)
        }
    }
}

/// Runs a PlatformIO build command with streaming output.
#[tauri::command]
pub async fn run_build(
    app_handle: AppHandle,
    app_name: String,
    environment: String,
    build_flags: Vec<String>,
) -> Result<bool, String> {
    validate_environment_name(&environment)?;
    validate_build_flags(&build_flags)?;

    info!(app = %app_name, env = %environment, "Starting build");
    let monorepo_path = monorepo::find_monorepo_root()?;
    let pio_path = pio_path::resolve_pio_path(&monorepo_path)?;
    let app_path = path_security::validate_app_path(&monorepo_path, &app_name)?;

    // Emit started event
    emit_build_event(
        &app_handle,
        BuildEvent::Started {
            app_name: app_name.clone(),
            environment: environment.clone(),
        },
    );

    let start_time = std::time::Instant::now();

    let mut cmd = Command::new(&pio_path);
    cmd.arg("run")
        .arg("-e")
        .arg(&environment)
        .current_dir(&app_path)
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .env("PYTHONUNBUFFERED", "1"); // Force unbuffered Python output

    // Inject build flags via environment variable
    if !build_flags.is_empty() {
        cmd.env("PLATFORMIO_BUILD_FLAGS", build_flags.join(" "));
    }

    let mut child = cmd.spawn().map_err(|e| format!("Failed to start PlatformIO: {}", e))?;

    let stdout = child.stdout.take().ok_or("Failed to capture stdout")?;
    let stderr = child.stderr.take().ok_or("Failed to capture stderr")?;

    let app_handle_out = app_handle.clone();
    let app_handle_err = app_handle.clone();

    // Spawn stdout reader
    let stdout_task = tokio::spawn(async move {
        let reader = BufReader::new(stdout);
        let mut lines = reader.lines();
        while let Ok(Some(line)) = lines.next_line().await {
            emit_build_event(&app_handle_out, BuildEvent::Output { line });
        }
    });

    // Spawn stderr reader
    let stderr_task = tokio::spawn(async move {
        let reader = BufReader::new(stderr);
        let mut lines = reader.lines();
        while let Ok(Some(line)) = lines.next_line().await {
            emit_build_event(&app_handle_err, BuildEvent::Output { line });
        }
    });

    let timeout_duration = Duration::from_secs(PIO_COMMAND_TIMEOUT_SECS);
    let status = wait_with_timeout(&mut child, timeout_duration).await?;
    // Wait for readers to finish
    let _ = tokio::join!(stdout_task, stderr_task);

    let duration_ms = start_time.elapsed().as_millis() as u64;
    let success = match status {
        Some(status) => status.success(),
        None => {
            emit_build_event(
                &app_handle,
                BuildEvent::Error {
                    message: format!(
                        "Build timed out after {} seconds",
                        PIO_COMMAND_TIMEOUT_SECS
                    ),
                },
            );
            emit_build_event(
                &app_handle,
                BuildEvent::Complete {
                    success: false,
                    duration_ms,
                },
            );
            return Err(format!(
                "Build timed out after {} seconds",
                PIO_COMMAND_TIMEOUT_SECS
            ));
        }
    };

    // Emit completion event
    emit_build_event(
        &app_handle,
        BuildEvent::Complete {
            success,
            duration_ms,
        },
    );

    Ok(success)
}

/// Runs a PlatformIO upload command with streaming output.
#[tauri::command]
pub async fn run_upload(
    app_handle: AppHandle,
    app_name: String,
    environment: String,
    build_flags: Vec<String>,
    upload_port: Option<String>,
) -> Result<bool, String> {
    validate_environment_name(&environment)?;
    validate_build_flags(&build_flags)?;
    if let Some(ref port) = upload_port {
        validate_upload_port(port)?;
    }

    info!(app = %app_name, env = %environment, "Starting upload");
    let monorepo_path = monorepo::find_monorepo_root()?;
    let pio_path = pio_path::resolve_pio_path(&monorepo_path)?;
    let app_path = path_security::validate_app_path(&monorepo_path, &app_name)?;

    // Emit started event
    emit_build_event(
        &app_handle,
        BuildEvent::Started {
            app_name: app_name.clone(),
            environment: environment.clone(),
        },
    );

    let start_time = std::time::Instant::now();

    let mut cmd = Command::new(&pio_path);
    cmd.arg("run")
        .arg("-e")
        .arg(&environment)
        .arg("-t")
        .arg("upload")
        .current_dir(&app_path)
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .env("PYTHONUNBUFFERED", "1");

    // Inject build flags
    if !build_flags.is_empty() {
        cmd.env("PLATFORMIO_BUILD_FLAGS", build_flags.join(" "));
    }

    // Set upload port if specified
    if let Some(port) = upload_port {
        cmd.env("PLATFORMIO_UPLOAD_PORT", port);
    }

    let mut child = cmd.spawn().map_err(|e| format!("Failed to start PlatformIO: {}", e))?;

    let stdout = child.stdout.take().ok_or("Failed to capture stdout")?;
    let stderr = child.stderr.take().ok_or("Failed to capture stderr")?;

    let app_handle_out = app_handle.clone();
    let app_handle_err = app_handle.clone();

    let stdout_task = tokio::spawn(async move {
        let reader = BufReader::new(stdout);
        let mut lines = reader.lines();
        while let Ok(Some(line)) = lines.next_line().await {
            emit_build_event(&app_handle_out, BuildEvent::Output { line });
        }
    });

    let stderr_task = tokio::spawn(async move {
        let reader = BufReader::new(stderr);
        let mut lines = reader.lines();
        while let Ok(Some(line)) = lines.next_line().await {
            emit_build_event(&app_handle_err, BuildEvent::Output { line });
        }
    });

    let timeout_duration = Duration::from_secs(PIO_COMMAND_TIMEOUT_SECS);
    let status = wait_with_timeout(&mut child, timeout_duration).await?;

    let _ = tokio::join!(stdout_task, stderr_task);

    let duration_ms = start_time.elapsed().as_millis() as u64;
    let success = match status {
        Some(status) => status.success(),
        None => {
            emit_build_event(
                &app_handle,
                BuildEvent::Error {
                    message: format!(
                        "Upload timed out after {} seconds",
                        PIO_COMMAND_TIMEOUT_SECS
                    ),
                },
            );
            emit_build_event(
                &app_handle,
                BuildEvent::Complete {
                    success: false,
                    duration_ms,
                },
            );
            return Err(format!(
                "Upload timed out after {} seconds",
                PIO_COMMAND_TIMEOUT_SECS
            ));
        }
    };

    emit_build_event(
        &app_handle,
        BuildEvent::Complete {
            success,
            duration_ms,
        },
    );

    Ok(success)
}

/// Runs PlatformIO tests for an environment.
#[tauri::command]
pub async fn run_tests(
    app_handle: AppHandle,
    app_name: String,
    environment: String,
) -> Result<bool, String> {
    validate_environment_name(&environment)?;

    info!(app = %app_name, env = %environment, "Starting tests");
    let monorepo_path = monorepo::find_monorepo_root()?;
    let pio_path = pio_path::resolve_pio_path(&monorepo_path)?;
    let app_path = path_security::validate_app_path(&monorepo_path, &app_name)?;

    emit_build_event(
        &app_handle,
        BuildEvent::Started {
            app_name: app_name.clone(),
            environment: environment.clone(),
        },
    );

    let start_time = std::time::Instant::now();

    let mut cmd = Command::new(&pio_path);
    cmd.arg("test")
        .arg("-e")
        .arg(&environment)
        .current_dir(&app_path)
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .env("PYTHONUNBUFFERED", "1");

    let mut child = cmd.spawn().map_err(|e| format!("Failed to start PlatformIO: {}", e))?;

    let stdout = child.stdout.take().ok_or("Failed to capture stdout")?;
    let stderr = child.stderr.take().ok_or("Failed to capture stderr")?;

    let app_handle_out = app_handle.clone();
    let app_handle_err = app_handle.clone();

    let stdout_task = tokio::spawn(async move {
        let reader = BufReader::new(stdout);
        let mut lines = reader.lines();
        while let Ok(Some(line)) = lines.next_line().await {
            emit_build_event(&app_handle_out, BuildEvent::Output { line });
        }
    });

    let stderr_task = tokio::spawn(async move {
        let reader = BufReader::new(stderr);
        let mut lines = reader.lines();
        while let Ok(Some(line)) = lines.next_line().await {
            emit_build_event(&app_handle_err, BuildEvent::Output { line });
        }
    });

    let timeout_duration = Duration::from_secs(PIO_COMMAND_TIMEOUT_SECS);
    let status = wait_with_timeout(&mut child, timeout_duration).await?;

    let _ = tokio::join!(stdout_task, stderr_task);

    let duration_ms = start_time.elapsed().as_millis() as u64;
    let success = match status {
        Some(status) => status.success(),
        None => {
            emit_build_event(
                &app_handle,
                BuildEvent::Error {
                    message: format!(
                        "Tests timed out after {} seconds",
                        PIO_COMMAND_TIMEOUT_SECS
                    ),
                },
            );
            emit_build_event(
                &app_handle,
                BuildEvent::Complete {
                    success: false,
                    duration_ms,
                },
            );
            return Err(format!(
                "Tests timed out after {} seconds",
                PIO_COMMAND_TIMEOUT_SECS
            ));
        }
    };

    emit_build_event(
        &app_handle,
        BuildEvent::Complete {
            success,
            duration_ms,
        },
    );

    Ok(success)
}

/// Cleans build artifacts for an app.
#[tauri::command]
pub async fn clean_build(
    app_handle: AppHandle,
    app_name: String,
    environment: Option<String>,
) -> Result<bool, String> {
    info!(app = %app_name, env = ?environment, "Starting clean");
    let monorepo_path = monorepo::find_monorepo_root()?;
    let pio_path = pio_path::resolve_pio_path(&monorepo_path)?;
    let app_path = path_security::validate_app_path(&monorepo_path, &app_name)?;

    if let Some(ref env) = environment {
        validate_environment_name(env)?;
    }

    emit_build_event(
        &app_handle,
        BuildEvent::Started {
            app_name: app_name.clone(),
            environment: environment.clone().unwrap_or_default(),
        },
    );

    let start_time = std::time::Instant::now();

    let mut cmd = Command::new(&pio_path);
    cmd.arg("run")
        .arg("-t")
        .arg("clean")
        .current_dir(&app_path)
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .env("PYTHONUNBUFFERED", "1");

    if let Some(env) = environment {
        cmd.arg("-e").arg(&env);
    }

    let mut child = match cmd.spawn() {
        Ok(child) => child,
        Err(e) => {
            let duration_ms = start_time.elapsed().as_millis() as u64;
            emit_build_event(
                &app_handle,
                BuildEvent::Complete {
                    success: false,
                    duration_ms,
                },
            );
            return Err(format!("Failed to start PlatformIO: {}", e));
        }
    };

    let stdout = child.stdout.take().ok_or("Failed to capture stdout")?;
    let stderr = child.stderr.take().ok_or("Failed to capture stderr")?;

    let app_handle_out = app_handle.clone();
    let app_handle_err = app_handle.clone();

    let stdout_task = tokio::spawn(async move {
        let reader = BufReader::new(stdout);
        let mut lines = reader.lines();
        while let Ok(Some(line)) = lines.next_line().await {
            emit_build_event(&app_handle_out, BuildEvent::Output { line });
        }
    });

    let stderr_task = tokio::spawn(async move {
        let reader = BufReader::new(stderr);
        let mut lines = reader.lines();
        while let Ok(Some(line)) = lines.next_line().await {
            emit_build_event(&app_handle_err, BuildEvent::Output { line });
        }
    });

    let timeout_duration = Duration::from_secs(PIO_COMMAND_TIMEOUT_SECS);
    let status = wait_with_timeout(&mut child, timeout_duration).await?;
    let _ = tokio::join!(stdout_task, stderr_task);

    let duration_ms = start_time.elapsed().as_millis() as u64;
    let success = match status {
        Some(status) => status.success(),
        None => {
            emit_build_event(
                &app_handle,
                BuildEvent::Error {
                    message: format!(
                        "Clean timed out after {} seconds",
                        PIO_COMMAND_TIMEOUT_SECS
                    ),
                },
            );
            emit_build_event(
                &app_handle,
                BuildEvent::Complete {
                    success: false,
                    duration_ms,
                },
            );
            return Err(format!(
                "Clean timed out after {} seconds",
                PIO_COMMAND_TIMEOUT_SECS
            ));
        }
    };

    emit_build_event(
        &app_handle,
        BuildEvent::Complete {
            success,
            duration_ms,
        },
    );

    Ok(success)
}

/// Gets PlatformIO version information.
#[tauri::command]
pub async fn get_pio_version() -> Result<HashMap<String, String>, String> {
    let monorepo_path = monorepo::find_monorepo_root()?;
    let pio_path = pio_path::resolve_pio_path(&monorepo_path)?;

    info!("Fetching PlatformIO version");
    let output = Command::new(&pio_path)
        .arg("--version")
        .output()
        .await
        .map_err(|e| format!("Failed to run PlatformIO: {}", e))?;

    let version = String::from_utf8_lossy(&output.stdout).trim().to_string();

    let mut info = HashMap::new();
    info.insert("version".to_string(), version);
    info.insert("path".to_string(), pio_path.to_string_lossy().to_string());

    Ok(info)
}
