# Codex Execution Protocol

Shared lifecycle and canonical prompt fragments for all Codex execution skills (`codex-ask`, `codex-plan`, `codex-fix`, `codex-implement`, `codex-review`). Each skill references this document for common ceremony and prompt text. Skill-specific behavior lives in the skill's own `SKILL.md`.

**Read this document first** when executing any Codex skill that references it.

---

## Part 1: Lifecycle Steps

Each skill follows these steps in order. Steps marked "Template" require parameter substitution from the skill's overlay table. Steps marked "Verbatim" are used as-is. Steps marked "Slot" are filled entirely by the skill.

**Overlay parameter note:** Skills define both template-consumed params (`{PREFIX}`, `{CONTEXT_PREFIX}`, `{PROFILE}`, `{TOML_BLOCK}`, `{DIRTY_TREE_GUIDANCE}`) and documentation-only params (`{SANDBOX}`, `{APPROVAL}`, `{VERIFICATION_OWNER}`, `{PREDEPLOY_OWNER}`). Documentation-only params are not substituted into templates — they exist in the overlay table as quick-reference for the skill's configuration model.

### Consolidated Execution Model

**Goal:** Minimize tool approval steps by combining sequential Bash and Write calls into fewer batched operations. The individual steps below remain as reference; consolidated patterns are the default execution mode.

**Two-call model** (down from 5-7 sequential calls):

**Call 1 — Consolidated Preflight** (combines Steps 2-4):

```bash
INVOC_ID="${PPID}-${RANDOM}"
# Step 3 mode-specific precheck
codex --version > /dev/null 2>&1 || { echo "ERROR: codex not installed"; exit 1; }
{MODE_SPECIFIC_PRECHECK}
echo "READY|INVOC_ID=$INVOC_ID"
```

Parse the pipe-delimited output to extract `INVOC_ID` for Call 2.

**Call 2 — Consolidated Invocation** (combines Steps 7-9, with prompt/context files written via heredoc):

```bash
INVOC_ID="{literal_invoc_id}"
set -euo pipefail
# Write context file (skills that use one — codex-plan, codex-implement, codex-fix)
cat > /tmp/codex-{skill}-context-$INVOC_ID.md << '__CODEX_CONTEXT_{literal_invoc_id}__'
{context content}
__CODEX_CONTEXT_{literal_invoc_id}__
# Write prompt file
cat > /tmp/codex-{skill}-prompt-$INVOC_ID.md << '__CODEX_PROMPT_{literal_invoc_id}__'
{prompt content}
__CODEX_PROMPT_{literal_invoc_id}__
# Pre-invocation cleanup (Step 7)
rm -f /tmp/codex-{skill}-output*-$INVOC_ID.txt /tmp/codex-{skill}-progress*-$INVOC_ID.err
# Invoke (Step 9) — disable -e so non-zero exit is captured
set +e
{invocation command from Step 9}
EC=$?; set -e; echo "Exit code: $EC"; exit "$EC"
```

Run Call 2 with `run_in_background: true` per Step 10. Skills that need Step 6 (dirty tree check) or Step 5 (profile check) must run them as **separate Bash calls** after Call 1.

**Placeholder rule:** `{literal_invoc_id}` is a documentation placeholder Claude Code substitutes before running Bash. Within prompt/context content examples, `$INVOC_ID` means "insert the literal invocation ID while assembling the file content."

**Heredoc for prompt files:** Use single-quoted, invocation-specific delimiters such as `__CODEX_PROMPT_{literal_invoc_id}__` to prevent shell expansion. If the generated delimiter appears in the content, use the Write tool for that file instead.

**Dirty tree check (Step 6):** For edit-mode skills, run as a **separate Bash call** after Call 1 (stdout corruption concern). If dirty and `--allow-dirty` was not passed, stop and wait for user confirmation.

**Profile check (Step 5):** When the skill uses edit-mode, run as a **separate Bash call** after Call 1.

---

### Step 1: Privacy Guardrail (Verbatim)

Defaults to `--search` enabled. When search is on, do NOT paste secrets, tokens, API keys, or proprietary code excerpts directly into the prompt or prompt files. Use file references and let Codex read files directly. Use `--no-search` for sensitive/proprietary code where search could leak context.

### Step 2: Invocation ID and Temp File Convention (Verbatim)

