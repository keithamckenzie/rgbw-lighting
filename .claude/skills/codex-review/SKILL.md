---
name: codex-review
description: Run a collaborative code review loop between Claude Code and Codex CLI. Use when the user asks to "review with Codex", "Codex review", "two-agent review", or "iterate with Codex".
allowed-tools: Read, Grep, Glob, Bash(codex *), Bash(git status *), Bash(git diff *), Bash(git ls-files *), Bash(git rev-parse *), Bash(git symbolic-ref *), Bash(rm -f /tmp/codex-*)
---

# Codex Collaborative Review Loop

Codex reviews and fixes, Claude Code orchestrates and integrates — repeat until convergence. Codex has the coding edge; default to letting Codex fix any valid issues directly rather than Claude Code rewriting manually.

## Arguments

- `/codex-review` — review unstaged + staged + untracked (working-tree, default)
- `/codex-review --scope staged` — review only staged changes
- `/codex-review --scope branch` — review branch diff against auto-detected base (warns: "uncommitted/staged changes are excluded; use --scope working-tree to include them")
- `/codex-review --scope branch --base develop` — branch diff against specific base
- `/codex-review --scope files --paths shared/lib/RGBWCommon apps/led-panel` — review staged + unstaged + untracked changes within specified directories only
- `--no-search` — disable web search (recommended for sensitive/proprietary code)
- `--max-rounds N` — override default max rounds (default: 5)
- `--state <path>` — persist dismissed issues to a JSON file (requires a path argument)

Scopes are mutually exclusive. `--paths` is only valid with `--scope files`.
All path-filtered git commands use `--` separator: `git diff --shortstat -- <paths>`

## Privacy Guardrail

When `--search` is enabled (the default), do NOT paste secrets, tokens, API keys, or proprietary code excerpts directly into prompts sent to Codex (web search may transmit context). Use file references and let Codex read files directly. For highly sensitive reviews, suggest `--no-search`.

## Temp File Convention

All temp files use `$PPID` (parent PID = the Claude Code process) for session-unique naming. `$PPID` is stable across all Bash tool calls within a session and unique across concurrent sessions. Per-round files use `r{N}` in the name (e.g., `/tmp/codex-r1-$PPID.txt`). If Anthropic changes Claude Code's process model and `$PPID` stops being stable, migrate to `mktemp -d` approach.

## Step 0: Preflight Check

Run once at skill start:

```bash
rm -f /tmp/codex-version-$PPID.log
codex --version > /tmp/codex-version-$PPID.log 2>&1; EC=$?; echo "Exit code: $EC"
```

- Exit code 0: Codex is available, proceed.
- Exit code non-zero or "command not found": Read `/tmp/codex-version-$PPID.log` for the specific error (distinguishes install-missing from auth-failure from other issues), report to user, and stop.

## Step 1: Scope Discovery

Run before Round 1 to understand what will be reviewed. Commands depend on the chosen scope:

**`--scope working-tree` (default):**
```bash
git status -sb
git diff --name-only
git diff --cached --name-only
git ls-files --others --exclude-standard
git diff --shortstat
git diff --cached --shortstat
```

**`--scope staged`:**
```bash
git status -sb
git diff --cached --name-only
git diff --cached --shortstat
```

**`--scope branch`:**
```bash
git status -sb
git diff --name-only $BASE...HEAD
git diff --shortstat $BASE...HEAD
```

**`--scope files --paths <paths>`:**
```bash
git status -sb
git diff --name-only -- <paths>
git diff --cached --name-only -- <paths>
git ls-files --others --exclude-standard -- <paths>
git diff --shortstat -- <paths>
git diff --cached --shortstat -- <paths>
```

### No-Changes Early Exit (scope-aware)

- `--scope working-tree` (default): If `git diff --shortstat` is empty AND `git diff --cached --shortstat` is empty AND `git ls-files --others --exclude-standard` returns nothing: report "No changes to review" and stop without invoking Codex.
- `--scope staged`: If `git diff --cached --shortstat` is empty: report "No staged changes to review" and stop.
- `--scope branch`: If `git diff --shortstat $BASE...HEAD` is empty: report "No branch changes to review" and stop.
- `--scope files --paths <paths>`: If `git diff --shortstat -- <paths>` is empty AND `git diff --cached --shortstat -- <paths>` is empty AND `git ls-files --others --exclude-standard -- <paths>` returns nothing: report "No changes in specified paths" and stop.

