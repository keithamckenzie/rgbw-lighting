use crate::utils::{config_schema, monorepo, path_security, pio_parser, pio_path};
use serde::{Deserialize, Serialize};
use std::fs;
use tracing::info;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AppInfo {
    pub name: String,
    pub path: String,
    pub has_config: bool,
    pub environments: Vec<pio_parser::DiscoveredEnvironment>,
    pub config_schema: config_schema::AppConfigSchema,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MonorepoInfo {
    pub path: String,
    pub pio_path: String,
    pub apps: Vec<AppInfo>,
}

/// Discovers all apps in the monorepo.
#[tauri::command]
pub fn discover_apps() -> Result<MonorepoInfo, String> {
    info!("Discovering apps");
    let monorepo_path = monorepo::find_monorepo_root()?;
    let pio = pio_path::resolve_pio_path(&monorepo_path)?;

    let apps_dir = monorepo_path.join("apps");
    if !apps_dir.exists() {
        return Err("apps/ directory not found in monorepo".to_string());
    }

    let mut apps = Vec::new();

    let entries = fs::read_dir(&apps_dir).map_err(|e| format!("Failed to read apps/: {}", e))?;

    for entry in entries {
        let entry = entry.map_err(|e| e.to_string())?;
        let path = entry.path();

        if !path.is_dir() {
            continue;
        }

        let platformio_ini = path.join("platformio.ini");
        if !platformio_ini.exists() {
            continue;
        }

        let name = path
            .file_name()
            .and_then(|n| n.to_str())
            .ok_or("Invalid app directory name")?
            .to_string();

        // Parse platformio.ini
        let environments = match pio_parser::parse_platformio_ini(&platformio_ini) {
            Ok(config) => config.environments,
            Err(e) => {
                eprintln!("Warning: Failed to parse {}/platformio.ini: {}", name, e);
                Vec::new()
            }
        };

        // Parse config.h schema
        let config_schema = config_schema::parse_config_schema(&path).unwrap_or_else(|_| {
            config_schema::AppConfigSchema {
                has_config: false,
                defines: Vec::new(),
                platform_conditional: std::collections::HashMap::new(),
            }
        });

        apps.push(AppInfo {
            name,
            path: path.to_string_lossy().to_string(),
            has_config: config_schema.has_config,
            environments,
            config_schema,
        });
    }

    // Sort apps by name
    apps.sort_by(|a, b| a.name.cmp(&b.name));

    Ok(MonorepoInfo {
        path: monorepo_path.to_string_lossy().to_string(),
        pio_path: pio.to_string_lossy().to_string(),
        apps,
    })
}

/// Gets detailed information about a specific app.
#[tauri::command]
pub fn get_app_info(app_name: String) -> Result<AppInfo, String> {
    info!(app = %app_name, "Fetching app info");
    let monorepo_path = monorepo::find_monorepo_root()?;
    let app_path = path_security::validate_app_path(&monorepo_path, &app_name)?;

    let platformio_ini = app_path.join("platformio.ini");
    let environments = pio_parser::parse_platformio_ini(&platformio_ini)
        .map(|c| c.environments)
        .unwrap_or_default();

    let config_schema = config_schema::parse_config_schema(&app_path).unwrap_or_else(|_| {
        config_schema::AppConfigSchema {
            has_config: false,
            defines: Vec::new(),
            platform_conditional: std::collections::HashMap::new(),
        }
    });

    Ok(AppInfo {
        name: app_name,
        path: app_path.to_string_lossy().to_string(),
        has_config: config_schema.has_config,
        environments,
        config_schema,
    })
}

/// Validates an app path for security.
#[tauri::command]
pub fn validate_app_path(app_name: String) -> Result<String, String> {
    let monorepo_path = monorepo::find_monorepo_root()?;
    let validated = path_security::validate_app_path(&monorepo_path, &app_name)?;
    Ok(validated.to_string_lossy().to_string())
}
