---
name: gemini-ask
description: Ask Gemini a quick one-off question with web search (if available). Use for second opinions, clarifications, or external research.
allowed-tools: Read, Write, Bash(gemini *), Bash(echo *), Bash(INVOC_ID=*), Bash(cat *), Bash(rm -f /tmp/gemini-ask-*)
---

# Gemini One-Shot Consultation

**Shared protocol:** Read `.claude/skills/shared/gemini-execution-protocol.md` for lifecycle steps and canonical fragments. Read `.claude/skills/shared/gemini-antipatterns.md` for failure modes to avoid.

Ask Gemini a single question. Return inline by default, or JSON via `--json-file`. Do not edit files.

## Arguments

- `/gemini-ask "Your question here"` — inline response (default)
- `/gemini-ask --json-file "Your question here"` — machine-parseable JSON file flow

## Privacy Guardrail

When web search is enabled, do NOT include secrets, tokens, or proprietary code in the prompt. Use file references instead.

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

**Write the prompt content to the prompt file via Write tool or Bash heredoc before invoking.** The prompt file path is `/tmp/gemini-ask-prompt-$INVOC_ID.md`.

### JSON-file mode (`--json-file`)

**Pre-invocation cleanup** (prevents stale reads from prior runs):
```bash
rm -f /tmp/gemini-ask-$INVOC_ID.json /tmp/gemini-ask-$INVOC_ID.err
```

```bash
gemini --approval-mode plan -m gemini-3.1-pro-preview --include-directories /tmp --include-directories $HOME/.claude/plans --output-format json -p "Read the prompt at /tmp/gemini-ask-prompt-$INVOC_ID.md and follow it exactly." > /tmp/gemini-ask-$INVOC_ID.json 2>/tmp/gemini-ask-$INVOC_ID.err; EC=$?; echo "Exit code: $EC"
```

Use the Bash tool's `run_in_background` parameter set to `true` (do NOT set `timeout`) in `json-file` mode. This returns a task ID immediately, enabling parallel skill invocations (e.g., launching codex-ask and gemini-ask concurrently without waiting).

Before reading the output file, call `TaskOutput` with the task ID (`block: true`, `timeout: 600000`) to wait for completion. If still running after the first poll, poll again.

### Inline mode (default)

Run in foreground and return stdout directly to the user:
```bash
gemini --approval-mode plan -m gemini-3.1-pro-preview --include-directories /tmp --include-directories $HOME/.claude/plans --output-format text -p "Read the prompt at /tmp/gemini-ask-prompt-$INVOC_ID.md and follow it exactly."
```

Do not redirect stdout in inline mode.

## No-Search Mode (If Supported)

If the Gemini CLI supports disabling search (check `gemini -h`), add the appropriate flag (e.g., `--no-search`) to the invocation. If no such flag exists, add a boundary in the prompt: "Do not use web search," and set `sources:[]` in the response.

## Prompt Template (strict)

```
You are answering a one-off question. Use web search if available and cite sources.
Return STRICT JSON only.

OUTPUT JSON SCHEMA (schema-first):
{
  "answer": "concise response",
  "rationale": "brief reasoning",
  "sources": [
    {"title": "Source title", "url": "https://example.com"}
  ],
  "confidence": 0.0,
  "followups": ["optional clarifying questions"]
}

INTENT SUMMARY:
- <short summary of what is needed and why>

BOUNDARIES:
- <what to ignore or exclude>

UNCERTAINTIES:
- <explicit doubts or areas to double-check>

QUESTION:
<user question>

CONTEXT (optional):
<file paths, repo context, constraints>

Rules:
- If web search is unavailable, set sources:[] and say so in rationale.
- Keep answer concise and actionable.
- Do not ask to write files. Return the full answer inline in terminal output.
```

## Inline Prompt Variant (default)

Use this prompt style for inline mode:

```
You are answering a one-off question. Keep it concise and actionable.
Use web search if available and cite sources with URLs.
Return the complete answer inline in this terminal message.
Do not ask to create/write files.
```

## After Invocation

### For `--json-file` mode

1. Read `/tmp/gemini-ask-<INVOC_ID>.json` with the Read tool (using the literal value from preflight)
2. Validate JSON and present the answer
3. If the file is empty or Gemini exited non-zero, read `/tmp/gemini-ask-<INVOC_ID>.err` and report the error
4. **Post-use cleanup** — remove temp files to avoid leaking context:
   ```bash
   rm -f /tmp/gemini-ask-prompt-$INVOC_ID.md /tmp/gemini-ask-$INVOC_ID.json /tmp/gemini-ask-$INVOC_ID.err
   ```

### For inline mode

1. Read stdout directly from the command result.
2. If output is missing or command fails, inspect stderr and report the error.

## Error Handling

| Scenario | Action |
| --- | --- |
| `--approval-mode plan` unavailable | Stop, ask user to enable `experimental.plan`, do not auto-fallback to `--yolo` |
| Non-zero exit | Read `/tmp/gemini-ask-<INVOC_ID>.err`, report error, stop |
| Empty output | Report "Gemini produced no output", show stderr |
| Invalid JSON | Report parse failure and show raw output |
| Stalled (no progress >20 min) | Check with `TaskOutput` (block: false). If still running, report to user, suggest simplifying the question |


## Post-Gemini Triage (Claude+Codex)

1. Validate JSON and summarize the answer.
2. Verify critical facts against repo context or known constraints.
3. Decide whether to act, ask follow-up, or defer.
