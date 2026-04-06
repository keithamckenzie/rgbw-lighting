---
name: commit
description: Guided commit with gate enforcement, protected file detection, AI attribution check, and staging protocol compliance. Use when user says "commit", "ready to commit", or wants to finalize changes.
allowed-tools: Read, Grep, Glob, Bash(git diff *), Bash(git status *), Bash(git add *), Bash(git commit *), Bash(git log *), Bash(git branch *), Bash(git rev-parse *), Bash(git symbolic-ref *), Bash(pio run *), Bash(pio test *)
---

# Commit Skill

Enforces the full CLAUDE.md commit protocol: pre-commit readiness, build verification, AI attribution check, scope verification, and user confirmation.

## Arguments

- `/commit` — start the commit flow (no auto-commit, always interactive)
- `/commit "message hint"` — start with a suggested summary direction (still goes through all gates)

## Preflight

Before starting, verify:
1. There are actual changes to commit (`git status -sb` shows modified/untracked files)
2. No review or verification is currently running

If no changes exist, report and stop.

## Step 1: Build Gate Check

Determine what build verification is needed:

**Gate requirements:**
- All commits: `pio run` must pass for affected environments
- If shared/lib/ touched: build all environments (esp32, esp8266, arduino-uno if applicable)
- If tests exist: `pio test -e native` must pass
- After codex-review or codex-implement: Codex VERIFICATION PASSED counts

**Gate matrix format:**
```
| Gate | Status | Evidence |
|------|--------|----------|
| pio run (esp32) | PASS/FAIL/NOT_RUN | [exit code or "Codex VERIFICATION PASSED"] |
| pio run (esp8266) | PASS/FAIL/NOT_RUN/N_A | [exit code or N/A if not affected] |
| pio test (native) | PASS/FAIL/NOT_RUN/N_A | [exit code or N/A if no tests] |
| codex-review | PASS/FAIL/NOT_RUN/N_A | [verdict] |
| gemini-review | PASS/FAIL/NOT_RUN/N_A | [verdict] |

UNRESOLVED FINDINGS: [count or "none"]
```

If required gates are NOT_RUN or FAIL, output `COMMIT_BLOCKED` with missing gates and stop.

## Step 2: Candidate File List

Get candidate files for staging:
```bash
git diff --name-only
git diff --cached --name-only
git status -sb
```

Present the full list of files that will be staged.

## Step 3: Scope Verification

**If implementing from a codex-implement or codex-fix brief:**
Cross-check candidate files against the brief's SCOPE and any accepted BETTER_SOLUTION_OUTSIDE_SCOPE files.

**For all commits:**
Check for out-of-scope files. Flag suspicious inclusions.

## Step 4: Protected File Detection

Check every candidate file against the protected list:
- `platformio.ini` (any app)
- `*.csv` (partition tables)
- `.github/**`

If any protected file is in the candidate list, **BLOCK** unless the user explicitly stated intent to modify it. Report which files are blocked and why.

## Step 5: Draft Commit Message

Draft a commit message following CLAUDE.md format:

```
<type>: <brief summary>

<Detailed body with sections>
- Specific files changed and what changed
- Technical implementation details
- Organized into sections when multiple areas affected

Why:
- Rationale for the changes
- Benefits and improvements
- Context for future maintainers
```

Types: `feat`, `fix`, `refactor`, `docs`, `test`, `chore`, `perf`

**Requirements:**
- Detailed body explaining WHAT changed (files, specific changes)
- "Why:" section explaining rationale and benefits
- Sections for different types of changes when applicable
- NO one-line commits (use detailed multi-paragraph format)

## Step 6: AI Attribution Check

Check the draft message against ALL forbidden patterns from CLAUDE.md:

**Forbidden (case-insensitive):**
- Tool names: claude code, github copilot, cursor, gemini, codex, windsurf, aider
- Attribution: generated with, co-authored-by: claude, ai-assisted, ai-generated, with ai help
- Robot emoji
- Internal process: "Tier 0/1/2", "Slice N", "codex-review", "gemini-review", "review rounds", "clean pass", "VERDICT: PASS", "NEEDS_WORK", "gate-status", "acceptance criteria"

**Format checks:**
- No emoji anywhere
- Not a one-liner (must have body with details)
- No sensitive info (passwords, keys, WiFi credentials, internal URLs)

If any pattern matches, revise and re-check. Do not present a failing message.

## Step 7: Present and Wait for Confirmation

Output the complete commit plan:
```
## Commit Plan

BRANCH: [current branch]
[gate-status matrix from Step 1]

FILES TO STAGE:
[list of files]

COMMIT MESSAGE:
[draft message]

[any warnings about scope, protected files, or concerns]

Confirm to proceed with staging and commit.
```

**Wait for explicit user confirmation in a new reply.** Per CLAUDE.md: NEVER commit until user validates.

## Step 8: Stage and Commit

Only after user confirmation:

1. Stage files with named paths only:
```bash
git add file1 file2 file3
```

2. Verify staging:
```bash
git diff --cached --name-only
```
Confirm every staged file matches the approved list.

3. Commit with the approved message (HEREDOC for formatting):
```bash
git commit -m "$(cat <<'EOF'
<message>
EOF
)"
```

4. Verify commit:
```bash
git log --oneline -1
```

## Error Handling

| Scenario | Action |
|----------|--------|
| Gates not met | Output `COMMIT_BLOCKED` with missing gates. Stop. |
| Protected file in candidates | Block that file. Report to user. Continue with remaining files if any. |
| AI attribution detected | Revise message automatically. Re-check. Present revised version. |
| Pre-commit hook fails | Read error output. Fix the issue. Create a NEW commit (never amend). |
| User rejects plan | Ask what they want to change. Revise and re-present. |
| No files to commit | Report "nothing to commit" and stop. |
