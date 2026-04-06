---
name: codex-implement
description: Have Codex write code to implement a feature or change. Use when user says "have Codex implement", "Codex build", "let Codex write", "Codex code this", or when Claude Code has a plan and wants Codex to write the code. Also use for hard algorithmic/architectural problems where Codex's fresh-context advantage matters most.
allowed-tools: Read, Write, Grep, Glob, Bash(echo *), Bash(codex --version *), Bash(codex --model gpt-5.3-codex --search exec --profile claude-full *), Bash(codex --model gpt-5.3-codex exec --profile claude-full *), Bash(cat *), Bash(git diff *), Bash(git status *), Bash(git ls-files *), Bash(INVOC_ID=*), Bash(rm -f /tmp/codex-impl-*), Bash(rm -f /tmp/codex-version-*), Bash(grep *), Bash(pio run *), Bash(pio test *)
---

# Codex Implementation

**Shared protocol:** Read `.claude/skills/shared/codex-execution-protocol.md` for lifecycle steps and canonical fragments (FRAG-CONVENTIONS, FRAG-NOTOUCHZONES, FRAG-VALIDATORS, FRAG-VERIFICATION, FRAG-BSOS, FRAG-IMPROVEMENT-CORE). Read `.claude/skills/shared/codex-antipatterns.md` for failure modes to avoid.

Have Codex write code for planned features, changes, or refactors.

**Honest strengths:**
- **Claude Code**: Orchestration, context management, multi-file coordination, test validation, conversation history
- **Codex**: Writing clean code (DRY, secure, performant), solving hard problems with fresh context, avoiding tunnel-vision solutions

## When to Use

- Claude Code has a plan and wants high-quality code written
- User asks Codex to build/implement/write something
- **Hard problems** — complex algorithms, tricky state machines, non-obvious architectural patterns. Codex's clean-context advantage is biggest on hard problems where a fresh perspective beats accumulated context baggage.
- Refactors where fresh eyes avoid tunnel vision
- A feature slice is well-defined enough for a focused implementation pass

## When NOT to Use

- Ambiguous requirements needing back-and-forth (use codex-ask first)
- Pure bug fixes with narrow scope (use codex-fix)
- Exploratory work where approach isn't decided (use codex-plan first)

## Arguments

- `/codex-implement "Brief description"` — implement with web search disabled (default)
- `/codex-implement --search "Brief description"` — with web search (for external API docs, library references)
- `/codex-implement --slice N` — implement only slice N from a multi-slice brief
- `/codex-implement --allow-dirty` — skip working tree check (user takes responsibility for interleaved diffs)

## Privacy Guardrail

Defaults to `--no-search` (opposite of ask/plan). Implementation touches proprietary code. Enable `--search` only when Codex needs external docs:
- New external API/library usage
- NeoPixelBus / ESP-DSP / FreeRTOS API specifics
- Arduino framework nuances

When `--search` is enabled, do NOT paste secrets or proprietary code into the prompt. Use file references.

## Temp File Convention

Per the shared codex-execution-protocol, use `INVOC_ID` (`${PPID}-${RANDOM}`) for invocation-unique naming. This supports concurrent skill invocations within a session. Get the literal value first via `INVOC_ID="${PPID}-${RANDOM}"; echo "INVOC_ID=$INVOC_ID"`, then use that literal in all file paths. Fallback: `mktemp -d` if process model changes.

## Step 0: Preflight Check

```bash
rm -f /tmp/codex-version-$INVOC_ID.log
codex --version > /tmp/codex-version-$INVOC_ID.log 2>&1; EC=$?; echo "Exit code: $EC"
```

Exit 0: proceed. Non-zero: read log, report error, stop.

**Profile check** (after version passes):
```bash
grep -Eq '^\[profiles\.claude-full\]' ~/.codex/config.toml 2>/dev/null; echo "Profile check: $?"
```

Exit 0: profile exists, proceed. Non-zero: report actionable error:
> "Codex profile `claude-full` not found in `~/.codex/config.toml`. Add this block:
> ```toml
> [profiles.claude-full]
> sandbox_mode = "danger-full-access"
> approval_policy = "on-request"
> ```"

## Step 1: Working Tree Check

Skip if `--allow-dirty` was passed.

```bash
git diff --shortstat
git diff --cached --shortstat
git ls-files --others --exclude-standard
```

