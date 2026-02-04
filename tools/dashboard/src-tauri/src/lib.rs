mod commands;
mod utils;

use commands::serial::SerialState;

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    let _ = tracing_subscriber::fmt()
        .with_env_filter(
            std::env::var("RUST_LOG").unwrap_or_else(|_| "rgbw_dashboard=info".to_string()),
        )
        .with_target(false)
        .try_init();

    tauri::Builder::default()
        .manage(SerialState::default())
        .invoke_handler(tauri::generate_handler![
            // App commands
            commands::apps::discover_apps,
            commands::apps::get_app_info,
            commands::apps::validate_app_path,
            // Config commands
            commands::config::validate_pin,
            commands::config::get_safe_pins,
            commands::config::save_profile,
            commands::config::load_profile,
            commands::config::list_profiles,
            commands::config::delete_profile,
            // PIO commands
            commands::pio::run_build,
            commands::pio::run_upload,
            commands::pio::run_tests,
            commands::pio::clean_build,
            commands::pio::get_pio_version,
            // Serial commands
            commands::serial::list_serial_ports,
            commands::serial::open_serial,
            commands::serial::write_serial,
            commands::serial::close_serial,
            commands::serial::acquire_port_for_upload,
            commands::serial::release_upload_lock,
            commands::serial::get_port_lock_status,
            commands::serial::list_serial_connections,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
