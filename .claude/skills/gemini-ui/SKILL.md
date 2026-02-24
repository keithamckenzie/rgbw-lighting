---
name: gemini-ui
description: Advisory Gemini UI review for Tauri dashboard interface changes — layout, accessibility, design-system compliance, and Svelte patterns. Use when dashboard UI is touched or user asks for UI critique.
allowed-tools: Read, Grep, Glob, Bash(gemini *), Bash(git status *), Bash(git diff *), Bash(ls *), Bash(rm -f /tmp/gemini-*)
---

# Gemini UI Review (Advisory)

Run a UI-focused review pass on the Tauri dashboard. Do not edit files.

## Arguments

- `/gemini-ui` — return UI findings inline in terminal output (default)
- `/gemini-ui --json-file` — machine-parseable JSON file flow

## Trigger

- Use when dashboard UI code or styles change (Svelte components, Tailwind classes, CSS).
- Use on explicit user request.
- Skip when no dashboard UI changes are present.

## Inputs to Gather

- Changed UI files and diff stats.
- Optional: screenshot or recording path.
- Target pages/components and expected behavior.

## Temp File Convention

All temp files use `$PPID` (parent PID = the Claude Code process) for session-unique naming. `$PPID` is stable across all Bash tool calls within a session and unique across concurrent sessions. If Anthropic changes Claude Code's process model and `$PPID` stops being stable, migrate to `mktemp -d` approach. The Read tool does not expand `$PPID` — get the literal value via `echo $PPID` first, then use the numeric path in Read.

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
rm -f /tmp/gemini-ui-$PPID.json /tmp/gemini-ui-$PPID.err
```

```bash
gemini --approval-mode plan -m gemini-3.1-pro-preview --include-directories /tmp --output-format json -p "PROMPT" > /tmp/gemini-ui-$PPID.json 2>/tmp/gemini-ui-$PPID.err; EC=$?; echo "Exit code: $EC"
```

Use the Bash tool's `run_in_background` parameter set to `true` (do NOT set `timeout`) in `json-file` mode. This returns a task ID immediately, enabling parallel skill invocations (e.g., launching codex-review and gemini-ui concurrently without waiting).

Before reading the output file, call `TaskOutput` with the task ID (`block: true`, `timeout: 600000`) to wait for completion. If still running after the first poll, poll again.

### Inline mode (default)

Run in foreground and return stdout directly to the user:
```bash
gemini --approval-mode plan -m gemini-3.1-pro-preview --include-directories /tmp --output-format text -p "PROMPT"
```

Do not redirect stdout in inline mode.

## No-Search Mode (If Supported)

If the Gemini CLI supports disabling search (check `gemini -h`), add the appropriate flag (e.g., `--no-search`) to the invocation. If no such flag exists, add a boundary in the prompt: "Do not use web search."

## Prompt Template (strict)

```
You are a UI/UX reviewer. Advisory-only; do not modify files.
This project has a Tauri desktop dashboard app for RGBW lighting configuration.
Frontend: Svelte 5 (runes syntax) + TypeScript + Tailwind CSS.
Design system: zinc color palette throughout. Do NOT use slate- classes.
Key UI areas: device connection, color picker (RGBW/HSV), LED strip configuration, PWM channel control, preset management, serial monitor.
Focus on component structure, Svelte 5 patterns, and design consistency — not pixel-perfect rendering.
Return STRICT JSON only.

OUTPUT JSON SCHEMA (schema-first):
{
  "summary": {"critical":0,"high":0,"medium":0,"low":0},
  "issues": [
    {
      "id": "GUI-1",
      "area": "layout|accessibility|design_system|responsiveness|svelte_patterns",
      "severity": "critical|high|medium|low",
      "file": "path/to/file.svelte",
      "line": 123,
      "detail": "What is wrong",
      "suggestion": "How to improve",
      "confidence": 0.0
    }
  ],
  "quick_wins": ["short actionable fix"],
  "overall_confidence": 0.0,
  "verdict": "pass|needs_work"
}

INTENT SUMMARY:
- <short summary of what changed and why>

BOUNDARIES:
- <what to ignore or exclude>

UNCERTAINTIES:
- <explicit doubts or areas to double-check>

CONTEXT (explicit scope, do not infer):
- Repo UI patterns:
  - Dashboard app: tools/dashboard/
  - Svelte components: tools/dashboard/src/lib/components/
  - Stores: tools/dashboard/src/lib/stores/
  - Tauri commands: tools/dashboard/src-tauri/src/
  - Tests: tools/dashboard/src/test/
- Design system: zinc color palette (zinc-50 through zinc-950), Tailwind CSS utility classes
- Svelte 5: runes ($state, $derived, $effect), no legacy reactive syntax
- Changed files: <list>
- Diff stats: <git diff --shortstat + git diff --cached --shortstat>
- Screenshot path (if provided): <path or none>

CHECKS:
- Layout: hierarchy, spacing, alignment, readability, logical grouping of controls
- Accessibility: contrast (WCAG AA), focus states, keyboard navigation, ARIA labels, screen reader support
- Design system: consistent zinc palette usage (no slate-), proper Tailwind spacing scale, consistent border-radius and shadows
- Responsiveness: window resizing behavior, minimum viable dimensions, overflow handling
- Svelte patterns: proper rune usage ($state/$derived/$effect), component composition, prop types, event handling, no legacy reactive patterns

Rules:
- Use file:"?" and line:null if unknown.
- If no issues, return issues:[] and verdict:"pass".
- Do not ask to write files. Return the full response inline in terminal output.
```

## Inline Prompt Variant (default)

Use this prompt style for inline mode:

```
You are a UI/UX reviewer. Advisory-only; do not modify files.
Return prioritized findings inline with severity P0/P1/P2.
For each finding include: file path, line number, impact, confidence, and concrete fix.
Do not ask to create/write files.
```

## After Invocation

### For `--json-file` mode

1. Get the literal PPID value: `echo $PPID`
2. Read `/tmp/gemini-ui-<PPID>.json` with the Read tool (using the numeric value)
3. Validate JSON and process findings
4. If the file is empty or Gemini exited non-zero, read `/tmp/gemini-ui-<PPID>.err` and report the error
5. **Post-use cleanup** — remove temp files to avoid leaking context:
   ```bash
   rm -f /tmp/gemini-ui-$PPID.json /tmp/gemini-ui-$PPID.err
   ```

### For inline mode

1. Read stdout directly from the command result.
2. If output is missing or command fails, inspect stderr and report the error.

## Error Handling

| Scenario | Action |
| --- | --- |
| `--approval-mode plan` unavailable | Stop, ask user to enable `experimental.plan`, do not auto-fallback to `--yolo` |
| Non-zero exit | Read `/tmp/gemini-ui-<PPID>.err`, report error, stop |
| Empty output | Report "Gemini produced no output", show stderr |
| Invalid JSON | Report parse failure and show raw output |
| Stalled (no progress >20 min) | Check with `TaskOutput` (block: false). If still running, report to user, suggest smaller scope |

## Post-Gemini Triage (Claude+Codex)

1. Validate JSON and summarize issues by area.
2. Prioritize accessibility and design-system violations.
3. Verify against existing UI patterns before acting.
4. Convert accepted issues into actionable tasks or fixes.
5. Ask the user for missing visual context when needed.