**Invocation ID:** Each skill invocation generates a unique ID:
```bash
INVOC_ID=$(echo "${PPID}-${RANDOM}"); echo "INVOC_ID=$INVOC_ID"
```
Use the literal result as `INVOC_ID` in all subsequent file paths for that invocation.

**File naming conventions:**
- Prompt: `/tmp/codex-{skill}-prompt-$INVOC_ID.md`
- Context (if used): `/tmp/codex-{skill}-context-$INVOC_ID.md`
- Output: `/tmp/codex-{skill}-output-$INVOC_ID.txt`
- Progress/stderr: `/tmp/codex-{skill}-progress-$INVOC_ID.err`
- Per-round (codex-review): `/tmp/codex-review-prompt-r{N}-$INVOC_ID.md`, `/tmp/codex-review-output-r{N}-$INVOC_ID.txt`, etc.

**Failure:** If Write fails, stop with error. No inline fallback for prompt files — inline prompts caused transport stalls this protocol exists to prevent.

### Step 3: Preflight — Mode-Specific Prechecks (Slot)

Each skill defines its own prechecks here (e.g., `codex --version`, mode-specific profile independence notes).

### Step 4: Preflight — CLI Availability (Verbatim)

Confirm Codex CLI is available:
```bash
codex --version > /dev/null 2>&1
```

If unavailable, report "Codex CLI not found. Install it first." and stop.

### Step 5: Preflight — Profile Existence Check (Template)

**Only needed for edit-mode skills** (`codex-implement`, `codex-fix`, `codex-review` edit mode).

Check the profile needed for the invocation mode:

```bash
grep -Eq '^\[profiles\.{PROFILE}\]' ~/.codex/config.toml 2>/dev/null; echo "Profile check ({PROFILE}): $?"
```

Exit 0: profile exists, proceed. Non-zero: report actionable error:
> "Codex profile `{PROFILE}` not found in `~/.codex/config.toml`. Add this block:
> ```toml
> {TOML_BLOCK}
> ```"

### Step 6: Preflight — Dirty Working Tree Check (Template)

Skip if `--allow-dirty` was passed. Only applicable to edit-mode skills.

```bash
git diff --shortstat
git diff --cached --shortstat
git ls-files --others --exclude-standard
```

If any produce non-empty output, warn:
> {DIRTY_TREE_GUIDANCE}

Wait for user confirmation before proceeding.

### Step 7: Pre-Invocation Cleanup (Template)

Remove output/progress files only — do NOT delete prompt or context files:
```bash
rm -f /tmp/codex-{skill}-output*-$INVOC_ID.txt /tmp/codex-{skill}-progress*-$INVOC_ID.err
```

### Step 8: Prompt File Creation (Verbatim)

Write the prompt to `/tmp/codex-{skill}-prompt-$INVOC_ID.md`. The skill's SKILL.md defines the prompt content and template.

For skills with context files (codex-plan, codex-implement, codex-fix): write the context file first, then the prompt file that references it.

**File creation methods (both valid):**
- **Write tool:** Use when creating files as standalone tool calls.
- **Bash heredoc:** Use when combining file creation with invocation in a single Bash call (consolidated execution model).

The key invariant: prompts go through files on disk, never inline CLI args.

### Step 9: Codex Invocation (Template)

**Read-only path:**
```bash
set +e
codex --model gpt-5.3-codex [--search] exec --sandbox read-only --full-auto \
  -C "$(git rev-parse --show-toplevel)" \
  -o /tmp/codex-{skill}-output-$INVOC_ID.txt \
  "Read the prompt at /tmp/codex-{skill}-prompt-$INVOC_ID.md and follow it exactly." \
  2>/tmp/codex-{skill}-progress-$INVOC_ID.err
EC=$?; set -e; echo "Exit code: $EC"; exit "$EC"
```

**Edit-mode path:**
```bash
set +e
codex --model gpt-5.3-codex [--search] exec --profile {PROFILE} --full-auto \
  -C "$(git rev-parse --show-toplevel)" \
  -o /tmp/codex-{skill}-output-$INVOC_ID.txt \
  "Read the prompt at /tmp/codex-{skill}-prompt-$INVOC_ID.md and follow it exactly." \
  2>/tmp/codex-{skill}-progress-$INVOC_ID.err
EC=$?; set -e; echo "Exit code: $EC"; exit "$EC"
```

**Search flag rules:**
- `--search` is a top-level flag — goes BEFORE subcommand: `codex --model gpt-5.3-codex --search exec`
- If user passed `--no-search`: simply omit `--search` from the command
- NEVER invent flags not in the templates above

