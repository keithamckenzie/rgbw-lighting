---
name: claude-plan
description: Ask Claude to review or refine an implementation plan before coding.
allowed-tools: Read, Write, Bash(claude *), Bash(cat *), Bash(rm -f /tmp/claude-plan-*)
---

# Claude Plan Review

Use Claude as a planning reviewer before implementation.

## Arguments

- `/claude-plan` — inline plan review (default)
- `/claude-plan --json-file` — strict JSON output to temp file for machine parsing
- `/claude-plan --context-file /tmp/path.md` — provide a plan context file path

## Preflight

- Run `claude --version` to confirm the CLI is available.
- If command fails, report the error and stop.

## Invocation

### Inline mode (default)

```bash
claude -p --permission-mode plan --output-format text --max-turns 1 \
  "You are reviewing an implementation plan. Return concise inline feedback: strengths, risks, missing steps, and recommended revisions. Do not ask to create/write files."
```

When using a context file, reference it in the prompt:

```bash
claude -p --permission-mode plan --output-format text --max-turns 1 \
  "Review the implementation plan in /tmp/plan-context-$PPID.md. Return full findings inline with prioritized revisions."
```

### JSON-file mode (`--json-file`)

```bash
rm -f /tmp/claude-plan-$PPID.json /tmp/claude-plan-$PPID.err
claude -p --permission-mode plan --output-format json --max-turns 1 \
  "Review the implementation plan and return STRICT JSON with: assessment, key_improvements, open_questions, alternative_approaches." \
  > /tmp/claude-plan-$PPID.json 2>/tmp/claude-plan-$PPID.err; EC=$?; echo "Exit code: $EC"
```

Use the Bash tool's `run_in_background` parameter set to `true` (do NOT set `timeout`) in `--json-file` mode only.

## After Invocation

### For inline mode

1. Read stdout directly from command result.
2. If output is missing or command fails, inspect stderr and report the error.

### For `--json-file` mode

1. Get literal PPID value: `echo $PPID`
2. Read `/tmp/claude-plan-<PPID>.json`
3. Validate JSON and summarize recommendations
4. On failure, read `/tmp/claude-plan-<PPID>.err`
5. Cleanup:
   ```bash
   rm -f /tmp/claude-plan-$PPID.json /tmp/claude-plan-$PPID.err
   ```

## Notes

- `--permission-mode plan` keeps this review-oriented and non-editing.
- For iterative planning rounds, use `claude -p --resume <session-id>`.
