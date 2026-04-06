---
name: codex-plan
description: Review an implementation plan with Codex before finalizing. Use during Plan Mode to get Codex's feedback on the plan, architectural decisions, trade-offs, and potential issues. Should be used before calling ExitPlanMode.
allowed-tools: Read, Write, Bash(codex *), Bash(echo *), Bash(INVOC_ID=*), Bash(cat *), Bash(rm -f /tmp/codex-plan-*), Bash(rm -f /tmp/codex-version-*)
---

# Codex Plan Review

**Shared protocol:** Read `.claude/skills/shared/codex-execution-protocol.md` for lifecycle steps and canonical fragments. Read `.claude/skills/shared/codex-antipatterns.md` for failure modes to avoid.

Review an implementation plan with Codex before finalizing. Use during Plan Mode to get a second opinion on architectural decisions, trade-offs, and potential issues.

## When to Invoke

- Automatically before finalizing any plan (before ExitPlanMode)
- When Claude Code wants a second opinion on a specific architectural decision during planning
- When the user says "check this plan with Codex" or "get Codex feedback on the plan"

## Arguments

- `/codex-plan` — review the current plan with web search enabled (default)
- `/codex-plan --no-search` — review without web search (recommended for sensitive/proprietary plans)
- `/codex-plan --json` — request strict JSON output (prompt-level; see Structured JSON Output). Mutually exclusive with `--rewrite`.
- `/codex-plan --rewrite` — Codex returns revised plan sections (not just suggestions). Use when Codex's feedback is substantial enough that Claude rewriting from suggestions is wasteful. Claude Code still integrates the rewritten sections and retains final plan ownership. Mutually exclusive with `--json`.

## Privacy Guardrail

When `--search` is enabled (the default), do NOT paste secrets, tokens, API keys, or proprietary code excerpts directly into the prompt or into `/tmp/codex-plan-context-$INVOC_ID.md`. The same privacy rules that apply to inline prompts apply equally to the context file. For sensitive plans, recommend `--no-search`.

## Temp File Convention

Per the shared codex-execution-protocol, use `INVOC_ID` (`${PPID}-${RANDOM}`) for invocation-unique naming. This supports concurrent skill invocations within a session. Get the literal value first via `INVOC_ID="${PPID}-${RANDOM}"; echo "INVOC_ID=$INVOC_ID"`, then use that literal in all file paths. `$PPID` is stable across all Bash tool calls within a session and unique across concurrent sessions. If Anthropic changes Claude Code's process model and `$PPID` stops being stable, migrate to `mktemp -d` approach.

## Step 0: Preflight Check

Run once at skill start:

```bash
rm -f /tmp/codex-version-$INVOC_ID.log
codex --version > /tmp/codex-version-$INVOC_ID.log 2>&1; EC=$?; echo "Exit code: $EC"
```

- Exit code 0: Codex is available, proceed.
- Exit code non-zero or "command not found": Read `/tmp/codex-version-$INVOC_ID.log`, report the error, and stop.

## Plan Review Protocol

1. **Write the context file** using the Write tool (not Bash) to `/tmp/codex-plan-context-$INVOC_ID.md` with:
   - The user's original request
   - The full plan text
   - Specific questions Claude Code has (decision trade-offs, alternative approaches, concerns)

   Use the format described in "Context File Format" below.

2. **If the Write tool fails** (permission error, disk full, etc.): stop with error and report the file-write failure to the user. Do NOT fall back to an inline prompt — per the shared codex-execution-protocol, all prompts must go through files.

3. Invoke Codex with a short prompt that references the context file
4. Read and evaluate Codex's response
5. Incorporate improvements into the plan
6. If significant changes were made, optionally run one more round for confirmation
7. Then call ExitPlanMode

## Context File Format

The context file written to `/tmp/codex-plan-context-$INVOC_ID.md` uses stable section headers that Codex can parse:

```markdown
# Plan Review Context

## USER'S ORIGINAL REQUEST:
[What the user asked for]

## PROPOSED PLAN:
[Full plan content]

## SPECIFIC QUESTIONS FROM CLAUDE CODE:
[Any decisions Claude Code wants a second opinion on]
```