### Step 10: Background Execution and Polling (Verbatim)

Use the Bash tool's `run_in_background` parameter set to `true` (do NOT set `timeout`). This returns a task ID immediately, enabling parallel skill invocations.

Before reading the output file, call `TaskOutput` with the task ID (`block: true`, `timeout: 600000`) to wait for completion. If still running after the first poll, poll again — complex work may take up to 20 minutes.

**Anti-pattern — NEVER poll `.err` or `.txt` files directly** (via `wc`, `tail`, `grep`, `cat`) to check progress. Use only `TaskOutput` for all monitoring:
- **Waiting for completion:** `TaskOutput(block: true, timeout: 600000)`
- **User asks for status mid-run:** `TaskOutput(block: false)`
- **Error diagnosis (non-zero exit only):** Read `.err` file per Step 13

### Step 11: BETTER_SOLUTION_OUTSIDE_SCOPE Triage (Shared Framework)

If present in Codex output, triage using these rules:

- **Standard risk** (`high_risk: false` — no auth/API contract/safety-critical impact):
  - **Auto-accept** when: verification passes, no HARD no-touch files, same library/app, <=5 outside-scope files
  - **Ask user** when: >5 outside-scope files, cross-library, public API touch, or ownership uncertainty
- **High risk** (`high_risk: true` — touches API contracts, safety-critical hardware code, shared types):
  - **Ask user before accept** by default
  - **Auto-accept only** when: required to satisfy stated acceptance criteria, same library, <=2 outside-scope files
- **Reject/defer** when: HARD no-touch collision

Mode-specific thresholds live in each skill's SKILL.md.

### Step 12: Post-Run Workflow (Shared Steps)

1. **Transport failure gate:** Check exit code and whether output file exists and is non-empty. If non-zero or missing, read stderr, report error, stop.
2. **Read** output file with the Read tool.
3. **Display the full output to the user.** Do NOT summarize or omit sections.
4. **Review the diff** (edit-mode skills only): `git diff` and `git status -sb`
5. Present any BETTER_SOLUTION_OUTSIDE_SCOPE decisions to the user.
6. **(Edit-mode skills only) Verification gate:** Parse VERIFICATION section and run skill-specific verification gate BEFORE presenting keep/discard choice.
7. Ask the user what they want to do: keep all / keep some / discard all.
8. **Do NOT revert changes without explicit user approval** — see CLAUDE.md Git Safety Rules.

### Step 13: Error Handling — Shared Rows

| Scenario | Action |
|----------|--------|
| Prompt file write fails | Stop with error. Do NOT fall back to inline prompt. |
| Codex exits non-zero | Read stderr file, report to user |
| Output file empty or missing | Report "Codex produced no output", show stderr |
| Stalled (no progress >20 min) | Check with `TaskOutput` (block: false). Report to user, suggest narrower scope |
| Codex not installed | Report "Codex CLI not found. Install it first." |
| Auth failure | Report "Codex auth issue, run `codex login`" |
| CONSTRAINT_CONFLICTS in output | HARD no-touch blocks objective. Re-scope or adjust objective, then re-invoke. |
| BETTER_SOLUTION_OUTSIDE_SCOPE | Triage per Step 11. |
| SOFT no-touch zone modified | Review Codex's justification in ASSUMPTIONS. Accept if justified; flag to user if questionable. |

### Step 14: Post-Use Cleanup (Template)

Remove all temp files for this invocation:
```bash
rm -f /tmp/codex-{skill}-prompt*-$INVOC_ID.md /tmp/codex-{skill}-context*-$INVOC_ID.md \
      /tmp/codex-{skill}-output*-$INVOC_ID.txt /tmp/codex-{skill}-progress*-$INVOC_ID.err
```

---

## Part 2: Canonical Prompt Fragments

Named fenced blocks with stable IDs. These are the actual text that goes into runtime prompts. They live HERE ONLY — skills reference them by ID, never duplicate the text.

### Runtime Constraints

#### FRAG-CONVENTIONS
```
Read CLAUDE.md and docs/coding-standards.md for full repo conventions, quality gates, and project standards.
PROJECT: PlatformIO monorepo — ESP32 (C++17), ESP8266 (C++11), AVR (C++11). Arduino framework, NeoPixelBus, ESP-DSP, FreeRTOS.
CONVENTIONS: shared libraries in shared/lib/, apps in apps/, no Arduino String class, no exceptions, pre-allocated buffers, enum class error codes.
GOLD STANDARD: shared/lib/RGBWCommon/src/rgbw.cpp (color math), shared/lib/LEDStrip/src/led_strip.cpp (driver template).
```

