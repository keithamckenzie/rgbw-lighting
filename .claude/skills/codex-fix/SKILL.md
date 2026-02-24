---
name: codex-fix
description: Let Codex directly edit files to fix bugs. Use when user says "let Codex fix", "Codex fix this", "have Codex debug", or wants Codex to fix a specific bug. For feature implementation, use codex-implement instead.
allowed-tools: Read, Write, Grep, Glob, Bash(echo *), Bash(codex --model gpt-5.3-codex exec --profile codex-fix-restricted *), Bash(codex --model gpt-5.3-codex --search exec --profile codex-fix-restricted *), Bash(codex --model gpt-5.3-codex exec --profile claude-full *), Bash(codex --model gpt-5.3-codex --search exec --profile claude-full *), Bash(cat *), Bash(git diff *), Bash(git status *), Bash(git ls-files *), Bash(rm -f /tmp/codex-*), Bash(grep *), Bash(pio run *), Bash(pio test *)
---

# Codex Direct Editing

Let Codex directly edit files to fix issues. Used when the user wants Codex to apply changes rather than just provide feedback.

## Arguments

- `/codex-fix "Fix instructions here"` — let Codex edit without web search (default, recommended for proprietary code)
- `/codex-fix --search "Fix instructions here"` — edit with web search enabled (for external API/library references)
- `/codex-fix --escalate "Fix instructions here"` — use `claude-full` profile instead of restricted (when fix needs broader access)
- `/codex-fix --allow-dirty "Fix instructions here"` — skip working tree check (user takes responsibility for interleaved diffs)

Flags can be combined: `/codex-fix --search --escalate "instructions"`

## Privacy Guardrail

Defaults to `--no-search` (same as `codex-implement`). Bug fixes touch proprietary code internals. Enable `--search` only when Codex needs external docs (library behavior, API references, version-specific syntax).

When `--search` is enabled, do NOT paste secrets, tokens, API keys, or proprietary code excerpts directly into the prompt. Use file references and let Codex read files directly.

## Sandbox Model

`codex-fix` uses a **least-privilege two-tier model**:

| Profile | Sandbox | Approval | When |
|---------|---------|----------|------|
| `codex-fix-restricted` (default) | `workspace-write` | `never` (fail-closed) | Normal bug fixes |
| `claude-full` (escalation) | `danger-full-access` | `on-request` | `--escalate` flag, or when restricted profile hits sandbox limits |

**Why restricted by default:** `codex-fix` is for narrow bug fixes. It only needs file edits + focused acceptance checks. Full system access (`danger-full-access`) is over-privileged for this use case.

**Why fail-closed (`approval_policy = "never"`):** `codex-fix` runs in background mode. Interactive approval prompts would hang. If Codex hits a sandbox limit, it fails cleanly and the error handling reports it — Claude Code can then suggest re-invoking with `--escalate`.

**Verification split:** Codex runs focused acceptance checks (which `workspace-write` supports). Claude Code owns full verification (`pio run` / `pio test -e native`) execution. This keeps Codex least-privileged while preserving full verification capability.

## Temp File Convention

All temp files use `$PPID` (parent PID = the Claude Code process) for session-unique naming. `$PPID` is stable across all Bash tool calls within a session and unique across concurrent sessions. If Anthropic changes Claude Code's process model and `$PPID` stops being stable, migrate to `mktemp -d` approach.

## Pre-Flight

**Profile check (mode-aware):**

Check only the profile needed for the current invocation mode:

**Default mode (no `--escalate`):**
```bash
grep -Eq '^\[profiles\.codex-fix-restricted\]' ~/.codex/config.toml 2>/dev/null; echo "Profile check (restricted): $?"
```

Exit 0: profile exists, proceed. Non-zero: report actionable error:
> "Codex profile `codex-fix-restricted` not found in `~/.codex/config.toml`. Add this block:
> ```toml
> [profiles.codex-fix-restricted]
> sandbox_mode = "workspace-write"
> approval_policy = "never"
> ```"