If any produce non-empty output, warn:
> "Working tree has uncommitted changes. Codex edits will interleave. Commit or stash first, or re-run with `--allow-dirty`."

Wait for user confirmation before proceeding.

## Step 2: Build the Implementation Brief

This is Claude Code's most important contribution. Write to `/tmp/codex-impl-context-$INVOC_ID.md` using the Write tool (not Bash).

**INVOC_ID note:** The Write tool doesn't expand shell variables. Get the literal INVOC_ID value first via the preflight step, then use that literal in Write tool file paths.

**If Write fails:** stop with error and report the failure to the user. Do NOT fall back to an inline prompt — per the shared codex-execution-protocol, all prompts must go through files.

### Brief Format

```markdown
# Implementation Brief

## OBJECTIVE
[What to build, 2-3 sentences. Specific end result, not journey.]

## IMPLEMENTATION FREEDOM
Codex owns the HOW. This brief defines required outcomes and constraints (WHAT).
Deviate from any suggestion below when you find a better approach — just document why in ASSUMPTIONS.

## ACCEPTANCE CRITERIA
- [ ] [Concrete, testable criterion]

## NON-GOALS
- [What Codex should NOT try to do — frame as outcomes to avoid, not files to avoid]

## API / BEHAVIOR EXAMPLES
[Input/output examples, before/after, expected shapes. Skip if not applicable.]

## SCOPE: Starting Files (not exhaustive)
- `path/to/file.cpp` — [starting point for modification, or "new file"]
- This is the expected starting point, not a hard boundary. Follow root causes beyond these files when it produces a better solution — use BETTER_SOLUTION_OUTSIDE_SCOPE in your output to flag and justify. NO-TOUCH ZONES still apply.

## EXEMPLAR FILES (read these for patterns)
- `shared/lib/RGBWCommon/src/rgbw.cpp` — color math, buffer management, RGBW type patterns
- `shared/lib/RGBWCommon/src/rgbw.h` — type definitions, API surface, C++11 compatible header patterns
- `shared/lib/LEDStrip/src/led_strip.cpp` — NeoPixelBus template driver pattern, strip type abstraction
- `shared/lib/LEDPWM/src/led_pwm.cpp` — PWM channel management, MOSFET control patterns
- `shared/lib/AudioInput/src/audio_input.cpp` — I2S DMA + ESP-DSP FFT pattern (ESP32 only)
- [task-specific exemplars]

## TASK-SPECIFIC CONVENTIONS
[Only conventions specific to this task. See docs/coding-standards.md for full coding standards. Codex can read it directly.]

## NO-TOUCH ZONES
Tiered list. HARD = user-directed or safety-critical, cannot touch without re-approval.
SOFT = Claude Code recommendation, touch with justification if needed for correctness/DRY/completeness.
Format: `TIER: path — reason`

- HARD: platformio.ini — board/framework config, user-managed
- HARD: partition tables (*.csv) — flash layout, user-managed
- HARD: .github/ — CI workflows
- SOFT: docs/*.md — reference docs, avoid churn
- SOFT: tools/dashboard/src-tauri/gen/ — Tauri generated code

## CANDIDATE UTILITIES / PATTERNS
- [Specific utils, types — names and paths]
- These are suggestions, not mandates. If a listed utility isn't fit-for-purpose, choose a better approach and explain why in ASSUMPTIONS.

## RISK PRIORITY
[What MUST be correct even if trade-offs are needed elsewhere. E.g., "color math accuracy is non-negotiable, animation smoothness is flexible."]

## VERIFICATION COMMANDS (Codex MUST run these)
[Commands Codex must execute after implementation. Must be safe, non-destructive, deterministic.]
- `cd /Users/keithmckenzie/Projects/rgbw-lighting/apps/<app-name> && pio run -e esp32 2>&1; echo "Exit: $?"` — ESP32 compile check
- `cd /Users/keithmckenzie/Projects/rgbw-lighting/apps/<app-name> && pio run -e esp8266 2>&1; echo "Exit: $?"` — ESP8266 compile check (if shared lib touched)
- `cd /Users/keithmckenzie/Projects/rgbw-lighting/apps/<app-name> && pio test -e native 2>&1; echo "Exit: $?"` — native unit tests (if tests exist)
- [Focused test command if applicable]

## CONTEXT FROM CONVERSATION
[Decisions, user preferences, constraints Codex can't see. This is the bridge between conversation history and Codex's clean context.]
```