### Scope Sizing Guardrails

- If diff exceeds 1000 changed lines: warn user, suggest splitting by library/app.
- If diff exceeds 3000 changed lines: strongly recommend splitting, propose groupings by library or app directory.

### Base Branch Detection (for `--scope branch`)

Fallback chain:
1. `git rev-parse --abbrev-ref @{upstream} 2>/dev/null` — tracking branch
2. `git symbolic-ref --short refs/remotes/origin/HEAD 2>/dev/null` — origin's default branch (do NOT fall through to step 3 if this succeeds)
3. `echo "origin/main"` — final fallback (only if steps 1 and 2 both fail)

User can override via `--base <branch>`.

**Validate before use:** Run `git rev-parse --verify "$BASE" 2>/dev/null` — if invalid, report "Base ref '$BASE' not found. Check branch name or run `git fetch`." and stop.

## Step 2: Review Loop

### Setup

1. Preflight check passed (Step 0)
2. Scope discovery complete (Step 1)
3. Determine mode: feedback-only (`--sandbox read-only`) or edit-allowed (no sandbox). Default is feedback-only for Round 1, but Claude Code should recommend switching to edit mode for any valid issues (see step 6 below).
4. Set MAX_ROUNDS = 5 (user can override via `--max-rounds`)
5. Initialize: `round_number=1`, `clean_round_count=0`, `dismissed_issues=[]`
6. Record round baseline: `round_base_head=$(git rev-parse HEAD)` + snapshot of `git diff --shortstat` and `git diff --cached --shortstat`
7. If `--state <path>` provided, load `dismissed_issues` from state file

### Each Round

**1. Compose prompt** with: scope, diff summary, prior feedback summary, what was fixed, dismissed_issues.

Diff commands vary by `--scope`:
- `working-tree` (default): `git diff --shortstat` + `git diff --cached --shortstat` + `git ls-files --others --exclude-standard`
- `staged`: `git diff --cached --shortstat` + `git diff --cached --name-only`
- `branch`: `git diff --shortstat $BASE...HEAD` + `git diff --name-only $BASE...HEAD`
- `files`: `git diff --shortstat -- <paths>` + `git diff --cached --shortstat -- <paths>` + `git diff --name-only -- <paths>` + `git diff --cached --name-only -- <paths>` + `git ls-files --others --exclude-standard -- <paths>`

**2. Invoke Codex** (session-unique temp files via `$PPID`, consistent `-C`, stderr capture):

**Pre-round cleanup** (prevents stale reads from prior runs or rounds):
```bash
rm -f /tmp/codex-r{N}-$PPID.txt /tmp/codex-r{N}-$PPID.err
```

Feedback mode:
```bash
codex --model gpt-5.3-codex --search exec --sandbox read-only --full-auto \
  -C /Users/keithmckenzie/Projects/rgbw-lighting \
  -o /tmp/codex-r{N}-$PPID.txt \
  "PROMPT" 2>/tmp/codex-r{N}-$PPID.err; EC=$?; echo "Exit code: $EC"
```

Edit mode:
```bash
codex --model gpt-5.3-codex --search exec --full-auto \
  -C /Users/keithmckenzie/Projects/rgbw-lighting \
  -o /tmp/codex-r{N}-$PPID.txt \
  "PROMPT" 2>/tmp/codex-r{N}-$PPID.err; EC=$?; echo "Exit code: $EC"
```

**Search flag rules:**
- `--model gpt-5.3-codex` is required — always include for model targeting
- `--search` is a top-level flag — goes BEFORE subcommand: `codex --model gpt-5.3-codex --search exec` (NOT `codex exec --search`)
- If user passed `--no-search`: simply omit `--search` from the command (there is NO `--no-search` flag)
- NEVER invent flags that aren't in the templates above. Use the templates exactly as written, only omitting `--search` when requested.

Use the Bash tool's `run_in_background` parameter set to `true` (do NOT set `timeout`). This returns a task ID immediately, enabling parallel skill invocations (e.g., launching codex-review and gemini-review concurrently without waiting).