**Escalated mode (`--escalate`):**
```bash
grep -Eq '^\[profiles\.claude-full\]' ~/.codex/config.toml 2>/dev/null; echo "Profile check (claude-full): $?"
```

Exit 0: proceed. Non-zero: report actionable error:
> "Codex profile `claude-full` not found in `~/.codex/config.toml`. Add this block:
> ```toml
> [profiles.claude-full]
> sandbox_mode = "danger-full-access"
> approval_policy = "on-request"
> ```"

Note: Each mode only requires its own profile. `--escalate` does NOT require `codex-fix-restricted`, and default mode does NOT require `claude-full`.

**Dirty working tree check:**

Skip if `--allow-dirty` was passed.

Before invoking Codex in edit mode, check for uncommitted changes (staged, unstaged, and untracked):

```bash
git diff --shortstat
git diff --cached --shortstat
git ls-files --others --exclude-standard
```

If any of these three commands produce non-empty output, the tree is dirty. Warn:
> "Working tree has uncommitted changes (unstaged/staged/untracked). Codex edits will interleave with your changes, making diffs hard to isolate. Commit or stash first?"

Wait for user confirmation before proceeding.

## Step 1: Build Fix Brief

Write to `/tmp/codex-fix-context-$PPID.md` using the Write tool (not Bash).

**$PPID note:** The Write tool doesn't expand `$PPID`. Get the literal value first via `echo $PPID` in Bash, then use that value in the Write tool file path.

**If Write fails:** fall back to compact inline prompt (objective + symptom + full evidence + scope + no-touch with tiers + acceptance checks). Report failure.

Structure:

```markdown
# Fix Brief

## BUG OBJECTIVE
[What must be fixed — 1-2 sentences]

## OBSERVED SYMPTOM
[Facts only: error messages, incorrect output, failing test names]

## REPRO STEPS
[How to reproduce, or "see failing test X"]

## EXPECTED BEHAVIOR
[What should happen instead]

## ACTUAL BEHAVIOR
[What currently happens]

## EVIDENCE
[Error text, log snippets, stack traces, test output.
IMPORTANT: For compiler/linter errors, include the FULL error message —
many errors embed fix guidance in the message itself. This is authoritative context Codex needs.]

## SCOPE: Starting Files (not exhaustive)
[Files to start with — Codex may follow root causes beyond these via BETTER_SOLUTION_OUTSIDE_SCOPE]

## NO-TOUCH ZONES
Tiered list. HARD = user-directed or safety-critical, cannot touch without re-approval.
SOFT = Claude Code recommendation, touch with justification if needed for correctness/DRY/completeness.
Format: `TIER: path — reason`

- HARD: platformio.ini — board/framework config, user-managed
- HARD: partition tables (*.csv) — flash layout, user-managed
- HARD: .github/ — CI workflows
- SOFT: docs/*.md — reference docs, avoid churn
- SOFT: tools/dashboard/src-tauri/gen/ — Tauri generated code

## ACCEPTANCE CHECKS
[What must pass: specific focused tests, manual verification steps]
[Codex will run these commands after applying fixes. Include only safe, non-destructive, deterministic commands.]
[NOTE: Do NOT include full pio run/test here — Claude Code runs those separately after Codex finishes.]

## OPTIONAL HYPOTHESES (NON-BINDING)
[Claude Code's best guess at root cause, if any. Codex should verify independently.]
```

**Key principle:** Describe the symptoms and desired outcome. Do NOT prescribe the fix. If Claude Code suspects a root cause, include it as a non-binding hypothesis — Codex verifies independently.

## Step 2: Invoke Codex

**Pre-invocation cleanup** (output/error files only — do NOT delete context file):
```bash
rm -f /tmp/codex-fix-$PPID.txt /tmp/codex-fix-$PPID.err
```