### Brief Quality Checklist (Claude Code self-check)

- Does the objective make sense without conversation context?
- Are acceptance criteria testable, not vague?
- Are all file paths verified to exist (for modifications)?
- Are exemplar files actually relevant?
- Did I SEARCH for existing utilities with Grep/Glob before listing candidates?
- Did I include conversation context Codex can't see?
- Did I specify NON-GOALS as outcomes to avoid (not files to avoid)?
- Did I set RISK PRIORITY for what must be right?
- Am I describing WHAT (outcomes/constraints), not HOW (implementation steps)?
- Are utilities listed as candidates (not mandates), with paths so Codex can evaluate fit?
- If this brief arose from compiler/linter errors, did I include the full error messages?
- Does VERIFICATION COMMANDS include the right build/test commands for affected platforms (ESP32, ESP8266, AVR, or multiple)?
- Are all verification commands safe, non-destructive, and deterministic?
- Is each NO-TOUCH entry minimally scoped (file-level, not broad globs unless justified)?
- Does each NO-TOUCH entry have a reason and correct tier (HARD for user directives, SOFT for Claude recommendations)?
- Does any acceptance criterion conflict with a HARD NO-TOUCH? If so, resolve before sending the brief.
- Is SCOPE framed as starting files, not as a hard boundary? (Codex can follow root causes beyond scope via BETTER_SOLUTION_OUTSIDE_SCOPE)
- Are shared libraries C++11 compatible? Did I note this constraint for shared lib work?

### When to Route Hard Problems to Codex

Claude Code should actively route to `codex-implement` when encountering:
- **Complex algorithms**: color space conversions, FFT windowing, gamma curve generation, beat detection state machines
- **Tricky concurrency**: FreeRTOS task coordination across Core 0/Core 1, ISR-safe ring buffers, DMA buffer ownership
- **Non-obvious architectural patterns**: when the "right" approach isn't the first one that comes to mind
- **Performance-critical paths**: NRZ timing generation, I2S DMA throughput, real-time audio processing
- **Memory-constrained code**: AVR implementations (2KB SRAM), ESP8266 with WiFi overhead

For hard problems, Claude Code may include **prior attempts as non-binding evidence** — what was tried, the observed result, and why it was rejected. Mark these clearly as `NON-BINDING CONTEXT`. Codex should use this to avoid known dead ends, not as required direction.

## Step 3: Invoke Codex

**Prompt-file protocol exception:** The shared codex-execution-protocol requires prompts to go through files. codex-implement uses a hybrid approach: the substantive implementation brief goes through the context file (`/tmp/codex-impl-context-$INVOC_ID.md`), and the inline prompt is a static template that instructs Codex to read that context file. The context file is the authoritative input; the inline portion is just the constraint/verification framework.

**Pre-invocation cleanup** (output files only — do NOT delete context file):
```bash
rm -f /tmp/codex-impl-$INVOC_ID.txt /tmp/codex-impl-$INVOC_ID.err
```

### Default (no web search):
```bash
codex --model gpt-5.3-codex exec --profile claude-full \
  -C /Users/keithmckenzie/Projects/rgbw-lighting \
  -o /tmp/codex-impl-$INVOC_ID.txt \
  "<PROMPT>" 2>/tmp/codex-impl-$INVOC_ID.err; EC=$?; echo "Exit code: $EC"
```

### With web search (`--search`):
```bash
codex --model gpt-5.3-codex --search exec --profile claude-full \
  -C /Users/keithmckenzie/Projects/rgbw-lighting \
  -o /tmp/codex-impl-$INVOC_ID.txt \
  "<PROMPT>" 2>/tmp/codex-impl-$INVOC_ID.err; EC=$?; echo "Exit code: $EC"
```

**Invariant:** The prompt body (`<PROMPT>`) must be byte-identical between variants; only `--search` before `exec` differs.

### Runtime Prompt (`<PROMPT>`)

