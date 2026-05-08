# Changelog

All notable changes to this project are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [1.0.0] — 2026-05-08

Initial public release. Bench-validated on a real Teknic ClearCore as
part of the [KegWasher](https://github.com/RobotBuzzard/KegWasher)
project bring-up.

### Added

- `ClearCoreWatchdog.h` / `ClearCoreWatchdog.cpp` — direct CMSIS register-
  access wrapper around the SAMD51/SAME53 hardware watchdog. Fills the
  gap left by Teknic's ClearCore Arduino library, which exposes no WDT
  API (`SysManager` has only `ConnectorByIndex` and `ResetBoard`).
- `Watchdog` global singleton with `enable(WdtPeriod)`, `disable()`,
  `kick()`, `isEnabled()`, `lastResetCause()` methods.
- `WdtPeriod` enum with discrete timeouts the SAMD51 hardware actually
  supports: `WdtPeriod_1s`, `_2s`, `_4s`, `_8s` (default), `_16s`.
- Three example sketches:
  - `Basic` — minimal arm + kick + reset-cause readout.
  - `HangDetect` — deliberate `while (true)` hang in `setup()`,
    demonstrates automatic reset within timeout.
  - `BracketLongOperation` — `disable()`/`enable()` bracket pattern
    around a 12 s blocking sequence inside a 4 s watchdog.
- Library Manager metadata (`library.properties`), PlatformIO metadata
  (`library.json`), and IDE syntax highlighting (`keywords.txt`).
- Comprehensive `README.md` with hardware reference (clock source,
  reset controller name, period table) and a troubleshooting matrix.

### Validation

- `arduino-lint --library-manager submit --compliance permissive`
  passes with zero errors and zero warnings on the library and all
  three examples.
- All three examples compile clean for `ClearCore:sam:clearcore`
  (~26-27 % flash, 9.2 KB globals).
- Hang-test on the bench produced 3 reset cycles in 60 seconds with
  USB device numbers `057 → 058 → 059 → 060` — confirms automatic
  reset within the configured timeout.
- Bracketed long-operation test (12 s blocking sequence inside a 4 s
  watchdog) completed without a spurious reset.

[1.0.0]: https://github.com/RobotBuzzard/ClearCoreWatchdog/releases/tag/v1.0.0
