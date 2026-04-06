---
name: claude-review
description: Run a Claude advisory review pass over current changes. Use for correctness, risk, and UX findings with file/line citations.
allowed-tools: Read, Grep, Glob, Bash(claude *), Bash(echo *), Bash(INVOC_ID=*), Bash(git status *), Bash(git diff *), Bash(git ls-files *), Bash(rm -f /tmp/claude-review-*)
---

# Claude Advisory Review

Run a focused review with Claude. Do not edit files.

## Arguments

- `/claude-review` — inline findings (default)
- `/claude-review --json-file` — strict JSON output to temp file for machine parsing

## Scope Discovery

Gather scope before invocation:

- `git status -sb`
- `git diff --name-only`
- `git diff --cached --name-only`
- `git ls-files --others --exclude-standard`
- `git diff --shortstat`
- `git diff --cached --shortstat`

If no changes are present, report "No changes to review" and stop unless user explicitly asked for pre-implementation review.

## Temp File Convention

Use `INVOC_ID` (`${PPID}-${RANDOM}`) for invocation-unique naming. This supports concurrent skill invocations within a session. Get the literal value first via `INVOC_ID="${PPID}-${RANDOM}"; echo "INVOC_ID=$INVOC_ID"`, then use that literal in all file paths. The Read tool does not expand shell variables — use the numeric literal in Read paths. Fallback: `mktemp -d` if process model changes.

## Preflight

- Run `claude --version` to confirm the CLI is available.
- If command fails, report the error and stop.

## Invocation

### Inline mode (default)

```bash
claude -p --permission-mode plan --output-format text --max-turns 1 \
  "You are performing a code and product review. Return prioritized findings (P0/P1/P2) with exact file paths and line numbers. For each finding include impact, confidence, and concrete fix. End with a short agreement-target checklist. Return full results inline. Do not ask to create/write files."
```

### JSON-file mode (`--json-file`)

```bash
rm -f /tmp/claude-review-$INVOC_ID.json /tmp/claude-review-$INVOC_ID.err
claude -p --permission-mode plan --output-format json --max-turns 1 \
  "Review the current diff and return STRICT JSON with keys: summary, issues, verdict, confidence, agreement_targets." \
  > /tmp/claude-review-$INVOC_ID.json 2>/tmp/claude-review-$INVOC_ID.err; EC=$?; echo "Exit code: $EC"
```

Use the Bash tool's `run_in_background` parameter set to `true` (do NOT set `timeout`) in `--json-file` mode only.

## After Invocation

### For inline mode

1. Read stdout directly from command result.
2. If output is missing or command fails, inspect stderr and report the error.

### For `--json-file` mode

1. Get literal INVOC_ID value (from the preflight step)
2. Read `/tmp/claude-review-<INVOC_ID>.json`
3. Validate JSON and summarize findings
4. On failure, read `/tmp/claude-review-<INVOC_ID>.err`
5. Cleanup:
   ```bash
   rm -f /tmp/claude-review-$INVOC_ID.json /tmp/claude-review-$INVOC_ID.err
   ```

## Error Handling

| Scenario | Action |
| --- | --- |
| Non-zero exit | Read stderr, report error, stop |
| Empty output | Report "Claude produced no output", include stderr |
| Invalid JSON (`--json-file` mode) | Report parse failure and show raw output |

## Notes

- Use `--permission-mode plan` to keep the pass read-only.
- For iterative rounds, use `claude -p --resume <session-id>`.
