# ClearCoreWatchdog

Hardware watchdog timer for the **[Teknic ClearCore](https://www.teknic.com/products/io-motion-controller-clearcore/)** (Microchip SAME53N19A) — direct CMSIS register access, since Teknic's ClearCore Arduino library does not expose any watchdog API.

Bench-validated on real hardware. Deliberate-hang test (the firmware enters `while(1)` without kicking) confirms the chip resets within the configured timeout and recovers cleanly through the bootloader. Bracketed long operations (`Watchdog.disable()` / `Watchdog.enable()` around a blocking section) do not trigger spurious resets.

---

## Why this exists

If you've gone looking for `Watchdog.enable()` on the ClearCore and come up empty, you're not alone. The ClearCore Arduino API exposes `SysManager::ConnectorByIndex()` and `SysManager::ResetBoard()` and that's it — no watchdog timer abstraction at all (verified against [the published API reference](https://teknic-inc.github.io/ClearCore-library/) at the time of writing).

The SAMD51/SAME53 chip itself has a perfectly capable hardware watchdog. This library wraps it.

---

## What you get

```cpp
#include <ClearCoreWatchdog.h>

void setup() {
  // Print why we last rebooted (POR, EXT, WDT, SYST, …)
  Serial.print("reset cause = 0x");
  Serial.println(Watchdog.lastResetCause(), HEX);

  Watchdog.enable();              // 8-second default
  // Watchdog.enable(WdtPeriod_4s);  // if you want shorter
}

void loop() {
  // ...your work...
  Watchdog.kick();
}
```

That's it. If `kick()` doesn't get called within the period, the chip resets via the bootloader, which (because the application slot is still valid) jumps straight back to the application — net effect of any hang ≥ timeout = a fresh `setup()`.

---

## Installation

### Arduino IDE / Library Manager (once indexed)

`Sketch → Include Library → Manage Libraries…` → search **ClearCoreWatchdog**.

### Manual install (works today, before Library Manager indexing)

```bash
cd ~/Arduino/libraries
git clone https://github.com/RobotBuzzard/ClearCoreWatchdog.git
```

Restart the Arduino IDE if it was open. The examples appear under `File → Examples → ClearCoreWatchdog`.

### `arduino-cli`

```bash
arduino-cli lib install --git-url https://github.com/RobotBuzzard/ClearCoreWatchdog.git
```

(Requires `arduino-cli config set library.enable_unsafe_install true` first if you haven't enabled git installs.)

### Prerequisites

- Teknic ClearCore board package installed (`ClearCore:sam`). See the companion repo [teknic-clearcore-cli](https://github.com/RobotBuzzard/teknic-clearcore-cli) for an idempotent install script and Linux gotchas.

---

## API

```cpp
class ClearCoreWatchdogClass {
  void   enable(WdtPeriod period = WdtPeriod_8s);
  void   disable();
  void   kick();
  bool   isEnabled();
  uint8_t lastResetCause();
};

extern ClearCoreWatchdogClass Watchdog;
```

### `enable(period)`

Configures and starts the WDT. Idempotent — safe to call repeatedly; will tear down and reconfigure cleanly if the watchdog is already running.

Available periods (the discrete values the SAMD51 hardware actually supports):

| Constant         | Timeout |
|------------------|---------|
| `WdtPeriod_1s`   |  1 sec  |
| `WdtPeriod_2s`   |  2 sec  |
| `WdtPeriod_4s`   |  4 sec  |
| `WdtPeriod_8s`   |  8 sec  *(default)* |
| `WdtPeriod_16s`  | 16 sec  |

### `disable()`

Stops the WDT. The typical reason: bracketing a deliberate long-blocking operation (self-test, calibration, factory provisioning) that would legitimately exceed the timeout. Re-enable with `enable()` on exit. See [`examples/BracketLongOperation`](examples/BracketLongOperation/BracketLongOperation.ino).

### `kick()`

Feeds the watchdog. Call at least once per timeout period — typically the last statement of your `loop()` before any `delay()`. If you have multiple long-running paths in `loop()`, prefer calling `kick()` at the *end* so that any earlier hang is caught.

> **ISR-unsafe.** `kick()` does a `SYNCBUSY` wait that can deadlock if invoked from an interrupt handler. Stick to non-ISR contexts. (`enable()` and `disable()` have the same caveat.)

### `isEnabled()`

Returns `true` if the WDT is currently armed. Useful for code that wants to bracket its own long-blocking section without assuming who else may have changed the WDT state.

```cpp
void runFactoryTest() {
  bool wasOn = Watchdog.isEnabled();
  if (wasOn) Watchdog.disable();
  // ...12 seconds of blocking I/O...
  if (wasOn) Watchdog.enable();
}
```

### `lastResetCause()`

Reads `RSTC.RCAUSE` and returns the latched bitmask from the most recent reset. Cached on first call. Useful for "did this thing reboot itself unexpectedly?" forensics.

| Bit  | Constant         | Meaning |
|------|------------------|---------|
| 0x01 | POR              | Power-on reset (cold boot) |
| 0x02 | BODCORE          | Brown-out detector — core voltage |
| 0x04 | BODVDD           | Brown-out detector — VDD |
| 0x08 | NVM              | NVM controller initiated |
| 0x10 | EXT              | External reset pin (also bossac post-upload) |
| **0x20** | **WDT**       | **Watchdog timer fired** ← what you check after a hang |
| 0x40 | SYST             | Cortex-M software reset (`AIRCR.SYSRESETREQ`) |
| 0x80 | BACKUP           | Wakeup from backup domain |

Multiple bits can be set if causes pile up across resets.

---

## Examples

### `Basic`

Minimal — print the reset cause, arm at 8 s, count up forever in `loop()` while kicking. Use this as your "did I install the library correctly?" smoke test.

### `HangDetect`

Deliberately hangs in `setup()` after arming the watchdog. Watch the Serial Monitor: you'll see `"Hanging now — expect reset in ~8s"`, then about 8 seconds later the device reboots and the next boot reports `Reset cause: 0x20` (the WDT bit). This proves the dog actually bites on your hardware.

### `BracketLongOperation`

A 12-second blocking sequence inside a 4-second watchdog. Without the `disable()` / `enable()` bracket the WDT would reset us in step 4; with it, the long op runs to completion. Pattern to copy when you have legitimate long-blocking work.

---

## Hardware reference

- **Chip**: Microchip SAME53N19A (Cortex-M4F, 120 MHz, FPU)
- **WDT clock source**: `OSC32KCTRL.CLK_OSC32K_1K` — 1.024 kHz tap derived internally from the always-on OSCULP32K oscillator
- **No GCLK channel needed** — unlike SAMD21, the SAMD51's WDT does not require a peripheral GCLK channel to be configured. The 1 kHz path is hard-wired. The library only sets `OSC32KCTRL->OSCULP32K.bit.EN1K = 1` defensively (already on at reset).
- **Period table** (in `WDT_CONFIG_PER` cycles at 1024 Hz):
  `CYC1024`/`CYC2048`/`CYC4096`/`CYC8192`/`CYC16384` ≈ 1/2/4/8/16 sec
- **Clear key**: `0xA5` written to `WDT->CLEAR.reg`. Anything else triggers an immediate reset.
- **Reset controller**: SAME53 calls it `RSTC`, **not** `RSTCTRL` like some other SAMD parts. The library uses `RSTC->RCAUSE`.

The full register layout is in `same53n19a.h` (component `wdt.h`, `rstc.h`, `osc32kctrl.h`) inside the ClearCore Arduino board package.

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| `error: 'RSTCTRL' was not declared` | Old datasheet name — SAME53 uses `RSTC` | Use `RSTC->RCAUSE`, or just call `Watchdog.lastResetCause()` |
| Spurious resets during legitimate long blocking | A blocking operation exceeds the timeout | Bracket with `disable()` / `enable()` (see `BracketLongOperation`) |
| Reset every ~8 s with no hang in your code | A library or peripheral driver is blocking longer than expected | Add `Watchdog.kick()` calls inside the offending block, raise the timeout (`WdtPeriod_16s`), or disable around it |
| `kick()` from an ISR causes a hang or fault | `SYNCBUSY` wait deadlocks under interrupt context | Set a flag in the ISR, kick from `loop()` |
| Watchdog never fires (testing with `HangDetect`) | The compiler optimised your busy-wait away | Use `volatile` in your hang loop, or trust the included example which is structured to survive optimization |
| First boot after upload shows `RCAUSE = 0x10` (EXT) | `bossac` issued an external-pin-equivalent reset on its way out | Normal — that's the upload-induced reset, not a real EXT pin event |

---

## How this was tested

Bench-validated on a real Teknic ClearCore as part of the [KegWasher](https://github.com/RobotBuzzard/KegWasher) project bring-up:

1. **Compile + flash** — sketch builds clean, uploads, runs.
2. **Normal-op stability** — 30 s of polling at 2 Hz showed zero `lsusb` device-number transitions (no spurious resets).
3. **Deliberate hang** — `while(1)` injected at the top of `loop()`. Observed 3 reset cycles in 60 s, USB device numbers `057 → 058 → 059 → 060` — period ~20 s (8 s WDT timeout + ~12 s of `setup()` reboot delays in that particular firmware).
4. **Recovery sanity** — reverted the hang, re-flashed, 30 s stable. Watchdog kicks itself out of the way.

Reproduce the hang test with `examples/HangDetect`.

---

## References

- [Teknic ClearCore product page](https://www.teknic.com/products/io-motion-controller-clearcore/)
- [Teknic ClearCore Arduino library docs](https://teknic-inc.github.io/ClearCore-library/) (note: no WDT API, which is why this library exists)
- [Microchip SAME53 datasheet](https://www.microchip.com/en-us/product/atsame53n19a) — Watchdog Timer chapter
- [teknic-clearcore-cli](https://github.com/RobotBuzzard/teknic-clearcore-cli) — sibling repo with the canonical install / compile / flash workflow

---

## License

MIT — see [LICENSE](LICENSE).
