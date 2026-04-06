---
name: gemini-review
description: Advisory Gemini code review for medium+ change sets (>5 files or >200 LOC). Auto-invoke at end of session when change size is medium+ or when the user asks for a Gemini review. Inline output by default, with optional structured JSON-file mode.
allowed-tools: Read, Write, Grep, Glob, Bash(gemini *), Bash(echo *), Bash(INVOC_ID=*), Bash(git status *), Bash(git diff *), Bash(git ls-files *), Bash(git rev-parse *), Bash(git symbolic-ref *), Bash(rm -f /tmp/gemini-review-*)
---

# Gemini Review (Advisory Last-10%)

**Shared protocol:** Read `.claude/skills/shared/gemini-execution-protocol.md` for lifecycle steps and canonical fragments (GFRAG-ROLE, GFRAG-ANTIPATTERNS, GFRAG-FINDING-SCHEMA, GFRAG-INTENT-BLOCK, GFRAG-CONFIDENCE, GFRAG-JSON-RULES, GFRAG-TRIAGE). Read `.claude/skills/shared/gemini-antipatterns.md` and `.claude/skills/shared/gemini-finding-schema.md` for output standards. Route disputed findings through `/review-findings`.

Run a Gemini advisory review pass AFTER codex-review. Gemini's role is catching the last 10% of edge cases, subtle issues, and risks that Claude Code and Codex missed. All findings are hypotheses requiring Claude+Codex validation. Do not edit files.

## Arguments

- `/gemini-review` — return findings inline in terminal output (default)
- `/gemini-review --json-file` — machine-parseable JSON file flow

## Trigger

- Auto-invoke when changes exceed 5 files or 200 LOC (Tier 2) OR involve hardware/safety-critical changes, AFTER codex-review completes.
- Invoke on explicit user request even if smaller.
- Invoke as SECONDARY advisory for memory safety, ISR, or DMA changes (Codex is primary via codex-ask/codex-plan at PLAN phase).
- Skip if no changes are detected **unless** this is a pre-implementation (plan-phase) review.

## Scope Discovery

Use git to collect a minimal scope summary before invoking:

- `git status -sb`
- `git diff --name-only`
- `git diff --cached --name-only`
- `git ls-files --others --exclude-standard`
- `git diff --shortstat`
- `git diff --cached --shortstat`

If all diffs and untracked lists are empty, stop and report "No changes to review," **unless** this is a pre-implementation (plan-phase) review. For plan-phase reviews, you must still provide an explicit planned file list in the prompt.

## Plan-Phase Review (High-Risk Changes)

When invoked pre-implementation (ISR/DMA/memory safety):
- Provide an explicit planned file list from the plan/intent (no guesswork).
- Use `Changed files: none (pre-implementation review)` in SCOPE SUMMARY.
- Keep diff stats lines present with `(none; pre-implementation)`.

## Temp File Convention

Per the shared gemini-execution-protocol, use `INVOC_ID` (`${PPID}-${RANDOM}`) for invocation-unique naming. This supports concurrent skill invocations within a session. Get the literal value first via `INVOC_ID="${PPID}-${RANDOM}"; echo "INVOC_ID=$INVOC_ID"`, then use that literal in all file paths. The Read tool does not expand shell variables — use the numeric literal in Read paths. Fallback: `mktemp -d` if process model changes.

## Preflight

- Run `gemini --version` (or `gemini -h`) to confirm the CLI is available.
- If the command fails, report the error and stop.

## Approval-Mode Guardrail

- Preferred mode is `--approval-mode plan` for read-only review behavior.
- If Gemini exits with `Approval mode "plan" is only available when experimental.plan is enabled`, stop and ask user to enable it in `~/.gemini/settings.json`:
  ```json
  {
    "experimental": {
      "plan": true
    }
  }
  ```
- Do **not** silently fall back to `--yolo`. Only use `yolo` if the user explicitly asks for that risk tradeoff.

## Invocation

### JSON-file mode (`--json-file`)

**Pre-invocation cleanup** (prevents stale reads from prior runs):
```bash
rm -f /tmp/gemini-review-$INVOC_ID.json /tmp/gemini-review-$INVOC_ID.err
```

**Write prompt to file first** (per shared gemini-execution-protocol — all prompts go through files, never inline `-p`):
Write prompt content to `/tmp/gemini-review-prompt-$INVOC_ID.md` via Write tool or heredoc.