#### FRAG-NOTOUCHZONES
```
NO-TOUCH ZONES: HARD = do NOT modify (report CONSTRAINT_CONFLICT if objective requires it). SOFT = avoid if possible, modify with justification in ASSUMPTIONS if needed.
- HARD: platformio.ini — board/framework config, user-managed
- HARD: partition tables (*.csv) — flash layout, user-managed
- HARD: .github/ — CI workflows
- SOFT: docs/*.md — reference docs, avoid churn
- SOFT: tools/dashboard/src-tauri/gen/ — Tauri generated code
```

#### FRAG-VALIDATORS
```
Do NOT weaken existing validators or narrow boundary types. Malformed boundary input must fail fast — no silent defaulting/skipping. Do NOT add code solely to suppress warnings — fix the issue. Use PRIu32/PRId32 from <inttypes.h> for printf format macros (not %lu/%ld). No Arduino String class (heap fragmentation). Pre-allocate all buffers in setup()/constructors — no runtime allocation in loops. Shared libraries MUST be C++11 compatible; guard C++17 with #if __cplusplus >= 201703L.
```

#### FRAG-IMPROVEMENT-CORE
```
The goal is always better code — not preserving existing behavior. When a problem can be eliminated by construction, always choose structural elimination over additive detection.
- Scope expansion with BSOS reporting is expected behavior, not a side effect.
- Improvements in files you're already modifying need no special reporting — just include them in FILES_CHANGED.
- Outside-scope improvements must be reported in BETTER_SOLUTION_OUTSIDE_SCOPE with risk classification: high_risk: true if it touches API contracts, safety-critical hardware code, or shared types, high_risk: false otherwise.
```

#### FRAG-VERIFICATION
```
VERIFICATION:
- STATUS: PASSED | FAILED | SKIPPED | BLOCKED
- COMMANDS_RUN: [list — see skill-specific verification section for scope]
- ATTEMPTS: [0, 1, or 2]
- FAILURE_ATTRIBUTION: CHANGES | BASELINE | ENVIRONMENT | UNKNOWN
- SUMMARY: [one line]
- FAILURE_DETAILS: [required when FAILED]
- SKIP_REASON: [required when SKIPPED]
- BLOCKED_REASON: [required when BLOCKED]
```

#### FRAG-ASSUMPTIONS
```
ASSUMPTIONS: (only if you made judgment calls or touched SOFT no-touch files)
- [Decisions, justifications]
```

#### FRAG-CONSTRAINT-CONFLICTS
```
CONSTRAINT_CONFLICTS: (only if conflicts exist)
- HARD: path — why needed, alternatives tried, impact of not touching
```

#### FRAG-REUSE-DECISIONS
```
REUSE_DECISIONS: (only if you extended/created shared code or rejected an obvious candidate)
- [extended|created|rejected]: path — rationale
```

#### FRAG-BSOS
```
BETTER_SOLUTION_OUTSIDE_SCOPE: (only if you modified files outside SCOPE)
- path — root-cause rationale, alternative if constrained, high_risk: true|false (true if API contracts/safety-critical hardware/shared types), risk delta, ownership/conflict notes
```

---

## Part 3: Skill Overlay Table

| Skill | Steps Used | Fragments Used | Key parameters |
| --- | --- | --- | --- |
| `codex-ask` | Steps 1-4, 9 (read-only), 10, 12-14 | `FRAG-CONVENTIONS` | No profile needed, read-only sandbox, search default ON |
| `codex-plan` | Steps 1-4, 8-10, 12-14 | `FRAG-CONVENTIONS`, `FRAG-NOTOUCHZONES` | No profile needed, read-only sandbox, search default ON |
| `codex-fix` | Steps 1-10, 12-14 | All fragments | Profile: `codex-fix-restricted`, edit-mode, search default OFF |
| `codex-implement` | Steps 1-10, 12-14 | All fragments | Profile: `claude-full`, edit-mode, search default OFF |
| `codex-review` | Steps 1-4, 9-10, 12-14 | `FRAG-CONVENTIONS`, `FRAG-NOTOUCHZONES` | Round 1 read-only, edit-mode when valid issues found |