### Default (no web search, restricted profile):
```bash
codex --model gpt-5.3-codex exec --profile codex-fix-restricted \
  -C /Users/keithmckenzie/Projects/rgbw-lighting \
  -o /tmp/codex-fix-$PPID.txt \
  "<PROMPT>" 2>/tmp/codex-fix-$PPID.err; EC=$?; echo "Exit code: $EC"
```

### With web search (`--search`):
```bash
codex --model gpt-5.3-codex --search exec --profile codex-fix-restricted \
  -C /Users/keithmckenzie/Projects/rgbw-lighting \
  -o /tmp/codex-fix-$PPID.txt \
  "<PROMPT>" 2>/tmp/codex-fix-$PPID.err; EC=$?; echo "Exit code: $EC"
```

### Escalated (`--escalate`):
```bash
codex --model gpt-5.3-codex exec --profile claude-full \
  -C /Users/keithmckenzie/Projects/rgbw-lighting \
  -o /tmp/codex-fix-$PPID.txt \
  "<PROMPT>" 2>/tmp/codex-fix-$PPID.err; EC=$?; echo "Exit code: $EC"
```

### Escalated with web search (`--escalate --search`):
```bash
codex --model gpt-5.3-codex --search exec --profile claude-full \
  -C /Users/keithmckenzie/Projects/rgbw-lighting \
  -o /tmp/codex-fix-$PPID.txt \
  "<PROMPT>" 2>/tmp/codex-fix-$PPID.err; EC=$?; echo "Exit code: $EC"
```

**Invariant:** The prompt body (`<PROMPT>`) must be byte-identical across all variants; only `--search` and `--profile` differ.

### Runtime Prompt (`<PROMPT>`)

```
Read the fix brief at /tmp/codex-fix-context-$PPID.md. It defines WHAT is broken and the acceptance checks. Determine HOW to fix it. Treat OPTIONAL HYPOTHESES as non-binding until verified.

This is a PlatformIO embedded project. See docs/coding-standards.md for conventions.
PROJECT: PlatformIO monorepo — ESP32 (C++17), ESP8266 (C++11), AVR (C++11). Arduino framework, NeoPixelBus, ESP-DSP, FreeRTOS.
CONVENTIONS: shared libraries in shared/lib/, apps in apps/, no Arduino String class, no exceptions, pre-allocated buffers, enum class error codes.
GOLD STANDARD: shared/lib/RGBWCommon/src/rgbw.cpp (color math patterns), shared/lib/LEDStrip/src/led_strip.cpp (driver patterns).

CONSTRAINTS:
- Start with files in SCOPE. You may follow root causes beyond SCOPE — MUST report every outside-SCOPE file in BETTER_SOLUTION_OUTSIDE_SCOPE. NO-TOUCH ZONES still apply.
- NO-TOUCH ZONES: HARD = do NOT modify (report CONSTRAINT_CONFLICT if fix requires it). SOFT = avoid if possible, modify with justification in ASSUMPTIONS.
- Prefer existing shared utilities. Do not create new files unless strictly required. Do not delete files.
- Shared libraries must be C++11 compatible. Guard C++17 features with #if __cplusplus >= 201703L.
- Use PRIu32/PRId32 from <inttypes.h> for printf format macros (not %lu/%ld).

VERIFICATION:
- Run ONLY the focused acceptance check commands from the ACCEPTANCE CHECKS section of the brief. Do NOT run pio run/test — Claude Code handles that separately.
- If acceptance checks fail due to your changes, fix and re-run once (max 2 attempts). Do not retry for pre-existing or environment failures.

REQUIRED OUTPUT — list all files changed, then:

VERIFICATION:
- STATUS: PASSED | FAILED | SKIPPED | BLOCKED
- COMMANDS_RUN: [list — acceptance checks only, not pio run/test]
- ATTEMPTS: [0, 1, or 2]
- FAILURE_ATTRIBUTION: CHANGES | BASELINE | ENVIRONMENT | UNKNOWN
- SUMMARY: [one line]
- FAILURE_DETAILS: [required when FAILED]
- SKIP_REASON: [required when SKIPPED]
- BLOCKED_REASON: [required when BLOCKED]

ASSUMPTIONS: (only if you made judgment calls or touched SOFT no-touch files)
- [Decisions, justifications]

CONSTRAINT_CONFLICTS: (only if conflicts exist)
- HARD: path — why needed, alternatives tried, impact

BETTER_SOLUTION_OUTSIDE_SCOPE: (only if you modified files outside SCOPE)
- path — root-cause rationale, alternative if constrained, risk delta, ownership/conflict notes

REUSE_DECISIONS: (only if you extended/created shared code or rejected an obvious candidate)
- [extended|created|rejected]: path — rationale
```