```bash
gemini --approval-mode plan -m gemini-3.1-pro-preview --include-directories /tmp --include-directories $HOME/.claude/plans --output-format json -p "Read the prompt at /tmp/gemini-review-prompt-$INVOC_ID.md and follow it exactly." > /tmp/gemini-review-$INVOC_ID.json 2>/tmp/gemini-review-$INVOC_ID.err; EC=$?; echo "Exit code: $EC"
```

Use the Bash tool's `run_in_background` parameter set to `true` (do NOT set `timeout`) in `json-file` mode. This returns a task ID immediately, enabling parallel skill invocations (e.g., launching codex-review and gemini-review concurrently without waiting).

Before reading the output file, call `TaskOutput` with the task ID (`block: true`, `timeout: 600000`) to wait for completion. If still running after the first poll, poll again — reviews may take up to 10 minutes.

### Inline mode (default)

Write prompt to `/tmp/gemini-review-prompt-$INVOC_ID.md`, then run in foreground:
```bash
gemini --approval-mode plan -m gemini-3.1-pro-preview --include-directories /tmp --include-directories $HOME/.claude/plans --output-format text -p "Read the prompt at /tmp/gemini-review-prompt-$INVOC_ID.md and follow it exactly."
```

Do not redirect stdout in inline mode.

## No-Search Mode (If Supported)

If the Gemini CLI supports disabling search (check `gemini -h`), add the appropriate flag (e.g., `--no-search`) to the invocation. If no such flag exists, add a boundary in the prompt: "Do not use web search," and ensure `sources:[]` or equivalent in the response.

## Prompt Template (strict)

```
You are a senior code reviewer. Advisory-only; do not modify files.
Treat this as a safety inspection after changes. Return STRICT JSON only (no markdown, no prose outside JSON).

OUTPUT JSON SCHEMA (schema-first):
{
  "summary": {"critical":0,"high":0,"medium":0,"low":0},
  "issues": [
    {
      "id": "GR-1",
      "severity": "critical|high|medium|low",
      "file": "path/to/file.cpp",
      "line": 123,
      "title": "Short issue title",
      "detail": "Why this is an issue",
      "suggestion": "How to fix",
      "confidence": 0.0
    }
  ],
  "overall_confidence": 0.0,
  "verdict": "pass|needs_work",
  "dry_check": "pass|needs_work",
  "shared_candidates": ["paths or empty array"],
  "notes": "optional"
}

DRY_CHECK semantics: "needs_work" when any issue involves duplicated logic, missed shared-utility reuse, feature-local code that should be shared, or parallel implementations of the same rule. "pass" when no such issues found.

INTENT SUMMARY (required):
- <short summary of what changed and why>

BOUNDARIES:
- <what to ignore or exclude>

UNCERTAINTIES:
- <explicit doubts or areas to double-check>

SCOPE SUMMARY (explicit file list + diff stats, do not infer):
- Changed files: <list>
- Untracked files: <list or none>
- Diff stats: <git diff --shortstat + git diff --cached --shortstat>
- Planned files (if pre-implementation): <list or none>

PROJECT CONTEXT:
- PlatformIO monorepo — ESP32 (C++17), ESP8266 (C++11), Arduino AVR (C++11). Arduino framework, NeoPixelBus, ESP-DSP, FreeRTOS.
- Architecture: shared libraries in shared/lib/ (RGBWCommon, LEDStrip, LEDPWM, AudioInput, Connectivity), apps in apps/ (each with platformio.ini + src/main.cpp)
- Shared libraries: RGBWCommon (color math, HSV/RGBW types, gamma), LEDStrip (NeoPixelBus template driver for SK6812/WS2815B), LEDPWM (MOSFET PWM channels), AudioInput (I2S DMA + ESP-DSP FFT), Connectivity (WiFi/BLE)
- LED protocols: SK6812 RGBW (4-channel, 5V, 800kHz NRZ), WS2815B RGB (3-channel, 12V, 800kHz NRZ), 24V non-addressable via MOSFET PWM
- Audio pipeline: I2S DMA → ring buffer → ESP-DSP FFT → band mapping (bass/mid/high) → beat detection → BPM tracking
- Coding standards: no Arduino String class, no exceptions, pre-allocated buffers, enum class error codes, ESP_LOG macros
- Dashboard: Tauri desktop app (Svelte 5 + TypeScript + Tailwind CSS) in tools/dashboard/
- See docs/ for full specification (led-control.md, power-and-wiring.md, audio-reactive.md, esp32-internals.md, coding-standards.md, pin-reference.md)

REVIEW FOCUS (Gemini as advisory last-10% reviewer — find what Claude Code and Codex missed):
- Memory safety: stack overflow risk in recursive calls, heap fragmentation from runtime allocation, buffer overruns in LED/audio buffers, DMA buffer alignment requirements, SRAM budget on AVR (2KB)
- Concurrency: FreeRTOS task priorities and starvation, ISR safety (no heap alloc, no printf, no mutex), Core 0 (WiFi/BLE) vs Core 1 (LED/audio) task pinning, mutex usage in shared state
- Hardware interaction: GPIO safety (strapping pins GPIO 0/2/12/15, input-only GPIO 34-39, ADC2+WiFi conflict), NRZ timing tolerances (800kHz), power injection requirements, MOSFET gate drive
- Platform portability: C++11 compatibility in shared libraries, #if __cplusplus >= 201703L guards for C++17 features, printf format macros (PRIu32 from inttypes.h), size_t with %zu
- Real-time constraints: LED strip show() blocking duration, vTaskDelayUntil() usage, loop timing for smooth animations, I2S DMA callback latency
- DMA lifecycle: buffer allocation alignment, DMA descriptor chain management, I2S DMA start/stop sequencing, buffer ownership (CPU vs DMA)
- LED behavior consistency: gamma correction application, white channel extraction in RGBW, brightness scaling, color model conversions (HSV→RGBW)
- Backward compatibility: library API stability (RGBWCommon types used across all apps), platformio.ini compatibility across ESP32/ESP8266/AVR

NOTE: All findings are hypotheses. Claude+Codex will validate before action. High confidence + evidence = likely real. Low confidence = likely noise.

Rules:
- Use file:"?" and line:null if unknown.
- Provide confidence in [0,1] for each issue and overall.
- If zero issues, return issues:[] and verdict:"pass".
- Do not ask to write files. Return the full response inline in terminal output.
```

