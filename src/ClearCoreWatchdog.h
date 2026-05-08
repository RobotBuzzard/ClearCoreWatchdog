// ======================================================================
// ClearCoreWatchdog.h
// ======================================================================
// Hardware watchdog timer for the Teknic ClearCore (Microchip SAME53N19A).
// Direct CMSIS register access — Teknic's ClearCore Arduino library
// does not expose a watchdog API (verified against the published API
// reference; SysManager has only ConnectorByIndex and ResetBoard).
//
// Bench-validated on a real ClearCore: deliberate-hang test confirms
// the dog bites within ~8 seconds; bracketed long operations
// (disable/enable around the blocking section) do not trigger
// spurious resets.
//
// Usage:
//   #include <ClearCoreWatchdog.h>
//
//   void setup() {
//     Watchdog.enable();          // default 8s
//     // OR Watchdog.enable(WdtPeriod_4s);
//   }
//
//   void loop() {
//     // ...your work...
//     Watchdog.kick();             // call at least every period
//   }
//
// ISR safety: do NOT call kick() / enable() / disable() from an
// interrupt handler. The SYNCBUSY waits in those paths can deadlock
// under interrupt context.
// ======================================================================
#pragma once

#include <Arduino.h>

// Available timeout periods for the SAMD51/SAME53 WDT, fed by the
// 1.024 kHz tap from OSCULP32K. Values are the WDT_CONFIG_PER raw
// codes (CYC1024..CYC16384).
enum WdtPeriod : uint8_t {
  WdtPeriod_1s  = 0x7,    // 1024 cycles  / 1024 Hz
  WdtPeriod_2s  = 0x8,    // 2048 cycles
  WdtPeriod_4s  = 0x9,    // 4096 cycles
  WdtPeriod_8s  = 0xA,    // 8192 cycles  (default)
  WdtPeriod_16s = 0xB,    // 16384 cycles
};

class ClearCoreWatchdogClass {
public:
  // Enable the watchdog with the given timeout. Idempotent — safe to
  // call repeatedly; reconfigures cleanly if already running.
  // Default timeout is 8 seconds.
  void enable(WdtPeriod period = WdtPeriod_8s);

  // Disable the watchdog. Use to bracket deliberately-blocking code
  // that legitimately exceeds the timeout (long calibration sweeps,
  // self-tests, etc.). Re-enable with enable() on exit.
  void disable();

  // Kick (feed) the watchdog. Call at least once per timeout period
  // — typically once per loop() iteration. NOT safe to call from an
  // interrupt handler; the SYNCBUSY wait can deadlock there.
  void kick();

  // Whether the watchdog is currently enabled. Useful for code that
  // wants to bracket its own long-blocking sections without assuming
  // the watchdog state.
  bool isEnabled();

  // Latched reset cause from RSTC.RCAUSE on this chip's most recent
  // reset. Cached on first call so later RCAUSE writes (if any) don't
  // lose the boot-time information.
  //
  // Bits per the SAME53 datasheet:
  //   0x01 POR     0x10 EXT (external pin / SYSRESETREQ-equiv)
  //   0x02 BODCORE 0x20 WDT     ← non-zero AND'd with this means
  //   0x04 BODVDD  0x40 SYST     "we recovered from a hang"
  //   0x08 NVM     0x80 BACKUP
  //
  // Multiple bits can be set if causes pile up across resets.
  uint8_t lastResetCause();
};

// Global singleton instance. Use as `Watchdog.enable()`, `Watchdog.kick()`,
// etc. — matches the convention used by other Arduino watchdog libraries
// (e.g. Adafruit's SleepyDog).
extern ClearCoreWatchdogClass Watchdog;