Use the Bash tool's `run_in_background` parameter set to `true` (do NOT set `timeout`). This returns a task ID immediately, enabling parallel skill invocations (e.g., launching codex-review and gemini-review concurrently without waiting).

Before reading the output file, call `TaskOutput` with the task ID (`block: true`, `timeout: 600000`) to wait for completion. If still running after the first poll, poll again — complex fixes may take up to 20 minutes.

Note: No `--sandbox read-only` here — Codex needs write access to edit files. The `codex-fix-restricted` profile provides `workspace-write` sandbox (least-privilege for file edits).

## Step 3: After Codex Finishes

1. Read `/tmp/codex-fix-$PPID.txt` with the Read tool to see Codex's description of what it did
2. Run `git diff` to see what Codex changed in tracked files
3. Run `git status -sb` to catch any newly created or deleted files
4. **Verification gate — two-phase:**
   - **Phase 1 (Codex's focused checks):** Check Codex's VERIFICATION section for acceptance check results. If STATUS=PASSED for focused checks, those are trusted.
   - **Phase 2 (Claude Code's full verification):** Claude Code runs full project verification when the fix warrants it:
     - **Always run for:** memory safety fixes, ISR changes, broad changes (>3 files), any fix touching DMA/I2S/WiFi/BLE
     - **Run if Codex FAILED/BLOCKED/SKIPPED** (with code changes)/missing for any tier
     - **Skip for:** single-file fixes where Codex's focused checks PASSED and the fix is clearly narrow
   - Full verification commands:
     ```bash
     cd /Users/keithmckenzie/Projects/rgbw-lighting/apps/<app-name> && pio run 2>&1 > /tmp/pio.log; EC=$?; echo "Exit code: $EC"; head -80 /tmp/pio.log; echo "..."; tail -80 /tmp/pio.log
     ```
     If tests exist:
     ```bash
     cd /Users/keithmckenzie/Projects/rgbw-lighting/apps/<app-name> && pio test -e native 2>&1 > /tmp/test.log; EC=$?; echo "Exit code: $EC"; head -80 /tmp/test.log; echo "..."; tail -80 /tmp/test.log
     ```
5. **BETTER_SOLUTION_OUTSIDE_SCOPE triage** (if present in Codex output):
   - **Auto-accept** when: no HARD no-touch files involved, low blast radius (same library), no API contract change
   - **Ask user** when: product behavior changes, cross-library refactor, or changes to shared types in RGBWCommon
   - **Reject/defer** when: collides with HARD no-touch constraints or active parallel session ownership
   - **Default** when ownership is uncertain: ask user
6. Present the diff, any BETTER_SOLUTION_OUTSIDE_SCOPE decisions, and status to the user
7. Ask the user what they want to do:
   - Keep all changes
   - Keep some changes (user specifies which)
   - Discard all changes
8. **Do NOT revert changes without explicit user approval and user-specified command** — reverting requires prohibited git commands (`git checkout --`, `git restore`), so only the user can authorize and execute this
9. **Post-use cleanup** — remove all temp files to avoid leaking context:
   ```bash
   rm -f /tmp/codex-fix-$PPID.txt /tmp/codex-fix-$PPID.err /tmp/codex-fix-context-$PPID.md
   ```

## Error Handling

| Scenario | Action |
|----------|--------|
| Context file write fails | Fall back to compact inline prompt (objective + symptom + full evidence + scope + no-touch with tiers + acceptance checks), report failure |
| Codex exits non-zero | Read `/tmp/codex-fix-$PPID.err`, report to user |
| Output file empty | Report "Codex produced no output", show stderr |
| Sandbox limit hit | Codex fails cleanly (fail-closed). Report: "Codex hit sandbox limit under `workspace-write`. Re-invoke with `--escalate` to use `claude-full` profile, or ask user to run the blocked command locally." Do NOT suggest narrowing acceptance checks — that weakens verification. |
| Stalled (no progress >20 min) | Check with `TaskOutput` (block: false). If still running, report to user, suggest narrower scope |
| Codex not installed | Report "Codex CLI not found. Install it first." |
| Auth failure | Report "Codex auth issue, run `codex login`" |
| CONSTRAINT_CONFLICT reported | Codex stopped because a HARD no-touch blocks the fix. Re-scope the NO-TOUCH ZONES or adjust the fix objective, then re-invoke. |
| BETTER_SOLUTION_OUTSIDE_SCOPE reported | Codex found a better root-cause fix outside SCOPE. Triage per Step 3.5: auto-accept (no HARD files, low blast radius, same library, no API contract change), ask user (behavior/cross-library/shared types), or reject/defer (HARD collision, concurrent session risk). |
| SOFT no-touch zone modified | Review Codex's justification in ASSUMPTIONS. Accept if justified; flag to user if questionable. |

## Notes

- `--model gpt-5.3-codex` is required — always include for model targeting
- `--search` is a top-level flag — goes BEFORE subcommand: `codex --model gpt-5.3-codex --search exec` (NOT `codex exec --search`)
- Each invocation is stateless — the fix brief IS the context
- Codex may start its own MCP servers if configured in `~/.codex/config.toml`
- Stderr is noisy (MCP startup, rollout errors); captured separately in `.err` file
- Default profile: `codex-fix-restricted` (`workspace-write`, `approval_policy = "never"`) — least-privilege for narrow bug fixes
- Escalation profile: `claude-full` (`danger-full-access`, `approval_policy = "on-request"`) — for fixes that need broader access
- Required profiles in `~/.codex/config.toml`:
  ```toml
  [profiles.codex-fix-restricted]
  sandbox_mode = "workspace-write"
  approval_policy = "never"

  [profiles.claude-full]
  sandbox_mode = "danger-full-access"
  approval_policy = "on-request"
  ```

## Codex -> Claude Code Mirror

If Codex is orchestrating bug-fix flow but you want Claude Code to perform the fix pass, use this mirror.

### Preflight

```bash
rm -f /tmp/claude-version-$PPID.log
claude --version > /tmp/claude-version-$PPID.log 2>&1; EC=$?; echo "Exit code: $EC"
```

### Invocation (fix mode, JSON)

```bash
rm -f /tmp/claude-fix-$PPID.json /tmp/claude-fix-$PPID.err
claude -p --permission-mode acceptEdits --output-format json \
  "Read /tmp/codex-fix-context-$PPID.md, fix the issue, run only brief-defined acceptance checks, and return STRICT JSON with files_changed, verification, assumptions, constraint_conflicts, better_solution_outside_scope, reuse_decisions." \
  > /tmp/claude-fix-$PPID.json 2>/tmp/claude-fix-$PPID.err; EC=$?; echo "Exit code: $EC"
```

### Notes

- Start with `--permission-mode acceptEdits`; use `--permission-mode bypassPermissions` only if permission prompts block non-interactive execution.
- Keep focused acceptance checks in the delegate run; outer orchestrator still owns full-project verification.
- Claude Code has no top-level `--search` toggle like Codex.