## Inline Prompt Variant (default)

Use this prompt style for inline mode:

```
You are a senior code reviewer. Advisory-only; do not modify files.
Return prioritized findings inline with severity P0/P1/P2.
For each finding include: file path, line number, impact, confidence, and concrete fix.
End with a short "agreement target" checklist.
Do not ask to create/write files.
```

## After Invocation

### For `--json-file` mode

1. Read `/tmp/gemini-review-<INVOC_ID>.json` with the Read tool (using the literal value from preflight)
2. Validate JSON and process findings
3. If the file is empty or Gemini exited non-zero, read `/tmp/gemini-review-<INVOC_ID>.err` and report the error
4. **Post-use cleanup** — remove temp files to avoid leaking context:
   ```bash
   rm -f /tmp/gemini-review-prompt-$INVOC_ID.md /tmp/gemini-review-$INVOC_ID.json /tmp/gemini-review-$INVOC_ID.err
   ```

### For inline mode

1. Read stdout directly from the command result.
2. If output is missing or command fails, inspect stderr and report the error.

## Error Handling

| Scenario | Action |
| --- | --- |
| `--approval-mode plan` unavailable | Stop, ask user to enable `experimental.plan`, do not auto-fallback to `--yolo` |
| Non-zero exit | Read `/tmp/gemini-review-<INVOC_ID>.err`, report error, stop |
| Empty output | Report "Gemini produced no output", show stderr |
| Invalid JSON | Report parse failure and show raw output |
| Stalled (no progress >20 min) | Check with `TaskOutput` (block: false). If still running, report to user, suggest smaller scope |

## Post-Gemini Triage (Claude+Codex)

1. Validate JSON and summarize counts, surface critical/high issues first.
2. **ALL Gemini findings are hypotheses.** Claude+Codex must validate each before action:
   - High confidence (>0.8) + code evidence = likely real, investigate
   - Medium confidence (0.5-0.8) = check against repo context, may be noise
   - Low confidence (<0.5) = likely noise, dismiss unless user requests investigation
3. Cross-check against repo conventions and current diffs.
4. Decide per issue: fix now, defer with rationale, or dismiss with documented reason.
5. If any issue lacks file/line, locate via Grep before action.
6. **Memory/hardware findings:** Codex validates first (Codex is primary for embedded safety). If Codex confirms, escalate to user. If Codex disagrees, present both perspectives to user.

## Calibration (Important)

- Gemini's strength is catching the last 10% of edge cases. Its weakness is a higher false-positive rate.
- Verify Gemini's complex analysis claims (concurrency, DMA lifecycle, ISR safety) empirically before acting.
- Do NOT act on Gemini findings without Claude+Codex validation.
