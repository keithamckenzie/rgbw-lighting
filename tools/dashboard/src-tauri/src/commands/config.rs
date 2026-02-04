use crate::utils::{
    pin_validator::{self, Module, PinPurpose, PinValidation, Platform},
    profile_paths,
};
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::fs;
use std::path::PathBuf;
use tracing::info;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SavedProfile {
    pub name: String,
    pub app_name: String,
    pub environment: String,
    pub defines: HashMap<String, String>,
    pub created_at: String,
    pub updated_at: String,
}

/// Gets the profiles directory path.
fn get_profiles_dir() -> Result<PathBuf, String> {
    let config_dir = dirs::config_dir().ok_or("Could not find config directory")?;
    let profiles_dir = config_dir.join("rgbw-dashboard").join("profiles");
    fs::create_dir_all(&profiles_dir)
        .map_err(|e| format!("Failed to create profiles directory: {}", e))?;
    Ok(profiles_dir)
}

/// Validates a GPIO pin configuration.
#[tauri::command]
pub fn validate_pin(
    pin: u8,
    purpose: String,
    platform: String,
    module: Option<String>,
) -> Result<PinValidation, String> {
    let purpose = match purpose.to_lowercase().as_str() {
        "output" => PinPurpose::Output,
        "input" => PinPurpose::Input,
        "adc" => PinPurpose::Adc,
        "i2c" => PinPurpose::I2c,
        "spi" => PinPurpose::Spi,
        "i2s" => PinPurpose::I2s,
        "pwm" => PinPurpose::Pwm,
        _ => return Err(format!("Unknown pin purpose: {}", purpose)),
    };

    let platform = match platform.to_lowercase().as_str() {
        "esp32" | "espressif32" => Platform::Esp32,
        "esp8266" | "espressif8266" => Platform::Esp8266,
        "avr" | "atmelavr" => Platform::Avr,
        _ => return Err(format!("Unknown platform: {}", platform)),
    };

    let module = module.and_then(|m| match m.to_lowercase().as_str() {
        "wrover" => Some(Module::Wrover),
        "pico" => Some(Module::Pico),
        "standard" => Some(Module::Standard),
        _ => None,
    });

    Ok(pin_validator::validate_pin(pin, purpose, platform, module))
}

/// Gets the list of safe GPIO pins for a platform.
#[tauri::command]
pub fn get_safe_pins(platform: String, module: Option<String>) -> Result<Vec<u8>, String> {
    let platform = match platform.to_lowercase().as_str() {
        "esp32" | "espressif32" => Platform::Esp32,
        "esp8266" | "espressif8266" => Platform::Esp8266,
        "avr" | "atmelavr" => Platform::Avr,
        _ => return Err(format!("Unknown platform: {}", platform)),
    };

    let module = module.and_then(|m| match m.to_lowercase().as_str() {
        "wrover" => Some(Module::Wrover),
        "pico" => Some(Module::Pico),
        "standard" => Some(Module::Standard),
        _ => None,
    });

    Ok(pin_validator::get_safe_pins(platform, module))
}

/// Saves a configuration profile.
#[tauri::command]
pub fn save_profile(profile: SavedProfile) -> Result<(), String> {
    info!(
        app = %profile.app_name,
        profile = %profile.name,
        "Saving profile"
    );
    let profiles_dir = get_profiles_dir()?;
    let mut profile = profile;
    let safe_app = profile_paths::sanitize_profile_component("app name", &profile.app_name)?;
    let safe_name = profile_paths::sanitize_profile_component("profile name", &profile.name)?;
    profile.app_name = safe_app.clone();
    profile.name = safe_name.clone();
    let file_name = format!("{}_{}.json", safe_app, safe_name);
    let file_path = profiles_dir.join(&file_name);

    let json =
        serde_json::to_string_pretty(&profile).map_err(|e| format!("Failed to serialize: {}", e))?;

    fs::write(&file_path, json).map_err(|e| format!("Failed to write profile: {}", e))?;

    Ok(())
}

/// Loads a configuration profile.
#[tauri::command]
pub fn load_profile(app_name: String, profile_name: String) -> Result<SavedProfile, String> {
    info!(app = %app_name, profile = %profile_name, "Loading profile");
    let profiles_dir = get_profiles_dir()?;
    let profiles_dir_canon =
        profiles_dir.canonicalize().map_err(|e| format!("Failed to resolve profiles dir: {}", e))?;
    let safe_app = profile_paths::sanitize_profile_component("app name", &app_name)?;
    let safe_profile = profile_paths::sanitize_profile_component("profile name", &profile_name)?;
    let file_name = format!("{}_{}.json", safe_app, safe_profile);
    let file_path = profiles_dir.join(&file_name);

    let canonical = file_path
        .canonicalize()
        .map_err(|e| format!("Failed to resolve profile path: {}", e))?;
    if !canonical.starts_with(&profiles_dir_canon) {
        return Err("Profile path escapes profiles directory".to_string());
    }

    let json = fs::read_to_string(&file_path)
        .map_err(|e| format!("Failed to read profile: {}", e))?;

    let profile: SavedProfile =
        serde_json::from_str(&json).map_err(|e| format!("Failed to parse profile: {}", e))?;

    Ok(profile)
}

/// Lists all saved profiles for an app.
#[tauri::command]
pub fn list_profiles(app_name: String) -> Result<Vec<String>, String> {
    info!(app = %app_name, "Listing profiles");
    let profiles_dir = get_profiles_dir()?;
    let safe_app = profile_paths::sanitize_profile_component("app name", &app_name)?;
    let prefix = format!("{}_", safe_app);

    let mut profiles = Vec::new();

    let entries = fs::read_dir(&profiles_dir).map_err(|e| e.to_string())?;

    for entry in entries.flatten() {
        let name = entry.file_name().to_string_lossy().to_string();
        if name.starts_with(&prefix) && name.ends_with(".json") {
            // Extract profile name: "{app_name}_{profile_name}.json"
            let profile_name = name
                .strip_prefix(&prefix)
                .and_then(|s| s.strip_suffix(".json"))
                .map(|s| s.to_string());

            if let Some(pn) = profile_name {
                profiles.push(pn);
            }
        }
    }

    profiles.sort();
    Ok(profiles)
}

/// Deletes a configuration profile.
#[tauri::command]
pub fn delete_profile(app_name: String, profile_name: String) -> Result<(), String> {
    info!(app = %app_name, profile = %profile_name, "Deleting profile");
    let profiles_dir = get_profiles_dir()?;
    let profiles_dir_canon =
        profiles_dir.canonicalize().map_err(|e| format!("Failed to resolve profiles dir: {}", e))?;
    let safe_app = profile_paths::sanitize_profile_component("app name", &app_name)?;
    let safe_profile = profile_paths::sanitize_profile_component("profile name", &profile_name)?;
    let file_name = format!("{}_{}.json", safe_app, safe_profile);
    let file_path = profiles_dir.join(&file_name);

    if file_path.exists() {
        let canonical = file_path
            .canonicalize()
            .map_err(|e| format!("Failed to resolve profile path: {}", e))?;
        if !canonical.starts_with(&profiles_dir_canon) {
            return Err("Profile path escapes profiles directory".to_string());
        }
        fs::remove_file(&file_path).map_err(|e| format!("Failed to delete profile: {}", e))?;
    }

    Ok(())
}