## Invocation

**Prompt-file protocol exception:** The shared codex-execution-protocol requires prompts to go through files. codex-plan uses a hybrid approach: the substantive plan content goes through the context file (`/tmp/codex-plan-context-$INVOC_ID.md`), but the review instructions are passed inline. This is acceptable because the inline portion is a static template (not user-varying content) and the plan data already follows the prompt-file protocol. The context file is the authoritative input; the inline prompt is just the review framework.

**Pre-invocation cleanup** (output files only — do NOT delete context file):
```bash
rm -f /tmp/codex-plan-$INVOC_ID.txt /tmp/codex-plan-$INVOC_ID.err
```

### With web search (default):
```bash
codex --model gpt-5.3-codex --search exec --sandbox read-only --full-auto \
  -C /Users/keithmckenzie/Projects/rgbw-lighting \
  -o /tmp/codex-plan-$INVOC_ID.txt \
  "You are reviewing an implementation plan created by Claude Code. Read the full plan context at /tmp/codex-plan-context-$INVOC_ID.md — it contains the user request, the proposed plan, and specific questions. Use the REQUIRED OUTPUT FORMAT specified below.

PROJECT CONTEXT: PlatformIO monorepo — ESP32/ESP8266/AVR, C++17/C++11, Arduino framework, NeoPixelBus, ESP-DSP, FreeRTOS. Shared libraries in shared/lib/ (RGBWCommon, LEDStrip, LEDPWM, AudioInput, Connectivity), apps in apps/. See docs/ for full specs (led-control.md, audio-reactive.md, esp32-internals.md, coding-standards.md, pin-reference.md).

REVIEW FOR:
- Correctness: Will this approach actually work? Any logical flaws? Does it align with project conventions in docs/coding-standards.md?
- Completeness: Are there missing steps, edge cases, or files?
- Architecture: Are the design decisions sound? Consistent with shared library patterns (RGBWCommon → LEDStrip/LEDPWM dependency graph)?
- Memory safety: Stack overflow risk, heap fragmentation, buffer overruns, DMA buffer alignment, SRAM budget on AVR (2KB)?
- Hardware interaction: GPIO safety (strapping pins, input-only, ADC2+WiFi conflict), timing constraints, power budget?
- Concurrency: FreeRTOS task priorities, ISR safety, mutex usage, Core 0 vs Core 1 task pinning?
- Platform portability: C++11 compatibility in shared libs, #if __cplusplus guards, printf format macros (PRIu32)?
- Real-time: Loop timing, vTaskDelayUntil(), blocking calls in time-critical paths (LED show, audio DMA)?
- DRY: Reuse existing shared utilities? Risk of duplicating logic across apps?
- Performance: NRZ timing tolerances (800kHz), I2S DMA throughput, PWM resolution trade-offs?
- Risk: What could go wrong? Rollback strategy?
- Existing code: Reuse existing patterns (RGBWCommon color math, LEDStrip template driver, LEDPWM channels)?

REQUIRED OUTPUT FORMAT (use exactly these sections; --rewrite mode adds extra sections):
ASSESSMENT: APPROVE | NEEDS_REVISION

KEY_IMPROVEMENTS:
1. [HIGH|MEDIUM|LOW] Description of improvement
   Suggestion: What to change in the plan

QUESTIONS_ANSWERED:
- [Question]: [Codex's answer/recommendation]

ALTERNATIVE_APPROACHES:
- [If any significantly better approach exists, describe it]" 2>/tmp/codex-plan-$INVOC_ID.err; EC=$?; echo "Exit code: $EC"
```

