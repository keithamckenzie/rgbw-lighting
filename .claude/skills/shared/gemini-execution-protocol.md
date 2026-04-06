# Gemini Execution Protocol

Shared lifecycle and canonical prompt fragments for all Gemini skills (`gemini-review`, `gemini-ui`, `gemini-ask`, `gemini-vision`). Each skill references this document for common ceremony and prompt text. Skill-specific behavior lives in the skill's own `SKILL.md`.

**Read this document first** when executing any Gemini skill that references it.

**Namespace isolation rule:** Gemini fragments use the `GFRAG-*` namespace only. Do not invent or reuse `FRAG-*` IDs here; those belong to the Codex protocol.

---

## Part 1: Lifecycle Steps

Each skill follows these steps in order. Steps marked "Template" require parameter substitution from the skill's overlay table. Steps marked "Verbatim" are used as-is. Steps marked "Slot" are filled entirely by the skill.

### Consolidated Execution Model

**Goal:** Minimize tool approval steps by combining sequential Bash and Write calls into fewer batched operations.

**Two-call model:**

**Call 1 — Consolidated Preflight** (combines Steps 1-3):

```bash
INVOC_ID="${PPID}-${RANDOM}"
# Step 3: skill-specific precheck
gemini --version > /dev/null 2>&1 || { echo "ERROR: gemini CLI not available"; exit 1; }
# Skill-specific extra prechecks (e.g., gemini-vision checks image path)
{SKILL_SPECIFIC_PRECHECKS}
echo "READY|INVOC_ID=$INVOC_ID"
```

**Call 2 — Consolidated Invocation** (combines Steps 4-6, with prompt files written via heredoc):

```bash
INVOC_ID="{literal_invoc_id}"
set -euo pipefail
# Write prompt file via heredoc (Step 4)
cat > /tmp/gemini-{skill}-prompt-$INVOC_ID.md << '__GEMINI_PROMPT_{literal_invoc_id}__'
{prompt content}
__GEMINI_PROMPT_{literal_invoc_id}__
# Pre-invocation cleanup (Step 5)
rm -f /tmp/gemini-{skill}-$INVOC_ID.{OUTPUT_EXT} /tmp/gemini-{skill}-$INVOC_ID.err
# Invoke with retry loop (Step 6) — disable -e for retry handling
set +e
MAX_RETRIES=2; RETRY_DELAY=15; for attempt in $(seq 0 $MAX_RETRIES); do
  gemini -m gemini-3.1-pro-preview \
    --include-directories /tmp \
    --include-directories $HOME/.claude/plans \
    {OUTPUT_SCHEMA_NOTE} \
    -p "Read the prompt at /tmp/gemini-{skill}-prompt-$INVOC_ID.md and follow it exactly." \
    > /tmp/gemini-{skill}-$INVOC_ID.{OUTPUT_EXT} 2>/tmp/gemini-{skill}-$INVOC_ID.err; EC=$?
  if [ $EC -eq 0 ] && [ -s /tmp/gemini-{skill}-$INVOC_ID.{OUTPUT_EXT} ]; then break; fi
  if [ $attempt -lt $MAX_RETRIES ] && python3 -c "import re,sys; sys.exit(0 if re.search(r'capacity|rate.limit|\\b429\\b|quota|overloaded|unavailable|\\b503\\b',open('/tmp/gemini-{skill}-$INVOC_ID.err').read(),re.I) else 1)" 2>/dev/null; then
    echo "Attempt $((attempt+1))/$((MAX_RETRIES+1)) failed (capacity), retrying in ${RETRY_DELAY}s..."
    sleep $RETRY_DELAY; RETRY_DELAY=$((RETRY_DELAY * 2))
  else break; fi
done; set -e; echo "Exit code: $EC (attempt $((attempt+1))/$((MAX_RETRIES+1)))"; exit "$EC"
```

Run Call 2 with `run_in_background: true` per Step 7.

**Placeholder rule:** `{literal_invoc_id}` is a documentation placeholder Claude Code substitutes before running Bash. Within prompt/context content examples, `$INVOC_ID` means "insert the literal invocation ID while assembling the file content."

**Heredoc for prompt files:** Use single-quoted, invocation-specific delimiters to prevent shell expansion. If the generated delimiter appears in the content, use the Write tool instead.