```
You are implementing code as part of a team. Claude Code wrote the plan; your job is to write great code. Read the implementation brief at /tmp/codex-impl-context-$INVOC_ID.md. The brief specifies WHAT must be true when done. You own the HOW — choose the best implementation approach.

This is a PlatformIO embedded project. See docs/coding-standards.md for full conventions.
PROJECT: PlatformIO monorepo — ESP32 (C++17), ESP8266 (C++11), AVR (C++11). Arduino framework, NeoPixelBus, ESP-DSP, FreeRTOS.
CONVENTIONS: shared libraries in shared/lib/, apps in apps/, no Arduino String class, no exceptions, pre-allocated buffers, enum class error codes.
GOLD STANDARD: shared/lib/RGBWCommon/src/rgbw.cpp (color math), shared/lib/LEDStrip/src/led_strip.cpp (driver template).
LED protocols: SK6812 RGBW (4-channel, 5V, 800kHz NRZ), WS2815B RGB (3-channel, 12V, 800kHz NRZ), 24V non-addressable via MOSFET PWM.
Audio pipeline: I2S DMA → ring buffer → ESP-DSP FFT → band mapping → beat detection → BPM tracking.

HARD CONSTRAINTS:
- Start with files listed in the brief's SCOPE. You may modify additional files when root-cause analysis shows the best fix is elsewhere. You MUST report every file modified outside SCOPE in BETTER_SOLUTION_OUTSIDE_SCOPE with justification — unreported outside-SCOPE modifications are treated as unexpected. NO-TOUCH ZONES still apply regardless.
- NO-TOUCH ZONES: HARD = do NOT modify (report CONSTRAINT_CONFLICT if objective requires it). SOFT = avoid if possible, modify with justification in ASSUMPTIONS if needed.
- Use exemplar files for repo conventions. Deviate when a better approach serves correctness, performance, or safety — document why.
- If the brief conflicts with repo conventions, follow repo conventions and report the conflict.
- Respect RISK PRIORITY — get those right even if other areas need trade-offs.
- Shared libraries MUST be C++11 compatible. Guard C++17 features with #if __cplusplus >= 201703L.
- Use PRIu32/PRId32 from <inttypes.h> for printf format macros (not %lu/%ld).
- No Arduino String class (heap fragmentation). Use char[] + snprintf().
- Pre-allocate all buffers in setup()/constructors. No runtime allocation in loops.

VERIFICATION:
- After all code edits, run the commands in the brief's VERIFICATION COMMANDS section.
- If verification fails due to your changes, fix and re-run once (max 2 total attempts). Do not retry for pre-existing or environment failures.

REQUIRED OUTPUT FORMAT:
FILES_CHANGED:
- path/to/file.cpp — [what changed]

FILES_CREATED:
- path/to/new.cpp — [why needed]

ACCEPTANCE_CRITERIA_STATUS:
- [x] Criterion — [how satisfied]
- [ ] Criterion — [why not, what's needed]

ASSUMPTIONS:
- [Decisions you made that weren't explicit in the brief]

REUSE_DECISIONS: (only if you extended/created shared code or rejected an obvious candidate)
- [extended|created|rejected]: path — rationale

CONSTRAINT_CONFLICTS: (only if conflicts exist)
- HARD: path — why needed, alternatives tried, impact of not touching

BETTER_SOLUTION_OUTSIDE_SCOPE: (only if you modified files outside SCOPE)
- path — root-cause rationale, alternative if constrained, risk delta, ownership/conflict notes

UNRESOLVED:
- [Questions, plan problems, incomplete items]

RISKS:
- [Potential breakage, uncertain edge cases, areas needing extra testing]

VERIFICATION:
- STATUS: PASSED | FAILED | SKIPPED | BLOCKED
- COMMANDS_RUN: [list]
- ATTEMPTS: [0, 1, or 2]
- FAILURE_ATTRIBUTION: CHANGES | BASELINE | ENVIRONMENT | UNKNOWN
- SUMMARY: [one line]
- FAILURE_DETAILS: [required when FAILED]
- SKIP_REASON: [required when SKIPPED]
- BLOCKED_REASON: [required when BLOCKED]
```

Use the Bash tool's `run_in_background` parameter set to `true` (do NOT set `timeout`). This returns a task ID immediately, enabling parallel skill invocations (e.g., launching codex-review and gemini-review concurrently without waiting).

Before reading the output file, call `TaskOutput` with the task ID (`block: true`, `timeout: 600000`) to wait for completion. If still running after the first poll, poll again — complex implementations may take up to 20 minutes.

## Step 4: After Codex Finishes