### Without web search (`--no-search`):
```bash
codex --model gpt-5.3-codex exec --sandbox read-only --full-auto \
  -C /Users/keithmckenzie/Projects/rgbw-lighting \
  -o /tmp/codex-plan-$INVOC_ID.txt \
  "You are reviewing an implementation plan created by Claude Code. Read the full plan context at /tmp/codex-plan-context-$INVOC_ID.md — it contains the user request, the proposed plan, and specific questions. Use the REQUIRED OUTPUT FORMAT specified below.

PROJECT CONTEXT: PlatformIO monorepo — ESP32/ESP8266/AVR, C++17/C++11, Arduino framework, NeoPixelBus, ESP-DSP, FreeRTOS. Shared libraries in shared/lib/ (RGBWCommon, LEDStrip, LEDPWM, AudioInput, Connectivity), apps in apps/. See docs/ for full specs (led-control.md, audio-reactive.md, esp32-internals.md, coding-standards.md, pin-reference.md).

REVIEW FOR:
- Correctness: Will this approach actually work? Any logical flaws? Does it align with project conventions in docs/coding-standards.md?
- Completeness: Are there missing steps, edge cases, or files?
- Architecture: Are the design decisions sound? Consistent with shared library patterns (RGBWCommon → LEDStrip/LEDPWM dependency graph)?
- Memory safety: Stack overflow risk, heap fragmentation, buffer overruns, DMA buffer alignment, SRAM budget on AVR (2KB)?
- Hardware interaction: GPIO safety (strapping pins, input-only, ADC2+WiFi conflict), timing constraints, power budget?
- Concurrency: FreeRTOS task priorities, ISR safety, mutex usage, Core 0 vs Core 1 task pinning?
- Platform portability: C++11 compatibility in shared libs, #if __cplusplus guards, printf format macros (PRIu32)?
- Real-time: Loop timing, vTaskDelayUntil(), blocking calls in time-critical paths (LED show, audio DMA)?
- DRY: Reuse existing shared utilities? Risk of duplicating logic across apps?
- Performance: NRZ timing tolerances (800kHz), I2S DMA throughput, PWM resolution trade-offs?
- Risk: What could go wrong? Rollback strategy?
- Existing code: Reuse existing patterns (RGBWCommon color math, LEDStrip template driver, LEDPWM channels)?

REQUIRED OUTPUT FORMAT (use exactly these sections; --rewrite mode adds extra sections):
ASSESSMENT: APPROVE | NEEDS_REVISION

KEY_IMPROVEMENTS:
1. [HIGH|MEDIUM|LOW] Description of improvement
   Suggestion: What to change in the plan

QUESTIONS_ANSWERED:
- [Question]: [Codex's answer/recommendation]

ALTERNATIVE_APPROACHES:
- [If any significantly better approach exists, describe it]" 2>/tmp/codex-plan-$INVOC_ID.err; EC=$?; echo "Exit code: $EC"
```

Use the Bash tool's `run_in_background` parameter set to `true` (do NOT set `timeout`). This returns a task ID immediately, enabling parallel skill invocations (e.g., launching codex-plan and gemini-ask concurrently without waiting).

Before reading the output file, call `TaskOutput` with the task ID (`block: true`, `timeout: 600000`) to wait for completion. If still running after the first poll, poll again.

## Structured JSON Output (Optional)

If `--json` is requested, add this to the prompt (after the review checklist, before closing the prompt string):

```
Return STRICT JSON only. Schema:
{
  "assessment": "APPROVE|NEEDS_REVISION",
  "key_improvements": [
    {"severity": "HIGH|MEDIUM|LOW", "issue": "description", "suggestion": "what to change"}
  ],
  "questions_answered": [
    {"question": "text", "answer": "text"}
  ],
  "alternative_approaches": ["description"],
  "overall_confidence": 0.0
}
```

If JSON is invalid, report a parse failure and summarize the raw output.

## Rewrite Mode (`--rewrite`)

When `--rewrite` is requested, append this to the prompt (after the review checklist):

```
In addition to the standard review output, provide a REVISED_PLAN section.
For any section you would change, provide the rewritten text.
For sections that are fine as-is, write "NO CHANGES".

REVISED_PLAN:
## [Section Name]
[Rewritten section text, or "NO CHANGES"]

PLAN_DIFF_SUMMARY:
- [Section]: [1-line summary of what changed and why]
```

### Evaluating Rewrite Output

1. Compare each revised section against the original
2. Accept revisions that are clearly better (simpler, more correct, more complete)
3. For revisions that change direction or add scope: present both versions to the user
4. Claude Code integrates accepted revisions into the plan — Codex doesn't have plan-file write access
5. Codex lacks conversation context, so reject revisions that contradict user decisions discussed earlier (note the contradiction when presenting to user)

