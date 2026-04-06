---
name: gemini-vision
description: Use Gemini to interpret a screenshot or mockup and produce implementation guidance. Use when the user provides an image or asks to build UI from a visual.
allowed-tools: Read, Write, Bash(gemini *), Bash(echo *), Bash(INVOC_ID=*), Bash(ls *), Bash(rm -f /tmp/gemini-vision-*)
---

# Gemini Vision (Advisory)

**Shared protocol:** Read `.claude/skills/shared/gemini-execution-protocol.md` for lifecycle steps and canonical fragments (GFRAG-ROLE, GFRAG-INTENT-BLOCK, GFRAG-CONFIDENCE). Read `.claude/skills/shared/gemini-antipatterns.md` for failure modes to avoid.

Convert a mockup or screenshot into structured implementation guidance. Do not edit files.

## Arguments

- `/gemini-vision <image>` — return implementation guidance inline in terminal output (default)
- `/gemini-vision --json-file <image>` — machine-parseable JSON file flow

## Image Syntax

Reference image files by path inside the prompt file content. The prompt file (not inline `-p`) passes the image path to Gemini:

```
Analyze the UI in ./screenshot.png
```

## Temp File Convention

Per the shared gemini-execution-protocol, use `INVOC_ID` (`${PPID}-${RANDOM}`) for invocation-unique naming. This supports concurrent skill invocations within a session. Get the literal value first via `INVOC_ID="${PPID}-${RANDOM}"; echo "INVOC_ID=$INVOC_ID"`, then use that literal in all file paths. The Read tool does not expand shell variables — use the numeric literal in Read paths. Fallback: `mktemp -d` if process model changes.

## Preflight

- Confirm the image path exists: `ls -la <path/to/image.png>`.
- Run `gemini --version` (or `gemini -h`) to confirm the CLI is available.
- If either command fails, report the error and stop.

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

**Write the prompt content to the prompt file via Write tool or Bash heredoc before invoking.** The prompt file path is `/tmp/gemini-vision-prompt-$INVOC_ID.md`.

### JSON-file mode (`--json-file`)

**Pre-invocation cleanup** (prevents stale reads from prior runs):
```bash
rm -f /tmp/gemini-vision-$INVOC_ID.json /tmp/gemini-vision-$INVOC_ID.err
```

```bash
gemini --approval-mode plan -m gemini-3.1-pro-preview --include-directories /tmp --include-directories $HOME/.claude/plans --output-format json -p "Read the prompt at /tmp/gemini-vision-prompt-$INVOC_ID.md and follow it exactly." > /tmp/gemini-vision-$INVOC_ID.json 2>/tmp/gemini-vision-$INVOC_ID.err; EC=$?; echo "Exit code: $EC"
```

Use the Bash tool's `run_in_background` parameter set to `true` (do NOT set `timeout`) in `json-file` mode. This returns a task ID immediately, enabling parallel skill invocations (e.g., launching gemini-vision and other skills concurrently without waiting).

Before reading the output file, call `TaskOutput` with the task ID (`block: true`, `timeout: 600000`) to wait for completion. If still running after the first poll, poll again.

### Inline mode (default)

Run in foreground and return stdout directly to the user:
```bash
gemini --approval-mode plan -m gemini-3.1-pro-preview --include-directories /tmp --include-directories $HOME/.claude/plans --output-format text -p "Read the prompt at /tmp/gemini-vision-prompt-$INVOC_ID.md and follow it exactly."
```

Do not redirect stdout in inline mode.

## No-Search Mode (If Supported)

If the Gemini CLI supports disabling search (check `gemini -h`), add the appropriate flag (e.g., `--no-search`) to the invocation. If no such flag exists, add a boundary in the prompt: "Do not use web search."

## Prompt Template (strict)

