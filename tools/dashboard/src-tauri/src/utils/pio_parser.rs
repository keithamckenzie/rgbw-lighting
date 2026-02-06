use configparser::ini::Ini;
use once_cell::sync::Lazy;
use regex::Regex;
use serde::{Deserialize, Serialize};
use std::collections::{HashMap, HashSet};
use std::path::Path;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PlatformioConfig {
    pub environments: Vec<DiscoveredEnvironment>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DiscoveredEnvironment {
    pub name: String,
    pub extends: Option<String>,
    pub platform: String,
    pub board: Option<String>,
    pub framework: Option<String>,
    pub build_flags: Vec<String>,
    pub lib_deps: Vec<String>,
    pub is_hardware_target: bool,
    pub can_upload: bool,
}

/// Parses a platformio.ini file with support for `extends` and variable interpolation.
pub fn parse_platformio_ini(path: &Path) -> Result<PlatformioConfig, String> {
    let mut ini = Ini::new();
    ini.load(path).map_err(|e| format!("Failed to load platformio.ini: {}", e))?;

    let sections = ini.sections();
    let mut raw_envs: HashMap<String, HashMap<String, String>> = HashMap::new();

    // First pass: collect all environments
    for section in &sections {
        if let Some(env_name) = section.strip_prefix("env:") {
            let props = ini
                .get_map()
                .ok_or("Failed to get INI map")?
                .get(section)
                .cloned()
                .unwrap_or_default();

            // Convert Option<String> values to String
            let clean_props: HashMap<String, String> = props
                .into_iter()
                .filter_map(|(k, v)| v.map(|val| (k, val)))
                .collect();

            raw_envs.insert(env_name.to_string(), clean_props);
        }
    }

    // Second pass: resolve extends
    let env_names: Vec<String> = raw_envs.keys().cloned().collect();
    let mut resolved_envs: HashMap<String, HashMap<String, String>> = HashMap::new();

    for env_name in &env_names {
        let resolved = resolve_extends(&raw_envs, env_name)?;
        resolved_envs.insert(env_name.clone(), resolved);
    }

    // Third pass: resolve variable interpolation
    for env_name in &env_names {
        let mut props = resolved_envs.get(env_name).expect("resolved env must exist").clone();
        resolve_interpolation(&mut props, &resolved_envs);
        resolved_envs.insert(env_name.clone(), props);
    }

    // Convert to DiscoveredEnvironment structs
    let environments: Vec<DiscoveredEnvironment> = env_names
        .iter()
        .map(|name| {
            let props = resolved_envs.get(name).expect("resolved env must exist");
            let platform = props.get("platform").cloned().unwrap_or_default();
            let is_native = platform == "native";

            DiscoveredEnvironment {
                name: name.clone(),
                extends: raw_envs
                    .get(name)
                    .and_then(|p| p.get("extends"))
                    .map(|s| s.strip_prefix("env:").unwrap_or(s).to_string()),
                platform: platform.clone(),
                board: props.get("board").cloned(),
                framework: props.get("framework").cloned(),
                build_flags: parse_multiline_list(props.get("build_flags")),
                lib_deps: parse_multiline_list(props.get("lib_deps")),
                is_hardware_target: !is_native,
                can_upload: !is_native,
            }
        })
        .collect();

    Ok(PlatformioConfig { environments })
}

/// Recursively resolves `extends` inheritance.
fn resolve_extends(
    envs: &HashMap<String, HashMap<String, String>>,
    env_name: &str,
) -> Result<HashMap<String, String>, String> {
    let mut visited = HashSet::new();
    resolve_extends_internal(envs, env_name, &mut visited)
}

fn resolve_extends_internal(
    envs: &HashMap<String, HashMap<String, String>>,
    env_name: &str,
    visited: &mut HashSet<String>,
) -> Result<HashMap<String, String>, String> {
    if !visited.insert(env_name.to_string()) {
        return Err(format!("Circular extends detected for environment '{}'", env_name));
    }

    let props = envs
        .get(env_name)
        .ok_or_else(|| format!("Environment '{}' not found", env_name))?
        .clone();

    if let Some(extends) = props.get("extends") {
        let parent_name = extends.strip_prefix("env:").unwrap_or(extends);
        let parent_props = resolve_extends_internal(envs, parent_name, visited)?;

        // Merge: child overrides parent
        let mut merged = parent_props;
        for (key, value) in props {
            if key != "extends" {
                // For multi-line values like build_flags, we might want to append
                // For now, child completely overrides parent
                merged.insert(key, value);
            }
        }
        Ok(merged)
    } else {
        Ok(props)
    }
}

/// Resolves ${env.key} and ${env:name.key} interpolation.
fn resolve_interpolation(
    props: &mut HashMap<String, String>,
    all_envs: &HashMap<String, HashMap<String, String>>,
) {
    // Replace ${env.key} and ${env:name.key} references within the environment values.
    static ENV_INTERPOLATION_RE: Lazy<Regex> =
        Lazy::new(|| Regex::new(r"\$\{env(?::([^.}]+))?\.([^}]+)\}").expect("env regex"));

    // Iterate to resolve nested references; cap iterations to avoid infinite loops.
    const MAX_ITERATIONS: usize = 10;
    for _ in 0..MAX_ITERATIONS {
        let mut changed = false;
        let keys: Vec<String> = props.keys().cloned().collect();
        for key in keys {
            if let Some(value) = props.get(&key).cloned() {
                let resolved = ENV_INTERPOLATION_RE
                    .replace_all(&value, |caps: &regex::Captures| {
                        let env_name = caps.get(1).map(|m| m.as_str());
                        let ref_key = caps.get(2).map(|m| m.as_str()).unwrap_or("");

                        if ref_key.is_empty() {
                            return caps.get(0).map(|m| m.as_str()).unwrap_or("").to_string();
                        }

                        match env_name {
                            None => props.get(ref_key).cloned().unwrap_or_default(),
                            Some(name) => all_envs
                                .get(name)
                                .and_then(|e| e.get(ref_key))
                                .cloned()
                                .unwrap_or_default(),
                        }
                    })
                    .to_string();

                if resolved != value {
                    changed = true;
                    props.insert(key, resolved);
                }
            }
        }

        if !changed {
            break;
        }
    }
    // If still changing after MAX_ITERATIONS, we stop to avoid infinite interpolation loops.
}

/// Parses a multiline INI value into a list of items.
fn parse_multiline_list(value: Option<&String>) -> Vec<String> {
    value
        .map(|s| {
            s.lines()
                .map(|line| line.trim())
                .filter(|line| !line.is_empty())
                .map(|s| s.to_string())
                .collect()
        })
        .unwrap_or_default()
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use tempfile::tempdir;

    #[test]
    fn test_parse_basic_env() {
        let temp = tempdir().unwrap();
        let ini_path = temp.path().join("platformio.ini");
        fs::write(
            &ini_path,
            r#"
[env:esp32]
platform = espressif32
board = esp32dev
framework = arduino
"#,
        )
        .unwrap();

        let config = parse_platformio_ini(&ini_path).unwrap();
        assert_eq!(config.environments.len(), 1);

        let env = &config.environments[0];
        assert_eq!(env.name, "esp32");
        assert_eq!(env.platform, "espressif32");
        assert_eq!(env.board, Some("esp32dev".to_string()));
        assert!(env.is_hardware_target);
        assert!(env.can_upload);
    }

    #[test]
    fn test_parse_native_env() {
        let temp = tempdir().unwrap();
        let ini_path = temp.path().join("platformio.ini");
        fs::write(
            &ini_path,
            r#"
[env:native]
platform = native
"#,
        )
        .unwrap();

        let config = parse_platformio_ini(&ini_path).unwrap();
        let env = &config.environments[0];

        assert_eq!(env.name, "native");
        assert!(!env.is_hardware_target);
        assert!(!env.can_upload);
    }

    #[test]
    fn test_parse_extends() {
        let temp = tempdir().unwrap();
        let ini_path = temp.path().join("platformio.ini");
        fs::write(
            &ini_path,
            r#"
[env:base]
platform = espressif32
framework = arduino

[env:esp32]
extends = env:base
board = esp32dev
"#,
        )
        .unwrap();

        let config = parse_platformio_ini(&ini_path).unwrap();

        let esp32 = config
            .environments
            .iter()
            .find(|e| e.name == "esp32")
            .unwrap();

        assert_eq!(esp32.platform, "espressif32");
        assert_eq!(esp32.board, Some("esp32dev".to_string()));
        assert_eq!(esp32.extends, Some("base".to_string()));
    }

    #[test]
    fn test_env_interpolation() {
        let temp = tempdir().unwrap();
        let ini_path = temp.path().join("platformio.ini");
        fs::write(
            &ini_path,
            r#"
[env:base]
platform = espressif32
lib_deps = lib1

[env:child]
extends = env:base
framework = ${env.platform}
lib_deps = ${env:base.lib_deps}
"#,
        )
        .unwrap();

        let config = parse_platformio_ini(&ini_path).unwrap();
        let child = config
            .environments
            .iter()
            .find(|e| e.name == "child")
            .unwrap();

        assert_eq!(child.framework, Some("espressif32".to_string()));
        assert_eq!(child.lib_deps, vec!["lib1".to_string()]);
    }

    #[test]
    fn test_circular_extends_errors() {
        let temp = tempdir().unwrap();
        let ini_path = temp.path().join("platformio.ini");
        fs::write(
            &ini_path,
            r#"
[env:a]
extends = env:b
platform = espressif32

[env:b]
extends = env:a
platform = espressif32
"#,
        )
        .unwrap();

        let result = parse_platformio_ini(&ini_path);
        assert!(result.is_err());
    }

    #[test]
    fn test_nested_interpolation_resolves() {
        let temp = tempdir().unwrap();
        let ini_path = temp.path().join("platformio.ini");
        fs::write(
            &ini_path,
            r#"
[env:base]
common_flags = -DFOO=1

[env:dev]
build_flags = ${env:base.common_flags} -DBAR=2
extra = ${env.build_flags}
"#,
        )
        .unwrap();

        let config = parse_platformio_ini(&ini_path).unwrap();
        let env = config
            .environments
            .iter()
            .find(|e| e.name == "dev")
            .unwrap();
        // Build flags on one line become one item after parsing
        assert!(!env.build_flags.is_empty());
        let flags_str = env.build_flags.join(" ");
        assert!(flags_str.contains("-DFOO=1"), "Should contain interpolated FOO flag");
        assert!(flags_str.contains("-DBAR=2"), "Should contain BAR flag");
        assert!(env.lib_deps.is_empty());
    }
}