Before reading the output file, call `TaskOutput` with the task ID (`block: true`, `timeout: 600000`) to wait for completion. If still running after the first poll, poll again — reviews may take up to 10 minutes.

Edit-mode prompt constraints (append to prompt):
> Only edit files within the review scope. List all files you changed in your response. Do not create new files unless strictly required. Do not delete files.

**3. Read** `/tmp/codex-r{N}-$PPID.txt` with Read tool.

**4. Parse structured output** (see Parser Spec below).

**5. Present results** to user with severity counts.

**6. Recommend action based on severity:**
- **If any valid issues found:** Recommend "let Codex fix directly" as the default action. Codex has the coding edge — it identified the issues and is best positioned to fix them cleanly. Present as: "Codex found N issues. Recommended: let Codex fix these directly. Alternatives: (a) Claude Code addresses them, (b) dismiss specific issues (false positives only — 'pre-existing' is not a valid reason), (c) stop."

**7. Await user decision** before proceeding.

**7a. If user chose "let Codex fix directly":** after Codex finishes editing, run `git diff` and `git status -sb` to show what changed (including any newly created files). Present the diff and status to the user. Ask user to confirm before continuing to the next round. Do NOT proceed to re-review until user has reviewed Codex's edits.

**8. If `--state` provided**, write updated `dismissed_issues` to state file.

**9. Post-round cleanup** — remove temp files from this round:
```bash
rm -f /tmp/codex-r{N}-$PPID.txt /tmp/codex-r{N}-$PPID.err
```

**10. After feedback is addressed** (by Codex directly or Claude Code), snapshot new round baseline:
- `round_base_head=$(git rev-parse HEAD)`
- `git diff --shortstat` (working tree)
- `git diff --cached --shortstat` (staged)

This becomes "CHANGES SINCE LAST REVIEW" for round N+1:
- If commits were made: `git diff --shortstat $prev_round_base_head...$new_round_base_head`
- Working tree changes: `git diff --shortstat`
- Staged changes: `git diff --cached --shortstat`

### Convergence

- If Codex reports VERDICT: PASS (zero valid findings at any severity): `clean_round_count += 1`
- If ANY new valid issue appears at any severity: `clean_round_count = 0` (RESET)
- If `clean_round_count >= 1`: propose stopping ("Codex reports clean pass")
- ALL valid findings at any severity must be fixed — not just critical/high
- Dismissed issues (false positives only, with documented rationale) do NOT count toward severity totals
- "Pre-existing" is NOT a valid dismissal reason — all valid findings in reviewed code are in scope
- Dismissed issue identity: keyed by file + short summary (not line number, since lines shift). Each dismissed item stored as: `{file, summary, reason, round_dismissed}`

### Termination

MAX_ROUNDS reached, user stops, or convergence detected.

## Prompt Templates

### Round 1 Prompt

