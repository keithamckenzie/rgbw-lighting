# Codex Failure Modes

This file is for Codex-facing failure modes only. See .claude/skills/shared/claude-triage-antipatterns.md for Claude Code triage self-checks.

**Policy context:** Scope expansion is expected behavior; under-expanding is the antipattern.

---

## General Failure Modes

### 1. Over-Concretizing Uncertainty
Fabricating file:line references when the exact location is unknown. Use `file:?` when the line is uncertain — never fabricate a line number. A wrong line number is worse than no line number.

### 2. Under-Expanding
Failing to improve nearby code when already modifying files. When touching a file for a primary fix, also extract duplicated logic, simplify complex patterns, align with gold standard conventions, and remove dead code. Reporting improvement opportunities as "optional" or "follow-up" is the failure mode — improvements are findings at their stated severity.

### 3. Under-Reporting UI/Accessibility
Treating UI and accessibility concerns as outside Codex's review domain. Keyboard navigation, focus management, ARIA labels, contrast, and reduced motion are in scope for all reviews that touch dashboard (tools/dashboard) components.

### 4. Fake Confidence Scores in Review Output
Using numeric confidence scores (0.7, 0.85) instead of the severity enum (critical/high/medium/low) in review findings. Codex review output uses severity enums exclusively — numeric confidence is a Gemini-specific review field.

### 5. Flag/Fragment Invention
Creating new `FRAG-*` fragment IDs or inventing new output section names not defined in the execution protocol. New fragments require a protocol update — Codex cannot introduce them unilaterally.

### 6. Self-Authored HARD Constraints
Declaring files or directories as HARD no-touch in briefs or review output. HARD no-touch comes from user directives or CLAUDE.md only. Codex may recommend SOFT no-touch with justification, never HARD.

### 7. Missing VERIFICATION/BSOS Sections
Omitting the VERIFICATION or BETTER_SOLUTION_OUTSIDE_SCOPE sections from output when they apply. VERIFICATION is always required after code changes. BSOS is required whenever files outside SCOPE are modified.

### 8. Partial Structured-Output Compliance
Producing output that includes some required sections but omits others, or drifts from strict heading names (e.g., writing "Issues Found:" instead of "ISSUES:"). All required sections (SUMMARY, ISSUES, VERDICT) must be present with exact heading names. Optional metadata such as `DRY_CHECK` and `SHARED_CANDIDATES` should be included when applicable.

### 9. Validating Functions in Isolation
Reviewing a function's internal correctness without reading its call sites. A function may look correct in isolation but break when its callers pass unexpected shapes, rely on side effects, or depend on ordering guarantees.

### 10. Over-Trusting Existing Patterns
Assuming a pattern is correct just because it already exists in the repo. Existing code may predate current conventions or may itself be the bug. Evaluate patterns against CLAUDE.md, docs/coding-standards.md, and gold standard files.

### 11. Diff-Blindness (Missing Deletions)
Focusing on added/modified lines while ignoring what was removed. Deleted validators, removed error handling, dropped safety checks, and removed shared-utility usage are as important as additions.

### 12. Stopping at the Local Patch
Fixing the immediate symptom without tracing data continuity across boundaries. A type fix in one file may leave stale assumptions in consumers. Trace the data flow from producer through all consumers before declaring a fix complete.

### 13. Under-Reporting Shared Utility Reuse
Identifying code that duplicates existing shared utilities but not flagging it as a concrete finding with `shared_candidates`. When code in an app reimplements logic that exists in `shared/lib/RGBWCommon`, `shared/lib/LEDStrip`, `shared/lib/LEDPWM`, or `shared/lib/AudioInput`, report the specific shared target. Vague "could be DRYer" without a concrete reuse path is not actionable.

---

## Embedded-Specific Failure Modes

### E1. ISR Safety Dismissal
Downplaying ISR safety findings to low severity. Any code that runs in an ISR context (interrupt handler, timer callback, DMA completion) must NEVER allocate heap memory, call printf/Serial.print, or acquire mutexes. These are always critical or high severity findings, never low.

### E2. DMA Buffer Alignment Ignorance
Ignoring DMA buffer alignment requirements. ESP32 DMA buffers must be 4-byte aligned and allocated from DMA-capable memory. Misaligned DMA buffers cause silent data corruption or hard faults — always high severity.

### E3. FreeRTOS Priority Inversion Blindness
Not checking for priority inversion when reviewing mutex-protected shared state. If a high-priority task (Core 1 LED/audio) blocks on a mutex held by a low-priority task (Core 0 WiFi), the system can deadlock or miss real-time deadlines. Check task priorities when reviewing mutex usage.

### E4. Strapping Pin Misuse
Not flagging GPIO usage on strapping pins (GPIO 0, 2, 12, 15) without explicit acknowledgment. GPIO 12 HIGH at boot prevents normal boot. GPIO 0 LOW at boot enters download mode. These must always be flagged, even if the code "works" in testing.

### E5. ADC2+WiFi Conflict Silence
Not flagging ADC2 pin usage (GPIO 0, 2, 4, 12-15, 25-27) in code that also uses WiFi. ADC2 is unavailable when WiFi is active. Use ADC1 (GPIO 32-39) instead. This is a hard hardware constraint, not a suggestion.

### E6. Arduino String Usage
Not flagging `String` class usage (Arduino String). Causes heap fragmentation on long-running embedded systems. Use `char[]` + `snprintf()` instead. This is a project-wide banned type.

### E7. Runtime Allocation in Loops
Not flagging `new`, `malloc`, or container resize operations inside `loop()`, task functions, or any repeatedly-called code path. All buffers must be pre-allocated in `setup()` or constructors. Runtime allocation in loops causes heap fragmentation and eventual crash.

### E8. Missing C++11 Compatibility Guards
Not checking shared library code for C++17 features without `#if __cplusplus >= 201703L` guards. Shared libraries in `shared/lib/` must compile on all platforms (ESP32 C++17, ESP8266 C++11, AVR C++11). Using `std::optional`, structured bindings, or `if constexpr` without guards breaks ESP8266/AVR builds.

### E9. Printf Format Macro Neglect
Not flagging `%lu` or `%ld` with `uint32_t` / `int32_t` variables. These are non-portable — use `PRIu32` / `PRId32` from `<inttypes.h>`. On ESP32, `uint32_t` is `unsigned int` (matches `%u`), but on other platforms it may be `unsigned long`. The macros are always correct.

### E10. Power Budget Blindness
Not considering power budget implications when reviewing code that drives LEDs at full brightness or enables multiple LED zones simultaneously. SK6812 draws 80mA/LED at full white. A 300-LED strip at full brightness draws 24A. Code that enables all LEDs without software brightness limiting or soft-start can cause brownout.
