---
name: review-findings
description: Process review findings through a structured state machine with evidence gathering and disposition assignment. Use when user says "triage findings", "process review", or after codex-review/gemini-review produces findings to resolve.
allowed-tools: Read, Grep, Glob, Bash(echo *), Bash(git diff *), Bash(git status *), Agent, Skill(codex-ask), Skill(codex-fix), Skill(codex-review), Skill(gemini-ask), Skill(gemini-review), Skill(gemini-ui)
---

# Review Findings Loop

Structured state machine for processing review findings from codex-review, gemini-review, or pasted feedback. Each finding goes through evidence gathering and disposition assignment.

## Arguments

- `/review-findings` — process findings from the most recent review in this session
- `/review-findings "paste findings here"` — process pasted findings directly

## Finding State Machine

```
NEW → EVIDENCE_REQUESTED → EVIDENCE_RETURNED → RESOLUTION_PENDING → RESOLVED
```

Each finding progresses through these states. The skill never skips states.

## Dispositions

| Disposition | When | Requirements |
|-------------|------|--------------|
| `VALID_FIX_NOW` | Default for confirmed-valid findings | No additional evidence needed beyond validation |
| `DISMISS_PENDING` | Contradicting evidence suggests the finding may be factually wrong | Concrete counter-evidence + required Step 3a re-review before any dismissal |
| `DISMISS_REVIEWER_CONFIRMED` | Reviewer agreed finding is false (intermediate) | After Step 3a reviewer agreement — NOT a final disposition. Present to user for acknowledgment. Reverts to `VALID_FIX_NOW` on timeout/abort. |
| `FALSE_POSITIVE_DISMISS` | Finding confirmed false after full workflow | Code evidence + reviewer agreement + **user acknowledgment** — the only final dismissal state |
| `VALID_USER_DEFERRED` | User explicitly defers | Only after user says "defer" or equivalent in their reply |
| `VALID_FIX_NOW` + `BLOCKED_BY` | Valid but blocked by HARD no-touch | Report the conflict with alternatives |

**Rules:**
- "Pre-existing," "outside scope," "architecture limitation," "cosmetic," "proportionality" are NEVER valid dismiss/defer reasons
- Claude Code may NOT recommend deferral — present valid findings as "fix required"
- Hardware safety findings (ISR, DMA, strapping pins, ADC2+WiFi) at any severity: always escalate to user

## Step 1: Parse Findings

Extract findings from review output. Normalize each into the shared triage envelope:

```
{
  id: "F1",                              // assigned by review-findings
  original_id: "GR-1" | null,           // from source
  text: "original finding text",
  source: "codex-review" | "gemini-review" | "gemini-ui" | "user-feedback",
  kind: "issue" | "improvement",         // default: "issue"
  severity: "critical" | "high" | "medium" | "low",
  title: "short title",
  detail: "full finding description" | null,
  evidence: "file:line — detail" | null,
  suggestion: "how to fix" | null,
  file: "path/to/file.cpp" | null,
  line: 123 | null,
  symbol: "functionName" | null,
  affected_areas: [{file, line?, symbol?}] | null,
  validation_steps: ["step"] | null,
  confidence: 0.0 | null,               // Gemini only
  affectedFiles: ["path/to/file.cpp"],   // COMPUTED
  status: "NEW",
  disposition: null,
  blocked_by: null,
  evidenceLog: []
}
```

### `affectedFiles` Computation

- From Codex: `[file]` if file is a real path (not "?"), else `[]`
- From Gemini: deduplicated `file` fields from `affected_areas[]` objects
- Never include bare descriptions or "?" as file paths

### Normalization Paths