```
You are reviewing code in a collaborative loop with Claude Code.
Your role: thorough, actionable code review.

PROJECT: PlatformIO monorepo — ESP32 (C++17), ESP8266 (C++11), AVR (C++11). Arduino framework, NeoPixelBus, ESP-DSP, FreeRTOS.
CONVENTIONS: shared libraries in shared/lib/, apps in apps/, no Arduino String class, no exceptions, pre-allocated buffers, enum class error codes.
GOLD STANDARD: shared/lib/RGBWCommon/src/rgbw.cpp (color math), shared/lib/LEDStrip/src/led_strip.cpp (driver template).
ARCHITECTURE: shared/lib/RGBWCommon (color math, types) → shared/lib/LEDStrip (addressable NeoPixelBus driver) + shared/lib/LEDPWM (MOSFET PWM channels). shared/lib/AudioInput (I2S DMA + ESP-DSP FFT, ESP32 only). shared/lib/Connectivity (WiFi/BLE, ESP32 only).
LED PROTOCOLS: SK6812 RGBW (4-channel, 5V, 800kHz NRZ), WS2815B RGB (3-channel, 12V, 800kHz NRZ), 24V non-addressable via MOSFET PWM.
DASHBOARD: Tauri desktop app (Svelte 5 + TypeScript + Tailwind) in tools/dashboard/.
DESIGN DOCS: docs/ (led-control.md, audio-reactive.md, esp32-internals.md, coding-standards.md, pin-reference.md, power-and-wiring.md)

SCOPE: [uncommitted changes / branch diff / specific files]

DIFF SUMMARY:
[git diff --shortstat output]
[git diff --name-only + git diff --cached --name-only — list of changed files]
[Untracked (new) files: list from git ls-files --others --exclude-standard]
[If binary files present: "Binary files (skip patch content, review usage only): [list]"]

(Do NOT paste full patches — let Codex read files directly in --full-auto mode.)

Codex is PRIMARY reviewer.

CHECK FOR (Codex primary domains first):
- Memory safety (PRIMARY): stack overflow in recursive calls, heap fragmentation from runtime allocation, buffer overruns in LED/audio buffers, DMA buffer alignment, SRAM budget on AVR (2KB), no Arduino String class
- Concurrency (PRIMARY): FreeRTOS task priorities and starvation, ISR safety (no heap alloc, no printf, no mutex in ISR), Core 0 (WiFi/BLE) vs Core 1 (LED/audio) task pinning, mutex usage in shared state, vTaskDelayUntil() correctness
- Hardware interaction (PRIMARY): GPIO safety (strapping pins GPIO 0/2/12/15, input-only GPIO 34-39, ADC2+WiFi conflict), NRZ timing tolerances (800kHz), power injection requirements, MOSFET gate drive, level shifting
- Platform portability (PRIMARY): C++11 compatibility in shared libraries, #if __cplusplus guards for C++17 features, printf format macros (PRIu32 from inttypes.h, %zu for size_t), platform-specific #ifdef guards
- DRY / Shared-first (PRIMARY): duplicated logic across apps, missed existing shared utilities in RGBWCommon/LEDStrip/LEDPWM, new local code that should be in shared lib
- Real-time (PRIMARY): LED strip show() blocking duration, loop timing for smooth animations, I2S DMA callback latency, vTaskDelayUntil() usage, blocking calls in time-critical paths
- Correctness: behavior matches docs/ specs, color math accuracy (HSV→RGBW, gamma correction), error paths properly surfaced via enum class
- LED behavior: gamma correction application order, white channel extraction in RGBW, brightness scaling linearity, color model conversion accuracy
- Coding standards: no Arduino String, no exceptions, pre-allocated buffers, enum class error codes, ESP_LOG macros on ESP32, Serial on AVR
- Compatibility: shared library API stability (RGBWCommon types used by LEDStrip/LEDPWM/apps), platformio.ini compatibility across ESP32/ESP8266/AVR
- Dashboard (if UI touched): Svelte 5 runes, zinc palette (not slate-), Tailwind CSS, Tauri invoke patterns, TypeScript typing

REQUIRED OUTPUT FORMAT (strict — no extra text outside this format):
SUMMARY:
- critical: <count>
- high: <count>
- medium: <count>
- low: <count>

ISSUES:
1. [SEVERITY] file:line - Description (use file:? if line unknown)
   Suggestion: How to fix
(If zero issues, write exactly:)
ISSUES:
- none

VERDICT: PASS | NEEDS_WORK (PASS = zero valid findings at any severity; NEEDS_WORK = any valid finding remains)
DRY_CHECK: PASS | NEEDS_WORK
SHARED_CANDIDATES: [paths to existing utilities that could be reused/extended, or "none"]
```

### Round N (N > 1) Prompt

```
Round N re-review. You are reviewing code after fixes were applied.

CHANGES SINCE LAST REVIEW:
[git diff --stat of working tree changes since round N-1 baseline]
[git diff --cached --stat for any newly staged changes]
[If commits were made: git diff --stat $round_base_head...HEAD]

ADDRESSED ISSUES:
[list of fixed items from prior round]

DISMISSED ISSUES (do not re-raise these — user has reviewed and dismissed them):
[user-dismissed items with brief reason]

Focus: verify fixes are correct, check for regressions introduced by the fixes, and re-check primary domains (Memory safety, Concurrency, Hardware interaction, Platform portability, DRY, Real-time). Find anything missed in prior rounds.

REQUIRED OUTPUT FORMAT (strict — no extra text outside this format):
SUMMARY:
- critical: <count>
- high: <count>
- medium: <count>
- low: <count>

ISSUES:
1. [SEVERITY] file:line - Description (use file:? if line unknown)
   Suggestion: How to fix
(If zero issues, write exactly:)
ISSUES:
- none

VERDICT: PASS | NEEDS_WORK (PASS = zero valid findings at any severity; NEEDS_WORK = any valid finding remains)
DRY_CHECK: PASS | NEEDS_WORK
SHARED_CANDIDATES: [paths to existing utilities that could be reused/extended, or "none"]
```