## Evaluating Codex Feedback

After reading Codex's response:

### If ASSESSMENT: APPROVE
- Note any LOW/MEDIUM improvements worth incorporating
- Proceed to ExitPlanMode
- Mention to user: "Codex reviewed and approved this plan."

### If ASSESSMENT: NEEDS_REVISION with HIGH-priority improvements
1. Address all HIGH improvements in the plan
2. Consider MEDIUM improvements (incorporate if clearly better, note trade-offs otherwise)
3. Optionally run a second round to confirm revisions (based on severity of changes)
4. Present the final plan to the user with a summary:
   > "Codex reviewed this plan and suggested N improvements. Key changes: [list]. Remaining trade-off: [if any]."

### For trade-off disagreements
- Present both Claude Code's and Codex's perspectives to the user
- Clearly label which view belongs to which agent
- Let the user decide

### For factual corrections
- Fix immediately in the plan — no need to ask

### For style/approach preferences
- Note Codex's alternative
- Keep Claude Code's choice with justification

## After Invocation

1. Read `/tmp/codex-plan-$INVOC_ID.txt` with the Read tool
2. Evaluate Codex's response per the rules above
3. If the file is empty or Codex exited non-zero, read `/tmp/codex-plan-$INVOC_ID.err` and report the error
4. **Post-use cleanup** — remove temp files to avoid leaking context:
   ```bash
   rm -f /tmp/codex-plan-$INVOC_ID.txt /tmp/codex-plan-$INVOC_ID.err /tmp/codex-plan-context-$INVOC_ID.md
   ```

## Error Handling

| Scenario | Action |
|----------|--------|
| Context file write fails | Stop with error. Do NOT fall back to inline prompt. Report failure to user. |
| Codex exits non-zero | Read `/tmp/codex-plan-$INVOC_ID.err`, report to user, proceed with plan as-is |
| Output file empty | Report "Codex produced no output", proceed with plan as-is |
| Stalled (no progress >20 min) | Check with `TaskOutput` (block: false). If still running, report to user, proceed with plan as-is |
| Parse failure (no structured format) | Read raw text, extract key points manually, summarize for user |
| Codex not installed | Report "Codex CLI not found", proceed with plan as-is |

## Notes

- `--model gpt-5.3-codex` is required — always include for model targeting
- `--search` is a top-level flag — goes BEFORE subcommand: `codex --model gpt-5.3-codex --search exec` (NOT `codex exec --search`)
- Each invocation is stateless — no conversation context carried
- Codex may start its own MCP servers if configured in `~/.codex/config.toml`
- Stderr is noisy (MCP startup, rollout errors); captured separately in `.err` file
- Plan review is non-blocking: if Codex fails or is unavailable, proceed with the plan and inform the user

## Codex -> Claude Code Mirror

**Note:** Mirror commands use `claude` CLI which is not in this skill's allowed-tools. The mirror section is reference documentation for when Codex orchestrates Claude Code — it is not executed by this skill directly.

If Codex is orchestrating plan review but you want Claude Code as the reviewer, use this non-interactive mirror.

### Preflight

```bash
rm -f /tmp/claude-version-$INVOC_ID.log
claude --version > /tmp/claude-version-$INVOC_ID.log 2>&1; EC=$?; echo "Exit code: $EC"
```

### Invocation (review only, JSON)

```bash
rm -f /tmp/claude-plan-$INVOC_ID.json /tmp/claude-plan-$INVOC_ID.err
claude -p --permission-mode plan --output-format json --max-turns 1 \
  "You are reviewing an implementation plan. Read /tmp/codex-plan-context-$INVOC_ID.md and return STRICT JSON with: assessment, key_improvements, questions_answered, alternative_approaches." \
  > /tmp/claude-plan-$INVOC_ID.json 2>/tmp/claude-plan-$INVOC_ID.err; EC=$?; echo "Exit code: $EC"
```

### Notes

- `--permission-mode plan` keeps this review-oriented and non-editing.
- Claude Code has no top-level `--search` toggle like Codex.
- For iterative rounds on the same plan, continue context with `claude -p --resume <session-id> "re-review after edits"`.
