use std::path::PathBuf;

/// Discovers the monorepo path by looking for repo markers.
pub fn find_monorepo_root() -> Result<PathBuf, String> {
    let mut current = std::env::current_dir().map_err(|e| e.to_string())?;

    loop {
        if is_monorepo_root(&current) {
            return Ok(current);
        }

        if !current.pop() {
            break;
        }
    }

    Err("Could not find rgbw-lighting monorepo root. \
         Ensure the dashboard is run from within the repository."
        .to_string())
}

fn is_monorepo_root(path: &PathBuf) -> bool {
    let has_apps = path.join("apps").is_dir();
    let has_shared = path.join("shared").is_dir();
    let has_tools = path.join("tools").is_dir();
    let has_readme = path.join("README.md").is_file();
    let has_repo_marker = path.join("CLAUDE.md").exists() || path.join(".git").exists();
    let has_example = path.join("apps").join("example-rgbw").is_dir();

    has_apps && (has_shared || has_tools) && (has_repo_marker || has_readme || has_example)
}