---

### Step 1: Invocation ID (Verbatim)

Generate a unique ID for each Gemini invocation:

```bash
INVOC_ID=$(echo "${PPID}-${RANDOM}"); echo "INVOC_ID=$INVOC_ID"
```

**File naming conventions:**
- Prompt: `/tmp/gemini-{skill}-prompt-$INVOC_ID.md`
- Output: `/tmp/gemini-{skill}-$INVOC_ID.json` (or `.txt`)
- Error: `/tmp/gemini-{skill}-$INVOC_ID.err`

### Step 2: Privacy Guardrail (Verbatim)

Gemini search is controlled at the prompt level, not by CLI flag. When a skill or caller requests `--no-search`, include `Do not use web search.` in the prompt. Do not rely on an invented CLI flag.

When search is allowed, do not paste secrets, tokens, API keys, or proprietary code excerpts directly into the prompt. Use file references instead.

### Step 3: Preflight (Slot)

Each skill defines its own prechecks here. Shared baseline:

```bash
gemini --version
```

If the command fails, stop and report the error.

### Step 4: Prompt File Creation (Verbatim)

Write the prompt to `/tmp/gemini-{skill}-prompt-$INVOC_ID.md`. All Gemini prompts go through files. There is no inline `-p` exception.

The invocation references the prompt file with:
```text
Read the prompt at /tmp/gemini-{skill}-prompt-$INVOC_ID.md and follow it exactly.
```

### Step 5: Pre-invocation Cleanup (Template)

Remove stale output and error files before each invocation:
```bash
rm -f /tmp/gemini-{skill}-$INVOC_ID.{OUTPUT_EXT} /tmp/gemini-{skill}-$INVOC_ID.err
```

### Step 6: Invocation (Template)

Use the canonical Gemini retry loop. Retry at most 3 total attempts (initial + 2 retries) with exponential backoff (15s, 30s), and only on capacity/rate-limit errors detected in stderr.

```bash
MAX_RETRIES=2; RETRY_DELAY=15; for attempt in $(seq 0 $MAX_RETRIES); do
  gemini -m gemini-3.1-pro-preview \
    --include-directories /tmp \
    --include-directories $HOME/.claude/plans \
    {OUTPUT_SCHEMA_NOTE} \
    -p "Read the prompt at /tmp/gemini-{skill}-prompt-$INVOC_ID.md and follow it exactly." \
    > /tmp/gemini-{skill}-$INVOC_ID.{OUTPUT_EXT} 2>/tmp/gemini-{skill}-$INVOC_ID.err; EC=$?
  if [ $EC -eq 0 ] && [ -s /tmp/gemini-{skill}-$INVOC_ID.{OUTPUT_EXT} ]; then break; fi
  if [ $attempt -lt $MAX_RETRIES ] && python3 -c "import re,sys; sys.exit(0 if re.search(r'capacity|rate.limit|\\b429\\b|quota|overloaded|unavailable|\\b503\\b',open('/tmp/gemini-{skill}-$INVOC_ID.err').read(),re.I) else 1)" 2>/dev/null; then
    echo "Attempt $((attempt+1))/$((MAX_RETRIES+1)) failed (capacity), retrying in ${RETRY_DELAY}s..."
    sleep $RETRY_DELAY; RETRY_DELAY=$((RETRY_DELAY * 2))
  else break; fi
done; echo "Exit code: $EC (attempt $((attempt+1))/$((MAX_RETRIES+1)))"; exit "$EC"
```

**Invariant:** All Gemini invocations use `gemini -m gemini-3.1-pro-preview` and include both `--include-directories /tmp` and `--include-directories $HOME/.claude/plans`.

### Step 7: Background Execution (Verbatim)

Use the Bash tool's `run_in_background` parameter set to `true` and do not set `timeout`. Call `TaskOutput` with the task ID (`block: true`, `timeout: 600000`) to wait for completion.

Use `TaskOutput(block: false)` only for user-requested mid-run status checks. Do not poll output files directly.

### Step 8: Output Reading & Validation (Template)

After the task completes:

1. **Failure gate:** Check exit code and whether output file exists and is non-empty. If failed, read stderr, report error, stop.
2. Read the output file.
3. If content is wrapped in markdown code fences, extract the content between the fences before parsing.
4. Validate the output against the skill's required schema.
5. **Display the full output to the user.** Do NOT summarize, paraphrase, or omit sections.

