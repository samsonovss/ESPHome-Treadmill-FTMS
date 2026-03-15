# Project investigation report

## Scope

This report summarizes a static review of the repository as it exists on 2026-03-15. The goal was to inspect the project, identify likely bugs and improvement opportunities, and document them in one place without changing the runtime behavior of the firmware.

## Repository overview

- Main firmware/configuration: `/home/runner/work/ESPHome-Treadmill-FTMS/ESPHome-Treadmill-FTMS/esphome/config.yaml`
- Incline profile data: `/home/runner/work/ESPHome-Treadmill-FTMS/ESPHome-Treadmill-FTMS/esphome/incline_profiles.h`
- User documentation: `/home/runner/work/ESPHome-Treadmill-FTMS/ESPHome-Treadmill-FTMS/README.md`
- UART reverse-engineering guide: `/home/runner/work/ESPHome-Treadmill-FTMS/ESPHome-Treadmill-FTMS/docs/guides/UART_PARSING.md`

## Validation status

The repository currently does not include an automated test suite, linter configuration, or CI workflows. That increases the risk of regressions in a large configuration file because most logic appears to be validated manually on hardware.

## Key findings

### 1. Hard-coded operational secrets are committed to the main ESPHome config

**Files**
- `/home/runner/work/ESPHome-Treadmill-FTMS/ESPHome-Treadmill-FTMS/esphome/config.yaml:127-148`

**What was found**
- The API encryption key is stored directly in the tracked config.
- The OTA password is stored directly in the tracked config.
- The fallback hotspot password is stored directly in the tracked config.

**Why it matters**
- This makes the published configuration unsafe to reuse as-is.
- It also makes credential rotation harder and increases the chance that users flash a shared password into real hardware.

**Recommendation**
- Move all secrets, including OTA and fallback AP credentials, into `secrets.yaml` or documented substitutions.
- Treat `config.yaml` as a template/sample where possible.

---

### 2. `resume_training` uses `sprintf()` for UART commands

**Files**
- `/home/runner/work/ESPHome-Treadmill-FTMS/ESPHome-Treadmill-FTMS/esphome/config.yaml:1069-1074`

**What was found**
- The code formats UART commands with:
  - `sprintf(cmd, "[SETSPD:%03d]", id(target_speed_goal));`
  - `sprintf(cmd, "[SETINC:%03d]", id(target_incline_goal));`

**Why it matters**
- The current buffer is likely large enough for normal values, but `sprintf()` provides no bounds checking.
- This is an avoidable stability risk in a safety-sensitive control path.

**Recommendation**
- Replace `sprintf()` with `snprintf()`.
- Clamp speed/incline values before formatting so unexpected state cannot generate malformed UART strings.

---

### 3. Warm-up “no pulse” safety behavior does not match its own comments

**Files**
- `/home/runner/work/ESPHome-Treadmill-FTMS/ESPHome-Treadmill-FTMS/esphome/config.yaml:1391-1395`
- `/home/runner/work/ESPHome-Treadmill-FTMS/ESPHome-Treadmill-FTMS/esphome/config.yaml:1420-1513`

**What was found**
- The comments say the firmware should emergency-stop if heart rate is unavailable for more than 60 seconds during warm-up.
- In practice, the `Warm-up: No pulse` branch is only checked inside `if (id(remaining_warm_up_time) <= 0)`.

**Why it matters**
- If the configured warm-up duration is longer than 60 seconds, the current code can continue waiting instead of triggering the documented safety stop.
- This looks like a real logic mismatch between intended behavior and implementation.

**Recommendation**
- Move the `warm_up_time >= 60 && isnan(heart_rate)` check outside the `remaining_warm_up_time <= 0` gate, or clarify the documentation if the current behavior is intentional.

---

### 4. FTMS control point validation is incomplete and inconsistent

**Files**
- `/home/runner/work/ESPHome-Treadmill-FTMS/ESPHome-Treadmill-FTMS/esphome/config.yaml:5579-5629`

**What was found**
- Target speed handling accepts any packet with `x.size() >= 3`.
- Target incline handling rate-limits commands, but target speed handling does not.
- Unknown opcodes still produce a generic response, but input normalization is minimal.

