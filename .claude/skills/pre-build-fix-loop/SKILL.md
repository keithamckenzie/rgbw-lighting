---
name: pre-build-fix-loop
description: Run PlatformIO builds and automatically fix failures in a loop. Use when user says "fix build", "build loop", or wants automated build-fix iteration across platforms.
allowed-tools: Read, Write, Edit, Grep, Glob, Bash(pio run *), Bash(pio test *), Bash(echo *), Bash(INVOC_ID=*), Bash(rm -f /tmp/pb-*), Bash(cat /tmp/pb-*), Skill(codex-fix)
---

# Pre-Build Fix Loop

Automated loop: run PlatformIO builds, parse errors, fix them (directly or via Codex), re-run until clean.

## Arguments

- `/pre-build-fix-loop` — build affected environments only (default)
- `/pre-build-fix-loop --all` — build all environments (esp32, esp8266, arduino-uno)
- `/pre-build-fix-loop --env esp32` — build specific environment only
- `/pre-build-fix-loop --with-tests` — include `pio test -e native` in the loop

## Temp File Convention

Use `INVOC_ID` (`${PPID}-${RANDOM}`) for invocation-unique naming. This supports concurrent skill invocations within a session.

**Get the literal value first:**
```bash
INVOC_ID="${PPID}-${RANDOM}"; echo "INVOC_ID=$INVOC_ID"
```
Use that literal value in all subsequent file paths. The Read tool does not expand shell variables — use the numeric literal in Read paths. Fallback: `mktemp -d` if process model changes.

**Cleanup:** Remove `/tmp/pb-$INVOC_ID.log` at the end (success or max iterations).

## Algorithm

### Step 1: Determine Environments

**Default (affected only):** Detect which environments need building based on changed files:
- `shared/lib/` touched → build all environments (shared libraries must compile everywhere)
- `apps/<name>/` touched → build that app's environments only (from its platformio.ini)
- `tools/dashboard/` touched → skip PlatformIO builds, run dashboard checks instead

**`--all`:** Build all environments for all apps with changes.

**`--env <env>`:** Build only the specified environment.

### Step 2: Run Build

For each app directory with changes, run from that app's directory:

```bash
cd /Users/keithmckenzie/Projects/rgbw-lighting/apps/<app-name> && pio run -e <env> > /tmp/pb-$INVOC_ID.log 2>&1; EC=$?; echo "Exit code: $EC"; head -80 /tmp/pb-$INVOC_ID.log; echo "..."; tail -80 /tmp/pb-$INVOC_ID.log
```

If `--with-tests` and tests exist:
```bash
cd /Users/keithmckenzie/Projects/rgbw-lighting/apps/<app-name> && pio test -e native > /tmp/pb-$INVOC_ID.log 2>&1; EC=$?; echo "Exit code: $EC"; head -80 /tmp/pb-$INVOC_ID.log; echo "..."; tail -80 /tmp/pb-$INVOC_ID.log
```

### Step 3: Check Exit Code

- **Exit 0:** Report success for this environment. Move to next environment or stop.
- **Exit non-zero:** Continue to Step 4.

### Step 4: Parse Errors

Read `/tmp/pb-$INVOC_ID.log` and categorize errors:

| Category | Pattern | Priority |
|----------|---------|----------|
| **Missing include** | `fatal error: No such file or directory` | 1 (fix first) |
| **Undeclared identifier** | `was not declared in this scope`, `use of undeclared identifier` | 2 |
| **Type error** | `cannot convert`, `no matching function`, `invalid conversion` | 3 |
| **Linker error** | `undefined reference to`, `multiple definition of` | 4 |
| **Warning-as-error** | `-Werror` triggered warnings | 5 |
| **Test failure** | `FAILED`, `assertion failed` | 6 |

### Step 5: Triage and Fix

Process errors in priority order:

| Category | Error Count | Action |
|----------|-------------|--------|
| **Missing include** | Any | Fix directly — add `#include`, check lib_deps |
| **Undeclared identifier** | < 5 | Fix directly — add declarations, includes, forward declarations |
| **Undeclared identifier** | >= 5 | Route to `codex-fix` with FULL error output |
| **Type error** | < 3 | Fix directly — correct types, add casts, fix signatures |
| **Type error** | >= 3 | Route to `codex-fix` with FULL error output |
| **Linker error** | Any | Fix directly if obvious (missing source file, duplicate symbol). Route to `codex-fix` if unclear. |
| **Warning-as-error** | Any | Fix directly — the warning text explains the issue |
| **Test failure** | Any | Route to `codex-fix` — tests need more context than mechanical fixes |

**When routing to `codex-fix`:** Include the FULL error output in the fix brief. Frame as: "Fix the following build errors" with the raw compiler output.

**Embedded-specific fix patterns:**
- `PRIu32 not defined` → add `#include <inttypes.h>`
- `String` class usage → replace with `char[]` + `snprintf()`
- C++17 feature on ESP8266/AVR → add `#if __cplusplus >= 201703L` guard or use C++11 alternative
- Platform-specific API → add `#ifdef ESP32` / `#ifdef ESP8266` guards

### Step 6: Re-Run

After applying fixes, go back to Step 2.

**Maximum 3 total iterations.** Track iteration count.

**Same-error detection:** If the same errors persist after a fix attempt, stop early — do not burn another iteration on the same failures.

### Step 7: Final Report

- **All passing (exit 0):** Report success with iteration count and environments verified.
- **Max iterations reached:** Report remaining failures:
  - Which environments still fail
  - Error count and category per environment
  - Suggestion: ask user for guidance

## Multi-Environment Workflow

When building multiple environments:
1. Build ESP32 first (most common target, C++17)
2. Fix any ESP32 errors
3. Build ESP8266 (C++11 — may surface compatibility issues)
4. Fix ESP8266-specific errors
5. Build AVR if applicable (C++11, 2KB SRAM — may surface memory issues)
6. Run tests last (if `--with-tests`)

This order catches issues early — ESP32 errors are usually the simplest, ESP8266/AVR reveal portability issues.

## Error Handling

| Scenario | Action |
|----------|--------|
| App directory doesn't exist | Report error, skip to next app |
| platformio.ini missing | Report error, skip |
| `codex-fix` fails | Report Codex failure. Attempt direct fix if error is understandable, otherwise ask user. |
| Same errors persist across iterations | Stop early. Report to user. |
| Environment not in platformio.ini | Report "Environment <env> not found in platformio.ini", stop |
| Build succeeds but with warnings | Report warnings but do not loop (exit 0 = success) |

## Cleanup

Always clean up temp files when done:
```bash
rm -f /tmp/pb-$INVOC_ID.log
```
