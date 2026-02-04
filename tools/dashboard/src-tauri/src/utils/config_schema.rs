use once_cell::sync::Lazy;
use regex::Regex;
use serde::{Deserialize, Serialize};
use std::collections::{HashMap, HashSet};
use std::fs;
use std::path::Path;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AppConfigSchema {
    pub has_config: bool,
    pub defines: Vec<ConfigDefine>,
    pub platform_conditional: HashMap<String, Vec<ConfigDefine>>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ConfigDefine {
    pub name: String,
    pub default_value: String,
    pub value_type: ValueType,
    pub enum_values: Option<Vec<EnumValue>>,
    pub platform: Option<String>,
    pub description: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EnumValue {
    pub value: String,
    pub label: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "lowercase")]
pub enum ValueType {
    Integer,
    Float,
    Boolean,
    String,
    Enum,
    Pin,
}

/// Parses a config.h file to extract configurable defines.
///
/// Looks for patterns like:
/// - `#ifndef PANEL_WIDTH`
/// - `#define PANEL_WIDTH 36`
///
/// And optional enum comments like:
/// - `// 0=SK6812_RGBW, 1=WS2815B_RGB`
pub fn parse_config_schema(app_path: &Path) -> Result<AppConfigSchema, String> {
    let config_path = app_path.join("src/config.h");

    if !config_path.exists() {
        return Ok(AppConfigSchema {
            has_config: false,
            defines: Vec::new(),
            platform_conditional: HashMap::new(),
        });
    }

    let content =
        fs::read_to_string(&config_path).map_err(|e| format!("Failed to read config.h: {}", e))?;

    let (defines, platform_conditional) = extract_config_defines(&content)?;

    Ok(AppConfigSchema {
        has_config: true,
        defines,
        platform_conditional,
    })
}

#[derive(Debug, Clone)]
struct ConditionalFrame {
    platform: Option<String>,
    platform_conditional: bool,
    ifndef_name: Option<String>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum PlatformContext {
    None,
    Specific,
    Other,
}

fn extract_config_defines(
    content: &str,
) -> Result<(Vec<ConfigDefine>, HashMap<String, Vec<ConfigDefine>>), String> {
    let mut defines: Vec<ConfigDefine> = Vec::new();
    let mut defines_seen: HashSet<String> = HashSet::new();
    let mut platform_conditional: HashMap<String, Vec<ConfigDefine>> = HashMap::new();
    let mut platform_seen: HashMap<String, HashSet<String>> = HashMap::new();
    let mut stack: Vec<ConditionalFrame> = Vec::new();
    // Track nested preprocessor conditionals to map defines to platform-specific buckets.

    static DEFINE_RE: Lazy<Regex> =
        Lazy::new(|| Regex::new(r"^#define\s+(\w+)(?:\s+(.*))?$").expect("define regex"));
    static IFNDEF_EXPR_RE: Lazy<Regex> = Lazy::new(|| {
        Regex::new(r"!\s*defined\s*\(?\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)?")
            .expect("ifndef regex")
    });

    for raw_line in content.lines() {
        let trimmed = raw_line.trim();
        if trimmed.is_empty() {
            continue;
        }

        let directive_line = strip_inline_comment(trimmed);

        if let Some(rest) = directive_line.strip_prefix("#ifdef") {
            let name = rest.trim().split_whitespace().next().unwrap_or("");
            let platform = extract_platform_from_tokens(name);
            stack.push(ConditionalFrame {
                platform: platform.clone(),
                platform_conditional: platform.is_some(),
                ifndef_name: None,
            });
            continue;
        }

        if let Some(rest) = directive_line.strip_prefix("#ifndef") {
            let name = rest.trim().split_whitespace().next().unwrap_or("").to_string();
            stack.push(ConditionalFrame {
                platform: None,
                platform_conditional: false,
                ifndef_name: if name.is_empty() { None } else { Some(name) },
            });
            continue;
        }

        if let Some(rest) = directive_line.strip_prefix("#elif") {
            if let Some(frame) = stack.last_mut() {
                let expr = rest.trim();
                let (platform, platform_conditional) = extract_platform_from_expr(expr);
                frame.platform = platform;
                frame.platform_conditional = platform_conditional;
                frame.ifndef_name = extract_ifndef_from_expr(expr, &IFNDEF_EXPR_RE);
            }
            continue;
        }

        if directive_line.starts_with("#else") {
            if let Some(frame) = stack.last_mut() {
                frame.platform = None;
                frame.ifndef_name = None;
            }
            continue;
        }

        if directive_line.starts_with("#endif") {
            stack.pop();
            continue;
        }

        if let Some(rest) = directive_line.strip_prefix("#if") {
            let expr = rest.trim();
            let (platform, platform_conditional) = extract_platform_from_expr(expr);
            let ifndef_name = extract_ifndef_from_expr(expr, &IFNDEF_EXPR_RE);
            stack.push(ConditionalFrame {
                platform,
                platform_conditional,
                ifndef_name,
            });
            continue;
        }

        if let Some(caps) = DEFINE_RE.captures(trimmed) {
            let name = caps.get(1).map(|m| m.as_str()).unwrap_or("").to_string();
            if name.is_empty() || !is_configurable_define(&stack, &name) {
                continue;
            }

            let value_with_comment = caps.get(2).map(|m| m.as_str()).unwrap_or("");
            let mut parts = value_with_comment.splitn(2, "//");
            let raw_value = parts.next().unwrap_or("").trim().to_string();
            let comment = parts.next().map(|s| s.trim().to_string());

            let (value_type, enum_values, default_value) = parse_value(&name, &raw_value, &comment);

            match current_platform_context(&stack) {
                PlatformContext::None => {
                    if defines_seen.insert(name.clone()) {
                        defines.push(ConfigDefine {
                            name,
                            default_value,
                            value_type,
                            enum_values,
                            platform: None,
                            description: comment,
                        });
                    }
                }
                PlatformContext::Specific => {
                    if let Some(platform) = current_platform_name(&stack) {
                        let seen = platform_seen.entry(platform.clone()).or_default();
                        if seen.insert(name.clone()) {
                            platform_conditional
                                .entry(platform.clone())
                                .or_default()
                                .push(ConfigDefine {
                                    name,
                                    default_value,
                                    value_type,
                                    enum_values,
                                    platform: Some(platform),
                                    description: comment,
                                });
                        }
                    }
                }
                PlatformContext::Other => {
                    // Ambiguous or negative platform branches are ignored to avoid duplicates.
                }
            }
        }
    }

    Ok((defines, platform_conditional))
}

fn strip_inline_comment(line: &str) -> &str {
    line.split("//").next().unwrap_or(line).trim()
}

fn extract_ifndef_from_expr(expr: &str, re: &Regex) -> Option<String> {
    re.captures(expr)
        .and_then(|caps| caps.get(1).map(|m| m.as_str().to_string()))
}

fn extract_platform_from_expr(expr: &str) -> (Option<String>, bool) {
    let mut platforms: HashSet<String> = HashSet::new();
    for token in expr
        .split(|c: char| !c.is_ascii_alphanumeric() && c != '_')
        .filter(|t| !t.is_empty())
    {
        // Heuristic: extract single known platform token from complex preprocessor expressions.
        if let Some(platform) = extract_platform_from_tokens(token) {
            platforms.insert(platform);
        }
    }

    let platform = if platforms.len() == 1 {
        platforms.iter().next().cloned()
    } else {
        None
    };
    let platform_conditional = !platforms.is_empty();
    (platform, platform_conditional)
}

fn extract_platform_from_tokens(token: &str) -> Option<String> {
    match token {
        "ESP32" | "ESP8266" | "__AVR__" => Some(token.to_string()),
        _ => None,
    }
}

fn current_platform_context(stack: &[ConditionalFrame]) -> PlatformContext {
    for frame in stack.iter().rev() {
        if frame.platform_conditional {
            return if frame.platform.is_some() {
                PlatformContext::Specific
            } else {
                PlatformContext::Other
            };
        }
    }
    PlatformContext::None
}

fn current_platform_name(stack: &[ConditionalFrame]) -> Option<String> {
    for frame in stack.iter().rev() {
        if frame.platform_conditional {
            if let Some(platform) = &frame.platform {
                return Some(platform.clone());
            }
            return None;
        }
    }
    None
}

fn is_configurable_define(stack: &[ConditionalFrame], name: &str) -> bool {
    stack
        .iter()
        .any(|frame| frame.ifndef_name.as_deref() == Some(name))
}

fn parse_value(
    name: &str,
    raw_value: &str,
    comment: &Option<String>,
) -> (ValueType, Option<Vec<EnumValue>>, String) {
    // Check for enum pattern in comment: "0=Label1, 1=Label2"
    if let Some(ref cmt) = comment {
        static ENUM_RE: Lazy<Regex> =
            Lazy::new(|| Regex::new(r"(\d+)\s*=\s*([^,]+)").expect("enum regex"));
        let enum_values: Vec<EnumValue> = ENUM_RE
            .captures_iter(cmt)
            .filter_map(|caps| {
                let value = caps.get(1)?.as_str().to_string();
                let label = caps.get(2)?.as_str().trim().to_string();
                Some(EnumValue { value, label })
            })
            .collect();

        if !enum_values.is_empty() {
            return (ValueType::Enum, Some(enum_values), raw_value.to_string());
        }
    }

    // Check for pin-related names
    if name.starts_with("PIN_") || name.ends_with("_PIN") {
        return (ValueType::Pin, None, raw_value.to_string());
    }

    // Check for boolean
    if raw_value == "true" || raw_value == "false" || raw_value == "1" || raw_value == "0" {
        if name.contains("ENABLED") || name.contains("ENABLE") || name.starts_with("USE_") {
            return (ValueType::Boolean, None, raw_value.to_string());
        }
    }

    // Check for float
    if raw_value.contains('.') {
        if let Ok(_) = raw_value.parse::<f64>() {
            return (ValueType::Float, None, raw_value.to_string());
        }
    }

    // Check for integer
    if let Ok(_) = raw_value.parse::<i64>() {
        return (ValueType::Integer, None, raw_value.to_string());
    }

    // Check for hex integer
    if raw_value.starts_with("0x") || raw_value.starts_with("0X") {
        if let Ok(_) = i64::from_str_radix(&raw_value[2..], 16) {
            return (ValueType::Integer, None, raw_value.to_string());
        }
    }

    // Default to string
    (ValueType::String, None, raw_value.to_string())
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::tempdir;

    #[test]
    fn test_no_config_file() {
        let temp = tempdir().unwrap();
        let result = parse_config_schema(temp.path()).unwrap();
        assert!(!result.has_config);
        assert!(result.defines.is_empty());
    }

    #[test]
    fn test_basic_defines() {
        let temp = tempdir().unwrap();
        let src_dir = temp.path().join("src");
        fs::create_dir_all(&src_dir).unwrap();
        fs::write(
            src_dir.join("config.h"),
            r#"
#ifndef PANEL_WIDTH
#define PANEL_WIDTH 36
#endif

#ifndef PANEL_HEIGHT
#define PANEL_HEIGHT 24
#endif

#ifndef BRIGHTNESS
#define BRIGHTNESS 0.8
#endif
"#,
        )
        .unwrap();

        let result = parse_config_schema(temp.path()).unwrap();
        assert!(result.has_config);
        assert_eq!(result.defines.len(), 3);

        let width = result.defines.iter().find(|d| d.name == "PANEL_WIDTH").unwrap();
        assert_eq!(width.default_value, "36");
        assert_eq!(width.value_type, ValueType::Integer);

        let brightness = result.defines.iter().find(|d| d.name == "BRIGHTNESS").unwrap();
        assert_eq!(brightness.value_type, ValueType::Float);
    }

    #[test]
    fn test_enum_define() {
        let temp = tempdir().unwrap();
        let src_dir = temp.path().join("src");
        fs::create_dir_all(&src_dir).unwrap();
        fs::write(
            src_dir.join("config.h"),
            r#"
#ifndef STRIP_TYPE
#define STRIP_TYPE 0 // 0 = SK6812 RGBW (default), 1 = WS2815B RGB
#endif
"#,
        )
        .unwrap();

        let result = parse_config_schema(temp.path()).unwrap();
        let strip = result.defines.iter().find(|d| d.name == "STRIP_TYPE").unwrap();

        assert_eq!(strip.value_type, ValueType::Enum);
        let enums = strip.enum_values.as_ref().unwrap();
        assert_eq!(enums.len(), 2);
        assert_eq!(enums[0].value, "0");
        assert_eq!(enums[0].label, "SK6812 RGBW (default)");
    }

    #[test]
    fn test_pin_define() {
        let temp = tempdir().unwrap();
        let src_dir = temp.path().join("src");
        fs::create_dir_all(&src_dir).unwrap();
        fs::write(
            src_dir.join("config.h"),
            r#"
#ifndef PIN_LED_DATA
#define PIN_LED_DATA 16
#endif
"#,
        )
        .unwrap();

        let result = parse_config_schema(temp.path()).unwrap();
        let pin = result.defines.iter().find(|d| d.name == "PIN_LED_DATA").unwrap();
        assert_eq!(pin.value_type, ValueType::Pin);
    }

    #[test]
    fn test_platform_conditionals() {
        let temp = tempdir().unwrap();
        let src_dir = temp.path().join("src");
        fs::create_dir_all(&src_dir).unwrap();
        fs::write(
            src_dir.join("config.h"),
            r#"
#ifndef PANEL_WIDTH
#define PANEL_WIDTH 36
#endif

#ifdef ESP32
#ifndef PIN_LED_DATA
#define PIN_LED_DATA 4
#endif
#elif defined(ESP8266)
#ifndef PIN_LED_DATA
#define PIN_LED_DATA 2
#endif
#else
#ifndef PIN_LED_DATA
#define PIN_LED_DATA 99
#endif
#endif
"#,
        )
        .unwrap();

        let result = parse_config_schema(temp.path()).unwrap();
        assert_eq!(result.defines.len(), 1);
        assert!(result
            .defines
            .iter()
            .any(|d| d.name == "PANEL_WIDTH"));
        assert!(result
            .defines
            .iter()
            .all(|d| d.name != "PIN_LED_DATA"));

        let esp32 = result
            .platform_conditional
            .get("ESP32")
            .expect("ESP32 defines");
        assert_eq!(esp32.len(), 1);
        assert_eq!(esp32[0].default_value, "4");

        let esp8266 = result
            .platform_conditional
            .get("ESP8266")
            .expect("ESP8266 defines");
        assert_eq!(esp8266.len(), 1);
        assert_eq!(esp8266[0].default_value, "2");
    }
}