1. **Read** `/tmp/codex-impl-$INVOC_ID.txt` with the Read tool
2. **Review the diff:** `git diff` and `git status -sb`
3. **Validate acceptance criteria** against Codex's self-assessment
4. **Verification gate:**
   - Parse Codex's VERIFICATION section
   - If STATUS=PASSED: skip re-running verification for normal single-slice flows
   - If STATUS=FAILED: Claude Code runs verification independently. Attribution informs triage:
     - CHANGES: review Codex's fix attempts, decide whether to re-invoke Codex or fix directly
     - BASELINE: note pre-existing failures, do not block on them
     - ENVIRONMENT: note infra issue, report to user, do not block on it
     - UNKNOWN: use Claude's independent run to determine true attribution
   - If STATUS=BLOCKED or VERIFICATION missing: Claude Code runs verification and triages
   - If STATUS=SKIPPED with code changes present: Claude Code runs verification
   - **Always re-run Claude-side for:** safety-critical changes, final multi-slice integration handoff
5. **BETTER_SOLUTION_OUTSIDE_SCOPE triage** (if present in Codex output):
   - **Auto-accept** when: no HARD no-touch files involved, low blast radius (same library), no API contract change
   - **Ask user** when: product behavior changes, cross-library impact, changes to shared types in RGBWCommon
   - **Reject/defer** when: collides with HARD no-touch constraints or active parallel session ownership
   - **Default** when ownership is uncertain: ask user
   - **Re-scoping checkpoint**: if BETTER_SOLUTION_OUTSIDE_SCOPE includes more than 3 files, Claude Code must pause and ask the user regardless of blast-radius assessment — the change has grown beyond the original brief
