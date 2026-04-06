---
name: snapshot
description: Output a concise session handoff snapshot inline. Use when user says "snapshot", "session summary", "handoff", or wants to capture current session state for resumption.
allowed-tools: Read, Grep, Glob, Bash(git diff *), Bash(git status *), Bash(git log *), Bash(git branch *), Bash(git rev-list *), Bash(git rev-parse *), Bash(git symbolic-ref *), Bash(echo *)
---

# Session Handoff Snapshot

Outputs a concise inline summary of the current session state for handoff to another session or for resumption later. No file creation — outputs directly inline.

## Arguments

- `/snapshot` — generate snapshot inline

## Output Format

Output the following sections inline (directly in the conversation, no file):

```
## Session Snapshot

**Objective:** [Current task/goal in 1-2 sentences]

**Branch:** [current branch name]
**Commits Ahead of Base:** [count from `git rev-list <base>..HEAD --count`, or "N/A" if on default branch]

**Files Touched:**
[from `git diff --name-only` + `git status -sb`, grouped by status]
- Modified: file1.cpp, file2.h
- Staged: file3.cpp
- Untracked: file4.cpp

**Open Findings:** [from review-findings state if available, or "none"]
- F1 (high): description — VALID_FIX_NOW
- F2 (medium): description — awaiting user decision

**Build Status:**
- [ ] pio run -e esp32 [PASS/FAIL/NOT_RUN]
- [ ] pio run -e esp8266 [PASS/FAIL/NOT_RUN/N_A]
- [ ] pio test -e native [PASS/FAIL/NOT_RUN/N_A]

**Review Status:**
- [ ] codex-review [PASS/FAIL/NOT_RUN/N_A]
- [ ] gemini-review [PASS/FAIL/NOT_RUN/N_A]

**Pending Verifications:** [any checks still needed]

**Next Action:** [what should happen next]
```

## Data Sources

1. **Objective:** Infer from conversation context (the current task being worked on)
2. **Branch:** `git branch --show-current`
3. **Base branch detection (for ahead-count):** Use fallback chain: `git rev-parse --abbrev-ref @{upstream}` → `git symbolic-ref --short refs/remotes/origin/HEAD` → `origin/main`. Same logic as codex-review base-branch detection. If on the default branch itself, output "N/A".
4. **Files touched:** `git diff --name-only` + `git diff --cached --name-only` + `git status -sb`
5. **Open findings:** Check if review-findings skill was used in this session; if so, summarize open findings
6. **Build status:** Check conversation context for pio run/test results
7. **Review status:** Check conversation context for codex-review results, gemini-review results
8. **Pending:** Infer from what's been done vs what's needed (shared lib change → need multi-env build)
9. **Next action:** Infer from current state (e.g., "run pio run -e esp32", "fix 2 remaining findings", "ready for /commit")

## Notes

- Keep the output concise — this is a snapshot, not a detailed report
- If information is unavailable (e.g., no review was run), mark as "N/A" or "NOT_RUN"
- Do not create files — output inline only
