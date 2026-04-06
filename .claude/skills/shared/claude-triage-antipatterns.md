# Claude Code Triage Failure Modes

Claude Code uses this file for its own self-checks while triaging review findings. This content is not for Codex or Gemini prompts. See .claude/skills/shared/reviewer-prompt-neutrality.md for the canonical reviewer prompt wording.

### T1. Coaching Reviewer Toward Dismissal
Framing counter-evidence as an argument for a particular conclusion rather than presenting it neutrally. Symptoms: "this appears to be a false positive because...", "given this is pre-existing, do you agree...", "would you accept a PASS if we defer...". The re-review prompt must follow the contract in .claude/skills/shared/reviewer-prompt-neutrality.md — present evidence and ask for evaluation, never argue for an outcome.

### T2. Pre-Existing Framing
Using "we didn't introduce this" or "this was already present" as evidence against a finding. Pre-existence is never relevant to validity. The question is "is it correct?" not "did we introduce it?" The structural cause: when Claude Code builds a mental model of "our changes" vs "existing code," findings in "existing code" feel unfair — but the triage protocol doesn't recognize that distinction.

### T3. Scope-Based Deferral
Using plan scope, PR scope, or change scope as a reason to defer valid findings. "This is outside the scope of this task" is prohibited deferral language. Valid findings in reviewed code are in scope by definition. The structural cause: plans have explicit SCOPE sections, creating an attractive boundary to hide behind — but scope constrains what to implement, not what to fix.

### T4. Effort-Qualifying Valid Findings
Adding effort/complexity qualifiers after marking a finding VALID_FIX_NOW: "valid but medium effort," "fix required, though this is a larger refactor," "valid — can be addressed in a follow-up." Once valid, the only next content is the fix approach. Effort qualifiers are deferral in disguise.

### T5. Self-Dismissing Via Triage Table
Filling both "Evidence" and "Disposition=FALSE_POSITIVE_DISMISS" in a single triage pass without running the re-review round. DISMISS_PENDING exists to force a pause between evidence and disposition. **Variant (T5b):** Using DISMISS_PENDING correctly as the label but writing conclusive justification paragraphs in the Reason column that make findings feel resolved, then presenting the whole table as "ready to proceed" without running Step 3a.

### T6. Reviewer Competence Dismissal
Dismissing the reviewer's ability to read the codebase rather than disproving the finding's factual claim. Symptoms: "stale read," "Gemini read wrong diffs," "cache artifact," "reviewer error," "tool inaccuracy." These arguments attack the reviewer's tooling rather than engaging with the finding. Even when the reviewer DID read stale state, the correct response is DISMISS_PENDING with grep evidence → Step 3a re-review — not self-concluded batch dismissal.

### T7. Inline Disposition During codex-review
Assigning dispositions (DISMISS_PENDING, FALSE_POSITIVE_DISMISS, VALID_FIX_NOW) directly within the codex-review loop instead of routing through `/review-findings`. codex-review is a review loop, not a triage tool. All disposition assignment happens through the review-findings state machine.

### T8. Embedded-Specific Severity Downgrade
Downgrading embedded-specific findings (ISR safety, DMA alignment, strapping pins, ADC2+WiFi) because they "only matter at runtime" or "work fine in testing." These are hardware-level constraints that cause intermittent failures under specific conditions. A GPIO 12 strapping pin issue won't show up in testing but will brick the device in production. Default to the reviewer's severity for hardware findings.
