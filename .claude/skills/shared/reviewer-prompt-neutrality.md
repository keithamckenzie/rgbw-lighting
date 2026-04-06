# Reviewer Prompt Neutrality Contract

Canonical prompt contract for disputed-review consultations and re-review rounds. Use this contract exactly when Claude Code asks Codex, Gemini, or another reviewer to re-evaluate a finding. Do not inline or restate these rules elsewhere.

## MUST

- Present the finding text exactly as the reviewer wrote it.
- Present counter-evidence as factual observations ("the code at file:line shows X").
- Present supporting evidence as factual observations when it exists.
- Ask the reviewer to re-evaluate given the evidence.
- Stop there. No additional framing.

## MUST NOT

- Argue for or against dismissal ("this appears to be a false positive because...").
- Frame evidence as supporting a particular conclusion ("this contradicts the finding...").
- Add severity qualifiers ("this is a minor issue that...").
- Reference scope, pre-existence, or proportionality.
- Ask leading questions ("would you accept a PASS if...", "given this is pre-existing, do you still...").
- Provide a recommended disposition ("I believe this should be dismissed because...").
- Coach the reviewer toward any particular outcome.

## Prohibited Patterns

- Severity-downgrading language such as "minor" or "cosmetic".
- Scope arguments such as "pre-existing", "outside diff", "outside this task", or "not in the plan".
- Deferral framing such as "can we defer?", "follow-up", "optional", or "accept a PASS if?".
- Approval coaching such as "would you agree this is a false positive?".

## Canonical Template

```text
Finding [ID] from your review:
"[exact original finding text]"

Counter-evidence gathered from the codebase:
[list of {citation, detail} from contradicting_evidence — factual only]

Supporting evidence (confirming the finding):
[list of {citation, detail} from supporting_evidence — factual only]

Please re-evaluate this finding given the above evidence. Is the finding valid or false positive?
```
