# Gemini Finding Object Schema

Shared definition for Gemini finding fields. Referenced by `gemini-ui/SKILL.md` and `gemini-review/SKILL.md`. This file focuses on per-finding fields first, then documents the top-level response fields each skill adds.

## Top-Level Response Fields

These fields live on the review response object, not on individual findings.

### Shared across Gemini review skills

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `verdict` | `"pass" \| "needs_work"` | Yes | Overall review result |
| `summary` | object | Yes | Severity counts object: `{critical, high, medium, low}` |
| `issues` | array | Yes | Array of finding objects defined below |
| `overall_confidence` | number [0,1] | No | Aggregate confidence for the review result |
| `analysis` | string | No | Reviewer reasoning/context used to derive findings |
| `dry_check` | `"pass" \| "needs_work"` | No | DRY/shared-first assessment for the reviewed scope |
| `shared_candidates` | array of strings | No | Reuse targets in shared/lib/ when DRY issues are reported |
| `notes` | string | No | Optional extra context |

### Skill-specific top-level extensions

| Skill | Additional fields |
|-------|-------------------|
| `gemini-review` | None beyond the shared top-level fields above |
| `gemini-ui` | `quick_wins` (top-level); `area` on each finding |

## Per-Finding Fields

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `id` | string | Yes | Finding identifier (e.g., "GR-1", "GUI-1") |
| `kind` | "issue" \| "improvement" | No (default: "issue") | Finding type — "improvement" for material improvement opportunities |
| `severity` | "critical" \| "high" \| "medium" \| "low" | Yes | Impact severity |
| `title` | string | Yes | Short issue title |
| `detail` | string | Yes | Why this is an issue |
| `evidence` | string | No | file:line — short code snippet or quote |
| `suggestion` | string | No | How to fix |
| `affected_areas` | array of objects | No | Multi-file locator. Each entry: `{file: string, line?: number, symbol?: string}` |
| `validation_steps` | array of strings | No (strongly requested) | Concrete steps to verify finding is real. Missing = flagged incomplete during triage, not discarded |
| `confidence` | number [0,1] | No | Confidence score. See calibration below |

## Confidence Calibration (Embedded-Specific)

| Range | Meaning | Examples |
|-------|---------|---------|
| 0.9+ | Logic error, crash, hardware safety | ISR calling malloc, DMA buffer misalignment, strapping pin misuse, buffer overrun |
| 0.7-0.9 | Strong heuristic with evidence | Missing mutex on shared state, ADC2+WiFi conflict, C++17 in shared lib without guard |
| 0.5-0.7 | Probable, needs more context | Timing concern without profiling data, potential power budget issue |
| <0.5 | Speculative | Style preference, theoretical performance concern |

## Backward Compatibility

Old outputs without `affected_areas`, `validation_steps`, or `kind` fields parse fine — all new fields have null/default fallbacks. The `file` and `line` top-level fields from old format are still accepted as primary locators when `affected_areas` is absent.

## Consistency Rules

- If `analysis` field reasoning says "minor concern," the derived finding must not be critical/high
- `kind` defaults to "issue" if omitted
- `affected_areas` entries should omit `line` when uncertain — never fabricate line numbers