**Why it matters**
- BLE-facing control paths should reject malformed or noisy input as early as possible.
- Inconsistent validation makes the behavior harder to reason about and harder to troubleshoot across client apps.

**Recommendation**
- Validate exact payload lengths per opcode, not only minimum lengths.
- Apply consistent throttling/rate limiting to both speed and incline writes.
- Centralize FTMS input checks so command handling is easier to audit.

---

### 5. The project carries a self-include in `incline_profiles.h`

**Files**
- `/home/runner/work/ESPHome-Treadmill-FTMS/ESPHome-Treadmill-FTMS/esphome/incline_profiles.h:1-2`

**What was found**
- The header begins with:
  - `#pragma once`
  - `#include "incline_profiles.h"`

**Why it matters**
- `#pragma once` prevents an include loop, so this is not an immediate functional bug.
- It is still confusing, unnecessary, and makes the file look accidentally generated or partially refactored.

**Recommendation**
- Remove the self-include.

---

### 6. The firmware logic is concentrated in a single very large YAML file

**Files**
- `/home/runner/work/ESPHome-Treadmill-FTMS/ESPHome-Treadmill-FTMS/esphome/config.yaml`

**What was found**
- The main ESPHome config contains thousands of lines of configuration, inline C++ lambdas, FTMS logic, workout state handling, display control, and hardware integration.

**Why it matters**
- This makes review and regression detection difficult.
- It also increases the chance of subtle state bugs because related logic is spread across many lambdas and intervals.

**Recommendation**
- Split the config into smaller packages/modules by concern, for example:
  - BLE/FTMS
  - treadmill UART/control
  - workout programs
  - Nextion UI
  - sensors/safety features

---

### 7. There is no automated validation path for a safety-relevant project

**Files**
- Repository-wide observation

**What was found**
- No tests
- No lint setup
- No `.github/workflows`

**Why it matters**
- This project controls real hardware, including speed and incline.
- Even lightweight automated checks would help catch syntax errors, broken includes, and unsafe refactors before flashing to a device.

**Recommendation**
- Add a minimal CI workflow that at least performs an ESPHome config compile/validation step.
- If possible, add small host-side tests for helper logic that can be isolated from hardware.

---

### 8. Documentation could better distinguish “example values” from production values

**Files**
- `/home/runner/work/ESPHome-Treadmill-FTMS/ESPHome-Treadmill-FTMS/README.md`
- `/home/runner/work/ESPHome-Treadmill-FTMS/ESPHome-Treadmill-FTMS/docs/guides/UART_PARSING.md`

**What was found**
- The UART guide shows example baud-rate discovery values around `4900-5000`, while the checked-in main config uses `4800`.
- The repo documents many features well, but build/validation steps and sample-vs-user-specific values are not clearly separated.

**Why it matters**
- Readers can mistake machine-specific reverse-engineering values for universal defaults.
- New users may also miss which parts of the config are intended to be edited before flashing.

**Recommendation**
- Add a short “before first flash” section that explicitly lists which settings are examples and must be adapted per treadmill and user environment.

## Prioritized improvement plan

### High priority
1. Move committed secrets to ESPHome secrets/substitutions.
2. Fix the warm-up no-pulse logic mismatch.
3. Replace `sprintf()` in control/UART formatting paths.
4. Add at least one CI validation step for the ESPHome config.

### Medium priority
1. Harden FTMS control point input validation.
2. Split `config.yaml` into smaller modules/packages.
3. Clarify documentation around required user-specific configuration.

### Low priority
1. Remove the self-include from `incline_profiles.h`.
2. Continue documenting workout/program state transitions.

## Suggested next actions

If this report is used as a follow-up work list, the most effective next issue would be:

1. create a safer sample configuration layout (`config.sample.yaml` + `secrets.yaml` guidance),
2. correct the warm-up no-pulse behavior,
3. add a minimal compile-only CI workflow.

## Summary

The project is ambitious and feature-rich, and the documentation shows substantial reverse-engineering and domain work. The main risks are not missing features, but maintainability and safety hardening: credentials in source, a few weak validation paths, a documented safety behavior that appears not to execute as intended, and the absence of automated validation around a very large monolithic ESPHome configuration.
