---
name: gemini-ui
description: Advisory Gemini UI review for Tauri dashboard interface changes — layout, accessibility, design-system compliance, and Svelte patterns. Use when dashboard UI is touched or user asks for UI critique.
allowed-tools: Read, Write, Grep, Glob, Bash(gemini *), Bash(echo *), Bash(INVOC_ID=*), Bash(git status *), Bash(git diff *), Bash(ls *), Bash(rm -f /tmp/gemini-ui-*)
---

# Gemini UI Review (Advisory)

**Shared protocol:** Read `.claude/skills/shared/gemini-execution-protocol.md` for lifecycle steps and canonical fragments (GFRAG-ROLE, GFRAG-ANTIPATTERNS, GFRAG-FINDING-SCHEMA, GFRAG-INTENT-BLOCK, GFRAG-CONFIDENCE, GFRAG-JSON-RULES, GFRAG-TRIAGE). Read `.claude/skills/shared/gemini-antipatterns.md` and `.claude/skills/shared/gemini-finding-schema.md` for output standards. Route disputed findings through `/review-findings`.

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

**Write the prompt content to the prompt file via Write tool or Bash heredoc before invoking.** The prompt file path is `/tmp/gemini-ui-prompt-$INVOC_ID.md`.

### JSON-file mode (`--json-file`)

**Pre-invocation cleanup** (prevents stale reads from prior runs):
```bash
rm -f /tmp/gemini-ui-$INVOC_ID.json /tmp/gemini-ui-$INVOC_ID.err
```

```bash
gemini --approval-mode plan -m gemini-3.1-pro-preview --include-directories /tmp --include-directories $HOME/.claude/plans --output-format json -p "Read the prompt at /tmp/gemini-ui-prompt-$INVOC_ID.md and follow it exactly." > /tmp/gemini-ui-$INVOC_ID.json 2>/tmp/gemini-ui-$INVOC_ID.err; EC=$?; echo "Exit code: $EC"
```

Use the Bash tool's `run_in_background` parameter set to `true` (do NOT set `timeout`) in `json-file` mode. This returns a task ID immediately, enabling parallel skill invocations (e.g., launching codex-review and gemini-ui concurrently without waiting).

Before reading the output file, call `TaskOutput` with the task ID (`block: true`, `timeout: 600000`) to wait for completion. If still running after the first poll, poll again.

### Inline mode (default)

Run in foreground and return stdout directly to the user:
```bash
gemini --approval-mode plan -m gemini-3.1-pro-preview --include-directories /tmp --include-directories $HOME/.claude/plans --output-format text -p "Read the prompt at /tmp/gemini-ui-prompt-$INVOC_ID.md and follow it exactly."
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

1. Read `/tmp/gemini-ui-<INVOC_ID>.json` with the Read tool (using the literal value from preflight)
2. Validate JSON and process findings
3. If the file is empty or Gemini exited non-zero, read `/tmp/gemini-ui-<INVOC_ID>.err` and report the error
4. **Post-use cleanup** — remove temp files to avoid leaking context:
   ```bash
   rm -f /tmp/gemini-ui-prompt-$INVOC_ID.md /tmp/gemini-ui-$INVOC_ID.json /tmp/gemini-ui-$INVOC_ID.err
   ```

### For inline mode

1. Read stdout directly from the command result.
2. If output is missing or command fails, inspect stderr and report the error.

## Error Handling

| Scenario | Action |
| --- | --- |
| `--approval-mode plan` unavailable | Stop, ask user to enable `experimental.plan`, do not auto-fallback to `--yolo` |
| Non-zero exit | Read `/tmp/gemini-ui-<INVOC_ID>.err`, report error, stop |
| Empty output | Report "Gemini produced no output", show stderr |
| Invalid JSON | Report parse failure and show raw output |
| Stalled (no progress >20 min) | Check with `TaskOutput` (block: false). If still running, report to user, suggest smaller scope |

## Post-Gemini Triage (Claude+Codex)

1. Validate JSON and summarize issues by area.
2. Prioritize accessibility and design-system violations.
3. Verify against existing UI patterns before acting.
4. Convert accepted issues into actionable tasks or fixes.
5. Ask the user for missing visual context when needed.