**Codex -> Envelope:**
1. Parse plaintext per codex-review Parser Spec
2. ISSUES entries: kind="issue", severity from `[SEVERITY]` tag
3. title = Description text, detail = full finding text including suggestion
4. confidence=null always (Codex doesn't produce it)

**Gemini Review -> Envelope:**
1. Parse JSON from gemini-review output
2. Map `issues[]` entries directly — fields mostly align
3. file = first entry's file from `affected_areas` (backward compat)

**Gemini UI -> Envelope:**
1. Same as Gemini Review
2. Preserve `area` field as metadata
3. `quick_wins` are NOT findings — present as separate list, apply directly without triage

**User Feedback -> Envelope:**
Manual parsing, severity assessment by Claude Code.

Present the finding list with count and severity breakdown.

## Step 2: Gather Evidence (per finding)

For each finding, search the codebase to verify the claim:

**For Gemini findings with `validation_steps`:**
Use those steps as initial evidence-gathering guide. If that guided pass confirms the claim, return `recommended_path: "fix_candidate"`.

**For simple findings (lint-style, obvious code reference):**
Use direct Read/Grep/Glob to verify.

**For complex findings (data flow, concurrency, hardware safety):**
Use Agent tool (`subagent_type: Explore`) or `codex-ask` to gather evidence.

**Evidence worker returns:**
```
{
  claim_under_test: "description of what the finding claims",
  supporting_evidence: [
    { citation: "file:line", detail: "what the code shows" }
  ],
  contradicting_evidence: [
    { citation: "file:line", detail: "why the finding may be wrong" }
  ],
  files_consulted: ["list of files examined"],
  uncertainties: ["things that couldn't be verified"],
  recommended_path: "dismiss_candidate" | "fix_candidate" | "inconclusive"
}
```

**The evidence worker NEVER sets the final disposition.** It only recommends a path.

**`recommended_path` → disposition mapping:**
- `fix_candidate` → `VALID_FIX_NOW`
- `dismiss_candidate` → `DISMISS_PENDING` (proceed to Step 3a — NOT a final disposition)
- `inconclusive` → `VALID_FIX_NOW` (default; insufficient evidence to disprove)

## Step 3: Assign Disposition

**Self-check before triaging:** Read .claude/skills/shared/claude-triage-antipatterns.md for Claude Code's own triage failure modes (T1-T8).

Based on the evidence packet:

**`VALID_FIX_NOW` (default):**
- Evidence worker cannot disprove the finding
- Or evidence is inconclusive (default = valid)
- Auto-assigned — no user interaction needed
- `confidence` scores (Gemini only) inform priority order. Low confidence is NEVER grounds for auto-dismissal

**`DISMISS_PENDING` (required intermediate for disputed findings):**
- Evidence packet has concrete `contradicting_evidence`
- This is the ONLY legal first step toward dismissal
- Mark finding `DISMISS_PENDING` then proceed to Step 3a

**`FALSE_POSITIVE_DISMISS` (requires completed Step 3a + user):**
- NEVER assigned in the initial triage pass
- Only after Step 3a completes with reviewer agreement AND user acknowledgment

**Hardware safety findings (ISR, DMA, strapping pins, ADC2+WiFi, power budget — any severity):**
- Present to user before proceeding past Step 3

## Step 3a: Re-Review Round (for DISMISS_PENDING findings only)

**Purpose:** Send counter-evidence to the original reviewer for independent validation.

**Prompt neutrality requirement:**
Read .claude/skills/shared/reviewer-prompt-neutrality.md and use that contract exactly. Its MUST/MUST NOT lists, prohibited patterns, and canonical template are the only source of truth for this step.

**After re-review:**
- Reviewer says false positive → `DISMISS_REVIEWER_CONFIRMED` (intermediate), present to user for acknowledgment. Only after user acknowledges → `FALSE_POSITIVE_DISMISS`. User declines → `VALID_FIX_NOW`.
- Reviewer maintains finding → `VALID_FIX_NOW`
- Reviewer is ambiguous → `VALID_FIX_NOW`

## Step 3.5: Present Initial Triage (before Step 3a)

```
## Initial Triage

### VALID_FIX_NOW (N findings)
| ID | Severity | Finding | Affected Files |
|----|----------|---------|----------------|

### DISMISS_PENDING (N findings) — will be sent to reviewer for Step 3a
| ID | Severity | Finding | Counter-Evidence |
|----|----------|---------|-----------------|

### VALID_FIX_NOW + BLOCKED_BY (N findings)
| ID | Severity | Finding | Blocked By | Alternatives |
|----|----------|---------|------------|--------------|

### Hardware safety escalations (N findings) — requires user decision
| ID | Severity | Finding | Evidence Summary |
|----|----------|---------|-----------------|
```

## Step 4: Present Final Dispositions (after Step 3a)

No `DISMISS_PENDING` findings should remain — all resolved.

```
## Findings Summary (Final)

### VALID_FIX_NOW (N findings)
| ID | Severity | Finding | Affected Files |
|----|----------|---------|----------------|

### DISMISS_REVIEWER_CONFIRMED (N findings) — awaiting user acknowledgment
| ID | Severity | Finding | Evidence + Reviewer Agreement |
|----|----------|---------|-------------------------------|

### Hardware safety escalations (N findings)
| ID | Severity | Finding | Evidence Summary |
|----|----------|---------|-----------------|
```

## Step 5: Fix Routing

After dispositions are assigned and user has acknowledged any dismissals and escalations:

**Fix hierarchy (structural fix over additive workaround):**
- **Structural elimination** (preferred): rewrite to call canonical source, change types to prevent invalid states, remove duplication by delegation
- **Additive detection**: add test/guard/assertion that catches the problem. Supplements structural fix, never substitutes.
- **Recovery** (least preferred): runtime fallback

**Embedded-specific:** If a finding says "X reimplements Y in shared/lib/", the fix is ALWAYS "make X call Y" — never "add a test that X matches Y."

| Condition | Action |
|-----------|--------|
| <= 3 trivial VALID_FIX_NOW fixes | Claude Code fixes directly |
| > 3 fixes or complex fixes | Route to `codex-fix` with all findings in the brief |
| Mix of trivial and complex | Claude Code fixes trivial, routes complex to `codex-fix` |

## Step 6: Verify Convergence

After fixes, re-run the original review to verify findings are resolved.

New findings enter as NEW (separate from original set). Max 2 review-fix cycles before reporting to user.

## Human Checkpoints

1. **DISMISS_REVIEWER_CONFIRMED:** Present evidence + reviewer agreement, wait for user acknowledgment
2. **Hardware safety findings:** Always show to user before fix routing
3. **VALID_USER_DEFERRED:** Only after user explicitly says "defer"
4. **Ambiguous evidence with non-trivial fix**

## Error Handling

| Scenario | Action |
|----------|--------|
| Review output unparseable | Ask user to paste findings in structured format |
| Evidence worker times out | Default to VALID_FIX_NOW |
| codex-ask fails | Fall back to direct codebase search with Read/Grep |
| Same finding persists after fix | Report to user — may need different approach |
| > 10 findings | Process in batches of 5 |
