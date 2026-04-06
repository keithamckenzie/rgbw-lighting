# Gemini Known Failure Modes

**Policy context:** Scope expansion is expected behavior; under-expanding is the antipattern.

---

## General Failure Modes

### 1. Severity Inflation Beyond Evidence
Rating a finding as critical or high without concrete code evidence demonstrating the impact. Severity must be proportional to demonstrated harm — not hypothetical worst-case scenarios.

### 2. Hallucinated Repository Conventions
Asserting that the repository follows a convention that does not exist. Always verify conventions against CLAUDE.md, docs/coding-standards.md, or gold standard files before citing them in findings.

### 3. Stale Reads in Round 2+
Using cached file contents from a prior review round instead of re-reading the current state. Temp file contamination from prior rounds causes false positives. Always verify findings against the current file state.

### 4. Analysis-JSON Severity Contradiction
When the `analysis` field reasoning says "minor concern" or "low risk" but the derived finding is rated critical or high. The JSON `severity` field is authoritative — but it must be consistent with the reasoning in `analysis`.

### 5. Under-Expanding (Flagging Improvements as Optional)
Marking improvement opportunities as "optional," "nice-to-have," or "follow-up." Per project policy, improvements are findings at their stated severity. They must be fixed before declaring ready — not deferred.

### 6. Diff-Blindness
Ignoring deletions in the diff. Removed validators, removed error handling, removed safety checks, narrowed types, and removed shared-utility usage are as important as what was added.

### 7. Style Over-Rotation
Rating style or formatting preferences as medium or higher severity. Style concerns that do not affect correctness, maintainability, or design-system compliance should be low severity at most.

### 8. Ungrounded Findings on Distant Code
Raising findings on code far from the change without demonstrating a concrete relationship (data-flow, dependency, shared contract, or behavioral coupling). Always include the relationship chain in the finding's `detail` or `evidence` field.

### 9. Missing Validation Steps for Hypothetical Findings
Asserting a problem exists without specifying concrete validation steps to confirm it. Every finding should include `validation_steps` — missing steps flag the finding as incomplete during triage.

### 10. Treating Components in Isolation
Reviewing a component's internal correctness without verifying system-wide data contracts. A component may look correct internally but break when its props, API response shape, or shared state contract changes.

---

## Embedded-Specific Failure Modes

### E1. Phantom Concurrency Issues
Raising concurrency findings without verifying that the code actually runs in a multi-task context. Not all ESP32 code uses FreeRTOS tasks — single-threaded `loop()` code doesn't need mutexes. Check whether tasks are actually created before flagging thread safety.

### E2. Misattributing Arduino Framework Guarantees
Flagging code as unsafe when the Arduino framework already provides the guarantee. For example, `analogWrite()` on ESP32 is thread-safe within the LEDC driver. Verify whether the underlying framework handles the concern before raising it.

### E3. Overstating AVR Memory Concerns for ESP32 Code
Applying AVR memory constraints (2KB SRAM) to ESP32-only code. ESP32 has 520KB SRAM. A 1KB buffer on ESP32 is fine; on AVR it's catastrophic. Check the target platform before raising memory findings.

### E4. Inventing Hardware Timing Requirements
Asserting specific timing requirements (e.g., "NeoPixel data must be sent within 50us") without citing the actual datasheet spec. SK6812 reset time is >=80us. WS2815B reset time is >=280us. Cite the actual spec from docs/led-control.md.

### E5. Misunderstanding Platform Abstraction
Raising findings about missing `#ifdef` guards on code that's already platform-gated by PlatformIO's build system. If a file only builds for `esp32` (configured in platformio.ini `build_src_filter` or `lib_compat_mode`), it doesn't need `#ifdef ESP32` guards. Only shared libraries need platform abstraction.
