---
name: claude-ask
description: Ask Claude a one-off question. Use for second opinions, clarifications, and quick analysis.
allowed-tools: Read, Bash(claude *), Bash(echo *), Bash(INVOC_ID=*), Bash(cat *), Bash(rm -f /tmp/claude-ask-*)
---

# Claude One-Shot Consultation

Ask Claude a quick question without a full review loop.

## Arguments

- `/claude-ask "Your question here"` — inline response (default)
- `/claude-ask --json-file "Your question here"` — strict JSON output to a temp file for machine parsing

## Temp File Convention

Use `INVOC_ID` (`${PPID}-${RANDOM}`) for invocation-unique naming. This supports concurrent skill invocations within a session. Get the literal value first via `INVOC_ID="${PPID}-${RANDOM}"; echo "INVOC_ID=$INVOC_ID"`, then use that literal in all file paths. The Read tool does not expand shell variables — use the numeric literal in Read paths. Fallback: `mktemp -d` if process model changes.

## Preflight

- Run `claude --version` to confirm the CLI is available.
- If the command fails, report the error and stop.

## Invocation

### Inline mode (default)

```bash
claude -p --permission-mode plan --output-format text --max-turns 1 \
  "QUESTION. Return the complete answer inline in this terminal response. Do not ask to create/write files."
```

### JSON-file mode (`--json-file`)

```bash
rm -f /tmp/claude-ask-$INVOC_ID.json /tmp/claude-ask-$INVOC_ID.err
claude -p --permission-mode plan --output-format json --max-turns 1 \
  "QUESTION. Return STRICT JSON with keys: answer, rationale, confidence, followups." \
  > /tmp/claude-ask-$INVOC_ID.json 2>/tmp/claude-ask-$INVOC_ID.err; EC=$?; echo "Exit code: $EC"
```

Use the Bash tool's `run_in_background` parameter set to `true` (do NOT set `timeout`) in `--json-file` mode only.

## After Invocation

### For inline mode

1. Read stdout directly from command result.
2. If output is missing or command fails, inspect stderr and report the error.

### For `--json-file` mode

1. Get literal INVOC_ID value (from the preflight step)
2. Read `/tmp/claude-ask-<INVOC_ID>.json`
3. Validate JSON and present answer
4. On failure, read `/tmp/claude-ask-<INVOC_ID>.err`
5. Cleanup:
   ```bash
   rm -f /tmp/claude-ask-$INVOC_ID.json /tmp/claude-ask-$INVOC_ID.err
   ```

## Notes

- `claude -p` is the non-interactive entrypoint.
- `--permission-mode plan` keeps this review/read-only oriented.
- For follow-ups in the same conversation, use `claude -p --resume <session-id>`.
