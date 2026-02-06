use std::path::{Path, PathBuf};

/// Resolves the PlatformIO CLI path from the project's virtual environment.
///
/// The repo uses a project-local venv per docs/build-and-tooling.md.
/// No fallback to system PATH to ensure reproducible builds.
pub fn resolve_pio_path(monorepo_path: &Path) -> Result<PathBuf, String> {
    #[cfg(target_os = "windows")]
    let venv_pio = monorepo_path.join(".venv/Scripts/pio.exe");

    #[cfg(not(target_os = "windows"))]
    let venv_pio = monorepo_path.join(".venv/bin/pio");

    if venv_pio.exists() {
        #[cfg(not(target_os = "windows"))]
        {
            use std::os::unix::fs::PermissionsExt;
            let metadata = std::fs::metadata(&venv_pio)
                .map_err(|e| format!("Failed to read PlatformIO metadata: {}", e))?;
            let mode = metadata.permissions().mode();
            if mode & 0o111 == 0 {
                return Err(format!(
                    "PlatformIO found at {} but is not executable. Run: chmod +x {}",
                    venv_pio.display(),
                    venv_pio.display()
                ));
            }
        }

        return Ok(venv_pio);
    }

    // Hard error with instructions - no fallback to system PATH
    Err(format!(
        "PlatformIO not found at {}\n\n\
         Run these commands from the repo root:\n\
         \n\
           uv venv .venv\n\
           uv pip install platformio --python .venv/bin/python\n\
         \n\
         See docs/build-and-tooling.md for details.",
        venv_pio.display()
    ))
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use tempfile::tempdir;

    #[test]
    fn test_pio_not_found_returns_helpful_error() {
        let temp = tempdir().unwrap();
        let result = resolve_pio_path(temp.path());

        assert!(result.is_err());
        let err = result.unwrap_err();
        assert!(err.contains("PlatformIO not found"));
        assert!(err.contains("uv venv .venv"));
        assert!(err.contains("uv pip install platformio"));
    }

    #[test]
    fn test_pio_found_when_exists() {
        let temp = tempdir().unwrap();

        #[cfg(target_os = "windows")]
        let pio_path = temp.path().join(".venv/Scripts/pio.exe");
        #[cfg(not(target_os = "windows"))]
        let pio_path = temp.path().join(".venv/bin/pio");

        fs::create_dir_all(pio_path.parent().unwrap()).unwrap();
        fs::write(&pio_path, "").unwrap();

        // Make the file executable on Unix
        #[cfg(not(target_os = "windows"))]
        {
            use std::os::unix::fs::PermissionsExt;
            let mut perms = fs::metadata(&pio_path).unwrap().permissions();
            perms.set_mode(0o755);
            fs::set_permissions(&pio_path, perms).unwrap();
        }

        let result = resolve_pio_path(temp.path());
        assert!(result.is_ok());
        assert_eq!(result.unwrap(), pio_path);
    }
}