### Step 9: Error Handling (Verbatim)

| Scenario | Action |
| --- | --- |
| Non-zero exit | Read stderr, report error, stop |
| Empty output | Report `Gemini produced no output`, show stderr |
| Invalid JSON / parse failure | Report parse failure and show raw output after fence extraction |
| Retries exhausted (capacity) | Report `Gemini capacity unavailable after 3 attempts`, show stderr, suggest trying later |
| Stalled (no progress >20 min) | Check with `TaskOutput(block: false)`. Report to user, suggest reducing scope |

### Step 10: Post-use Cleanup (Template)

Base cleanup:
```bash
rm -f /tmp/gemini-{skill}-prompt-$INVOC_ID.md \
      /tmp/gemini-{skill}-$INVOC_ID.{OUTPUT_EXT} \
      /tmp/gemini-{skill}-$INVOC_ID.err
```

---

## Part 2: Canonical Prompt Fragments

Named fenced blocks with stable IDs. They live here only; skills reference them by ID.

### GFRAG-ROLE
```text
Advisory-only; do not modify files. Return STRICT JSON only.
```

### GFRAG-ANTIPATTERNS
```text
KNOWN FAILURE MODES: Read .claude/skills/shared/gemini-antipatterns.md before responding.
```

### GFRAG-FINDING-SCHEMA
```text
FINDING SCHEMA: See .claude/skills/shared/gemini-finding-schema.md for per-finding field definitions.
```

### GFRAG-INTENT-BLOCK
```text
INTENT SUMMARY:
- <short summary of what is needed and why>

BOUNDARIES:
- <what to ignore or exclude>

UNCERTAINTIES:
- <explicit doubts or areas to double-check>
```

### GFRAG-CONFIDENCE
```text
CONFIDENCE CALIBRATION:
- 0.9+ = Logic error, syntax crash, clear security vulnerability, hardware safety issue (ISR, DMA, strapping pins)
- 0.7-0.9 = Strong heuristic violation with code or evidence (memory safety, concurrency, timing)
- 0.5-0.7 = Probable issue that needs more codebase or runtime/hardware context to confirm
- <0.5 = Style preference or speculative concern
```

### GFRAG-JSON-RULES
```text
JSON RULES:
- Use affected_areas when a finding touches one or more files; omit line when uncertain and never fabricate line numbers
- Include validation_steps for each finding when the skill outputs findings
- kind defaults to "issue" if omitted; use "improvement" for material improvement opportunities
- If zero issues, return issues:[] and verdict:"pass"
```

### GFRAG-TRIAGE
```text
POST-GEMINI TRIAGE:
1. Validate the JSON before acting on it.
2. Verify critical facts against repo context, diffs, or supplied assets.
3. Treat all findings as hypotheses until Claude+Codex validate them.
4. Assign VALID_FIX_NOW (default for confirmed-valid) or VALID_USER_DEFERRED (only after explicit user deferral) inline. To dispute findings, route through /review-findings which owns the full triage state machine.
5. Ask the user for missing context only when validation cannot proceed from the available evidence.
```

---

## Part 3: Skill Overlay Table

| Skill | Steps Used | Fragments Used | Skill-specific parameters |
| --- | --- | --- | --- |
| `gemini-review` | Steps 1-10 | All GFRAGs | `{OUTPUT_EXT}=json`; `{OUTPUT_SCHEMA_NOTE}=--output-format json`; optional context file |
| `gemini-ui` | Steps 1-10 | All GFRAGs | `{OUTPUT_EXT}=json`; `{OUTPUT_SCHEMA_NOTE}=--output-format json`; area-tagged findings |
| `gemini-ask` | Steps 1-10 | `GFRAG-ROLE`, `GFRAG-INTENT-BLOCK` | `{OUTPUT_EXT}=json`; `{OUTPUT_SCHEMA_NOTE}=--output-format json`; search default ON |
| `gemini-vision` | Steps 1-10 | `GFRAG-ROLE`, `GFRAG-INTENT-BLOCK`, `GFRAG-CONFIDENCE` | `{OUTPUT_EXT}=json`; preflight includes image path check |