## Parser Spec (Claude Code reads directly — no script)

Claude Code reads the Codex response file with the Read tool and parses structured format:

1. Look for `SUMMARY:` section — extract integer counts for critical/high/medium/low
2. Look for `ISSUES:` section — if contains `- none`, zero issues confirmed
3. Look for `VERDICT:` line — extract `PASS` or `NEEDS_WORK`
4. Tolerate `file:?` in issue references (unknown line number)
5. If structured format is missing or malformed: read raw text, do own severity assessment, report: "Codex did not use structured format; my assessment of severity: [counts]. Raw output below."
6. Look for optional `DRY_CHECK:` line — extract `PASS` or `NEEDS_WORK`. If absent, treat as not reported (do not fail parsing).
7. Look for optional `SHARED_CANDIDATES:` line — extract paths list. If absent, treat as empty.
8. When structured format is missing, skip auto-convergence logic (no reliable counts).

## Disagreement Protocol

When Claude Code and Codex disagree on an issue:
1. Present both perspectives with reasoning
2. Clearly label which view belongs to which agent
3. User decides the resolution
4. If user sides with Claude Code, dismiss the Codex issue with reason "disagreement — Claude Code's approach preferred"

## Error Handling

| Scenario | Action |
|----------|--------|
| Codex exits non-zero | Read `/tmp/codex-r{N}-$PPID.err`, report to user |
| Output file empty | Report "Codex produced no output", show stderr, do not retry |
| Stalled (no progress >20 min) | Check with `TaskOutput` (block: false). If still running, report to user, suggest narrower scope or file grouping |
| Parse failure | Read raw text, do own severity assessment, report with raw output |
| Codex auth failure | Report "Codex auth issue, run `codex login`" |
| Scope too large (>3000 lines) | Pre-check catches; propose file group splits before invoking Codex |
| No changes detected | Early exit without invoking Codex |
| Codex not installed | Preflight catches; report and stop |

## State Persistence (`--state`)

- Default: dismissed issues kept in-memory only (lost on context compression)
- `--state <path>`: persist dismissed_issues to the specified JSON file path
- A path argument is always required when using `--state` (e.g., `--state /tmp/review-state.json`)
- If user specifies a repo-relative path, warn them to add it to `.gitignore` — do NOT modify `.gitignore` automatically; ask the user first.
- State file format: `{ "dismissed_issues": [{ "file": "...", "summary": "...", "reason": "...", "round_dismissed": N }] }`

## Codex -> Claude Code Mirror

If Codex is orchestrating review loops but you want Claude Code to run the review pass, use this mirror.

### Preflight

```bash
rm -f /tmp/claude-version-$PPID.log
claude --version > /tmp/claude-version-$PPID.log 2>&1; EC=$?; echo "Exit code: $EC"
```

### Round Invocation (review only, JSON)

```bash
rm -f /tmp/claude-review-r{N}-$PPID.json /tmp/claude-review-r{N}-$PPID.err
claude -p --permission-mode plan --output-format json --max-turns 1 \
  "Review the current diff and return STRICT JSON with: summary, issues, verdict, dry_check, shared_candidates." \
  > /tmp/claude-review-r{N}-$PPID.json 2>/tmp/claude-review-r{N}-$PPID.err; EC=$?; echo "Exit code: $EC"
```

### Notes

- Keep one round per invocation (`--max-turns 1`) so orchestration logic remains in the outer loop.
- For multi-round continuity, pass the previous session with `claude -p --resume <session-id>`.
- Claude Code has no top-level `--search` toggle like Codex.
