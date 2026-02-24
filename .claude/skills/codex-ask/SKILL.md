---
name: codex-ask
description: Ask Codex a one-off question with web search. Use when user says "ask Codex", "what does Codex think", "consult Codex", or wants a second opinion.
allowed-tools: Read, Bash(codex *), Bash(cat *), Bash(rm -f /tmp/codex-*)
---

# Codex One-Shot Consultation

Ask Codex a quick question without a full review loop. Useful for getting a second opinion, checking an approach, or leveraging Codex's web search.

## Arguments

- `/codex-ask "Your question here"` — ask Codex with web search enabled (default)
- `/codex-ask --no-search "Your question here"` — ask without web search (recommended for sensitive questions)
- `/codex-ask --json "Your question here"` — request strict JSON output (prompt-level; see Structured JSON Output)

## Privacy Guardrail

When `--search` is enabled (the default), do NOT paste secrets, tokens, API keys, or proprietary code excerpts directly into the prompt. Use file references and let Codex read files directly. For sensitive questions, recommend `--no-search`.

## Temp File Convention

All temp files use `$PPID` (parent PID = the Claude Code process) for session-unique naming. `$PPID` is stable across all Bash tool calls within a session and unique across concurrent sessions. If Anthropic changes Claude Code's process model and `$PPID` stops being stable, migrate to `mktemp -d` approach.

## Step 0: Preflight Check

Run once at skill start:

```bash
rm -f /tmp/codex-version-$PPID.log
codex --version > /tmp/codex-version-$PPID.log 2>&1; EC=$?; echo "Exit code: $EC"
```

- Exit code 0: Codex is available, proceed.
- Exit code non-zero or "command not found": Read `/tmp/codex-version-$PPID.log`, report the error, and stop.

## Invocation

**Pre-invocation cleanup** (prevents stale reads from prior runs):
```bash
rm -f /tmp/codex-ask-$PPID.txt /tmp/codex-ask-$PPID.err
```

### With web search (default):
```bash
codex --model gpt-5.3-codex --search exec --sandbox read-only --full-auto \
  -C /Users/keithmckenzie/Projects/rgbw-lighting \
  -o /tmp/codex-ask-$PPID.txt \
  "Question here" 2>/tmp/codex-ask-$PPID.err; EC=$?; echo "Exit code: $EC"
```

### Without web search (`--no-search`):
```bash
codex --model gpt-5.3-codex exec --sandbox read-only --full-auto \
  -C /Users/keithmckenzie/Projects/rgbw-lighting \
  -o /tmp/codex-ask-$PPID.txt \
  "Question here" 2>/tmp/codex-ask-$PPID.err; EC=$?; echo "Exit code: $EC"
```

Use the Bash tool's `run_in_background` parameter set to `true` (do NOT set `timeout`). This returns a task ID immediately, enabling parallel skill invocations (e.g., launching codex-ask and gemini-ask concurrently without waiting).

Before reading the output file, call `TaskOutput` with the task ID (`block: true`, `timeout: 600000`) to wait for completion. If still running after the first poll, poll again.

## Structured JSON Output (Optional)

When `--json` is requested, prepend to the prompt:

```
Return STRICT JSON only.
Schema:
{
  "answer": "concise response",
  "rationale": "brief reasoning",
  "confidence": 0.0,
  "followups": ["optional clarifying questions"]
}
```

If the output is not valid JSON, report a parse failure and show the raw response.

## After Invocation

1. Read `/tmp/codex-ask-$PPID.txt` with the Read tool
2. Present Codex's response to the user
3. If the file is empty or Codex exited non-zero, read `/tmp/codex-ask-$PPID.err` and report the error
4. **Post-use cleanup** — remove temp files to avoid leaking context:
   ```bash
   rm -f /tmp/codex-ask-$PPID.txt /tmp/codex-ask-$PPID.err
   ```

## Error Handling

| Scenario | Action |
|----------|--------|
| Codex exits non-zero | Read `/tmp/codex-ask-$PPID.err`, report to user |
| Output file empty | Report "Codex produced no output", show stderr |
| Stalled (no progress >20 min) | Check with `TaskOutput` (block: false). If still running, report to user, suggest simplifying the question |
| Codex not installed | Report "Codex CLI not found. Install it first." |
| Auth failure | Report "Codex auth issue, run `codex login`" |

## Notes

- `--model gpt-5.3-codex` is required — always include for model targeting
- `--search` is a top-level flag — goes BEFORE subcommand: `codex --model gpt-5.3-codex --search exec` (NOT `codex exec --search`)
- Each invocation is stateless — no conversation context carried
- Codex may start its own MCP servers if configured in `~/.codex/config.toml`
- Stderr is noisy (MCP startup, rollout errors); captured separately in `.err` file

## Codex -> Claude Code Mirror

If Codex is orchestrating but you want Claude Code to answer this one-shot question, use this non-interactive mirror.

### Preflight

```bash
rm -f /tmp/claude-version-$PPID.log
claude --version > /tmp/claude-version-$PPID.log 2>&1; EC=$?; echo "Exit code: $EC"
```

### Invocation (single turn, JSON)

```bash
rm -f /tmp/claude-ask-$PPID.json /tmp/claude-ask-$PPID.err
claude -p --permission-mode plan --output-format json --max-turns 1 \
  "Question here. Return STRICT JSON with keys: answer, rationale, confidence, followups." \
  > /tmp/claude-ask-$PPID.json 2>/tmp/claude-ask-$PPID.err; EC=$?; echo "Exit code: $EC"
```

### Notes

- Claude Code non-interactive entrypoint is `claude -p`.
- Claude Code has no top-level `--search` toggle like Codex; control search/tooling via prompt constraints or permission/tool settings.
- To continue a prior Claude session, use `claude -p --resume <session-id> "Follow-up question"`.
