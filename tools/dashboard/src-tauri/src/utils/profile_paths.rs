/// Sanitizes a profile identifier to prevent path traversal.
pub fn sanitize_profile_component(label: &str, value: &str) -> Result<String, String> {
    let trimmed = value.trim();
    const MAX_LEN: usize = 50;

    if trimmed.is_empty() {
        return Err(format!("Invalid {}: empty or whitespace-only", label));
    }
    if trimmed.len() > MAX_LEN {
        return Err(format!(
            "Invalid {}: too long (max {} characters)",
            label, MAX_LEN
        ));
    }

    // Restrict to portable filename characters across platforms.
    if !trimmed
        .chars()
        .all(|c| c.is_ascii_alphanumeric() || matches!(c, ' ' | '_' | '-' | '.'))
    {
        return Err(format!(
            "Invalid {}: use letters, numbers, spaces, '.', '_', or '-'",
            label
        ));
    }

    if trimmed.contains("..")
        || trimmed.contains('/')
        || trimmed.contains('\\')
        || trimmed.contains('\0')
    {
        return Err(format!(
            "Invalid {}: contains path traversal characters",
            label
        ));
    }

    Ok(trimmed.to_string())
}

/// Builds a profile filename from app/profile names after sanitization.
pub fn build_profile_filename(app_name: &str, profile_name: &str) -> Result<String, String> {
    let app = sanitize_profile_component("app name", app_name)?;
    let profile = sanitize_profile_component("profile name", profile_name)?;
    Ok(format!("{}_{}.json", app, profile))
}
