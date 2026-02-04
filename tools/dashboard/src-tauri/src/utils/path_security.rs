use std::path::{Path, PathBuf};

/// Validates that an app path is within the monorepo boundary.
///
/// Prevents arbitrary directory traversal attacks by:
/// 1. Rejecting path traversal attempts (.. / \)
/// 2. Canonicalizing paths to resolve symlinks
/// 3. Verifying the result is within the monorepo
/// 4. Checking for platformio.ini to confirm it's a valid project
pub fn validate_app_path(monorepo_path: &Path, app_name: &str) -> Result<PathBuf, String> {
    // Canonicalize monorepo path
    let monorepo = monorepo_path
        .canonicalize()
        .map_err(|_| "Invalid monorepo path")?;

    // Reject path traversal attempts
    if app_name.contains("..")
        || app_name.contains('/')
        || app_name.contains('\\')
        || app_name.contains('\0')
    {
        return Err("Invalid app name: contains path traversal characters".to_string());
    }

    // Reject empty or whitespace-only names
    if app_name.trim().is_empty() {
        return Err("Invalid app name: empty or whitespace-only".to_string());
    }

    let app_path = monorepo.join("apps").join(app_name);

    // Verify it exists and is within monorepo
    let canonical_app = app_path
        .canonicalize()
        .map_err(|_| format!("App '{}' not found", app_name))?;

    if !canonical_app.starts_with(&monorepo) {
        return Err("Path escapes monorepo boundary".to_string());
    }

    // Verify it's a PlatformIO project
    if !canonical_app.join("platformio.ini").exists() {
        return Err(format!("'{}' is not a PlatformIO project", app_name));
    }

    Ok(canonical_app)
}

/// Validates that a file path is within an allowed directory.
pub fn validate_file_path(base_path: &Path, file_path: &Path) -> Result<PathBuf, String> {
    let base = base_path
        .canonicalize()
        .map_err(|e| format!("Invalid base path: {}", e))?;

    let canonical = file_path
        .canonicalize()
        .map_err(|e| format!("Invalid file path: {}", e))?;

    if !canonical.starts_with(&base) {
        return Err("File path escapes allowed directory".to_string());
    }

    Ok(canonical)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use tempfile::tempdir;

    #[test]
    fn test_rejects_path_traversal() {
        let temp = tempdir().unwrap();
        let result = validate_app_path(temp.path(), "../etc/passwd");
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("path traversal"));
    }

    #[test]
    fn test_rejects_forward_slash() {
        let temp = tempdir().unwrap();
        let result = validate_app_path(temp.path(), "foo/bar");
        assert!(result.is_err());
    }

    #[test]
    fn test_rejects_empty_name() {
        let temp = tempdir().unwrap();
        let result = validate_app_path(temp.path(), "");
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("empty"));
    }

    #[test]
    fn test_rejects_whitespace_name() {
        let temp = tempdir().unwrap();
        let result = validate_app_path(temp.path(), "   ");
        assert!(result.is_err());
    }

    #[test]
    fn test_rejects_missing_app() {
        let temp = tempdir().unwrap();
        fs::create_dir_all(temp.path().join("apps")).unwrap();
        let result = validate_app_path(temp.path(), "nonexistent");
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("not found"));
    }

    #[test]
    fn test_rejects_non_pio_project() {
        let temp = tempdir().unwrap();
        let app_path = temp.path().join("apps/myapp");
        fs::create_dir_all(&app_path).unwrap();

        let result = validate_app_path(temp.path(), "myapp");
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("not a PlatformIO project"));
    }

    #[test]
    fn test_accepts_valid_app() {
        let temp = tempdir().unwrap();
        let app_path = temp.path().join("apps/myapp");
        fs::create_dir_all(&app_path).unwrap();
        fs::write(app_path.join("platformio.ini"), "[env:test]").unwrap();

        let result = validate_app_path(temp.path(), "myapp");
        assert!(result.is_ok());
    }
}