6. **Integration check** (Claude Code's strength):
   - Convention compliance (C++11 compat in shared libs, naming conventions, no Arduino String)
   - Type safety (RGBW/HSV types, enum class usage)
   - Integration with existing code (library includes, platformio.ini lib_deps, API compatibility)
   - Test coverage needs
7. **Fix integration seams** with a threshold rule:
   - **Claude Code handles directly** (~20 LOC or less, no logic): wiring includes, adding lib_deps, re-exporting types, updating header guards
   - **Re-invoke Codex** (>~20 LOC or touches logic): non-trivial integration, writing adapter code, adding new color conversions, state machine transitions. Write a focused follow-up brief for the integration slice.
8. **Present to user:** diff, Codex's summary, any BETTER_SOLUTION_OUTSIDE_SCOPE decisions (with rationale), Claude Code's integration notes, unresolved items
9. **Ask user:** keep all / keep some / discard all
10. **NEVER revert files** — multiple Claude Code sessions share the working tree. `git checkout --`, `git restore`, `git checkout .` will destroy other sessions' work. If Codex modified files that are in neither SCOPE nor BETTER_SOLUTION_OUTSIDE_SCOPE, treat as unexpected and ask user. Only validate changes within your brief's SCOPE files plus any accepted BETTER_SOLUTION_OUTSIDE_SCOPE files. Filter diffs to relevant paths.

## Step 5: Cleanup

```bash
rm -f /tmp/codex-impl-$INVOC_ID.txt /tmp/codex-impl-$INVOC_ID.err /tmp/codex-impl-context-$INVOC_ID.md /tmp/codex-version-$INVOC_ID.log
```

## Multi-Slice Implementation

For larger features, Claude Code breaks work into slices. Each slice gets its own brief.

### Slicing Strategy
- Slice by **functional boundary**, not by file (each slice independently testable)
- Target: <=8 files per slice, 300-800 LOC as soft guide (not hard ceiling)
- A cohesive 700 LOC slice is better than two artificial 350 LOC splits
- Order slices so earlier ones don't depend on later ones

### Slice Workflow
1. Claude writes Brief for Slice 1 -> Codex implements -> Claude validates + integrates
2. Claude writes Brief for Slice 2 (updated context) -> Codex implements -> Claude validates
3. Repeat until complete
4. Run `codex-review` on the full change set

### Between Slices
- Require per-slice VERIFICATION from Codex; if missing, FAILED, BLOCKED, or SKIPPED with code changes present, Claude Code runs verification before starting next slice
- Update next brief with integration changes Claude made
- If Codex's approach in Slice N changes the plan for N+1, update accordingly

## Scope Guardrails

- Single slice >800 LOC or >8 files: warn, suggest splitting
- Full feature >1500 LOC: require multi-slice
- Safety-critical changes (ISR, DMA, power management): auto-escalate to Tier 2 review after implementation

## Error Handling

| Scenario | Action |
|----------|--------|
| Context file write fails | Stop with error. Do NOT fall back to inline prompt. Report failure to user. |
| Codex exits non-zero | Read `/tmp/codex-impl-$INVOC_ID.err`, report to user |
| Output file empty | Report "Codex produced no output", show stderr |
| Stalled (no progress >20 min) | Check with `TaskOutput` (block: false). If still running, report to user, suggest narrower scope or splitting into slices |
| Codex not installed | Report "Codex CLI not found. Install it first." |
| Auth failure | Report "Codex auth issue, run `codex login`" |
| Criteria partially met | Claude completes remaining items, or re-invoke with narrower scope |
| Unexpected files created | Flag to user, don't auto-delete |
| HARD no-touch zone modified | **NEVER revert with git checkout/restore** — multiple sessions share the working tree. Report the HARD violation to the user and let them decide. Only validate changes within your brief's SCOPE files plus any accepted BETTER_SOLUTION_OUTSIDE_SCOPE files. |
| Codex modifies files outside SCOPE | If listed in BETTER_SOLUTION_OUTSIDE_SCOPE, triage per Step 4.5. If NOT listed in BETTER_SOLUTION_OUTSIDE_SCOPE, treat as unexpected — ask user before accepting. Do NOT revert — other sessions may have legitimate in-progress changes. |
| SOFT no-touch zone modified | Review Codex's justification in ASSUMPTIONS. Accept if justified; flag to user if questionable. |
| CONSTRAINT_CONFLICT reported | Codex stopped because a HARD no-touch blocks the objective. Re-scope the NO-TOUCH ZONES or adjust the objective, then re-invoke. |
| BETTER_SOLUTION_OUTSIDE_SCOPE reported | Codex found a better root-cause fix outside the brief's SCOPE and implemented it. Triage per Step 4.5: auto-accept (no HARD files, low blast radius, same library, no API contract change), ask user (behavior changes, cross-library impact, shared type changes, concurrent-session conflict risk, uncertain ownership), or reject/defer (HARD constraint collision, active parallel session ownership). |
| Brief conflicts with repo conventions | Codex follows repo conventions and reports the conflict |

## Notes

- `--model gpt-5.3-codex` is required — always include for model targeting
- `--search` is a top-level flag — goes BEFORE subcommand: `codex --model gpt-5.3-codex --search exec`
- Each invocation is stateless — the brief IS the context
- Codex may start its own MCP servers if configured in `~/.codex/config.toml`
- Stderr is noisy (MCP startup, rollout errors); captured separately in `.err` file
- Uses `--profile claude-full` (defined in `~/.codex/config.toml`) — provides full access with on-request approval
- Default is `--no-search` because implementation touches proprietary code

## Codex -> Claude Code Mirror

**Note:** Mirror commands use `claude` CLI which is not in this skill's allowed-tools. The mirror section is reference documentation for when Codex orchestrates Claude Code — it is not executed by this skill directly.

If Codex is orchestrating implementation but you want Claude Code to perform the coding pass, use this mirror.

### Preflight

```bash
rm -f /tmp/claude-version-$INVOC_ID.log
claude --version > /tmp/claude-version-$INVOC_ID.log 2>&1; EC=$?; echo "Exit code: $EC"
```

### Invocation (edit-capable, JSON)

```bash
rm -f /tmp/claude-impl-$INVOC_ID.json /tmp/claude-impl-$INVOC_ID.err
claude -p --permission-mode acceptEdits --output-format json \
  "Read /tmp/codex-impl-context-$INVOC_ID.md and implement the brief. Return STRICT JSON with files_changed, files_created, acceptance_criteria_status, assumptions, unresolved, risks, verification." \
  > /tmp/claude-impl-$INVOC_ID.json 2>/tmp/claude-impl-$INVOC_ID.err; EC=$?; echo "Exit code: $EC"
```

### Notes

- Use `--permission-mode acceptEdits` for normal coding; escalate to `--permission-mode bypassPermissions` only when blocked by prompts.
- Keep the brief file contract unchanged so Codex and Claude delegates can share orchestration.
- Claude Code has no top-level `--search` toggle like Codex.