```
You are converting a UI image into implementation guidance.
This project has a Tauri desktop dashboard for RGBW LED lighting configuration.
Frontend: Svelte 5 (runes syntax) + TypeScript + Tailwind CSS.
Design system: zinc color palette, consistent spacing via Tailwind utility classes.
Key UI areas: device connection, color picker (RGBW/HSV), LED strip configuration, PWM channel control, preset management, serial monitor.
Dashboard location: tools/dashboard/ (Svelte components in src/lib/components/, stores in src/lib/stores/, Tauri backend in src-tauri/src/).
Focus on Svelte component structure and design-system mapping, not pixel-perfect rendering.
Return STRICT JSON only.

OUTPUT JSON SCHEMA (schema-first):
{
  "summary": "one-paragraph visual summary",
  "layout": "structure and hierarchy",
  "components": ["component list — Svelte components mapped to dashboard structure"],
  "tokens": {
    "colors": ["zinc-* Tailwind classes or RGBW hex values where relevant"],
    "typography": ["font families, sizes, weights — use Tailwind classes"],
    "spacing": ["key spacing values using Tailwind spacing scale"]
  },
  "interactions": ["motion/hover/focus behaviors, Tauri command interactions"],
  "accessibility": ["a11y considerations"],
  "implementation_steps": ["ordered, concise steps"],
  "questions": ["missing info to clarify"],
  "confidence": 0.0
}

INTENT SUMMARY:
- <short summary of what to build and why>

BOUNDARIES:
- <what to ignore or exclude>

UNCERTAINTIES:
- <explicit doubts or areas to double-check>

IMAGE (explicit):
- Path: <path/to/image.png>
- Viewport assumptions: <desktop/mobile if known>

CONSTRAINTS:
- Follow zinc color palette and existing Tailwind design patterns.
- Use Svelte 5 runes ($state, $derived, $effect) — no legacy reactive syntax.
- Dashboard communicates with device via Tauri invoke commands.
- Advisory only; do not modify files.

Rules:
- Prefer Tailwind utility classes and zinc-* palette tokens over raw values.
- Map visual elements to existing dashboard components when possible.
- Ask questions when the image is ambiguous.
- Do not ask to write files. Return the full response inline in terminal output.
```

## Inline Prompt Variant (default)

Use this prompt style for inline mode:

```
You are converting a UI image into implementation guidance.
Return concise, actionable guidance inline in this terminal response.
Include: summary, mapped components, implementation steps, and open questions.
Do not ask to create/write files.
```

## After Invocation

### For `--json-file` mode

1. Read `/tmp/gemini-vision-<INVOC_ID>.json` with the Read tool (using the literal value from preflight)
2. Validate JSON and extract implementation guidance
3. If the file is empty or Gemini exited non-zero, read `/tmp/gemini-vision-<INVOC_ID>.err` and report the error
4. **Post-use cleanup** — remove temp files to avoid leaking context:
   ```bash
   rm -f /tmp/gemini-vision-prompt-$INVOC_ID.md /tmp/gemini-vision-$INVOC_ID.json /tmp/gemini-vision-$INVOC_ID.err
   ```

### For inline mode

1. Read stdout directly from the command result.
2. If output is missing or command fails, inspect stderr and report the error.

## Error Handling

| Scenario | Action |
| --- | --- |
| `--approval-mode plan` unavailable | Stop, ask user to enable `experimental.plan`, do not auto-fallback to `--yolo` |
| Non-zero exit | Read `/tmp/gemini-vision-<INVOC_ID>.err`, report error, stop |
| Empty output | Report "Gemini produced no output", show stderr |
| Invalid JSON | Report parse failure and show raw output |
| Stalled (no progress >20 min) | Check with `TaskOutput` (block: false). If still running, report to user, suggest smaller image or narrower scope |

## Post-Gemini Triage (Claude+Codex)

1. Validate JSON and extract implementation steps.
2. Map components to existing dashboard patterns:
   - Svelte components: `tools/dashboard/src/lib/components/`
   - Stores: `tools/dashboard/src/lib/stores/`
   - Tauri commands: `tools/dashboard/src-tauri/src/`
3. Confirm tokens against zinc palette and existing Tailwind patterns.
4. Turn open questions into a short clarification list.

## Calibration (Important)

- Confirm Gemini can read the provided image path with a small smoke test before relying on output.
