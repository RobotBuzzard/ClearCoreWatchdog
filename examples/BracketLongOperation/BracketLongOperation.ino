// ======================================================================
// BracketLongOperation.ino — Disable/enable around blocking work
// ======================================================================
// Real applications often have legitimate operations that exceed the
// watchdog timeout: hardware self-tests, calibration sweeps, slow
// sensor probes, factory provisioning sequences. The bracket pattern
// is: capture the watchdog state, disable, run the long op, restore.
//
// This example simulates a 12-second blocking sequence inside a
// running watchdog. Without the bracket the WDT would reset us;
// with it, the long op runs to completion and the WDT continues
// after.
//
// Open the Serial Monitor at 115200 baud.
// ======================================================================
#include <ClearCoreWatchdog.h>

static void simulateLongOperation() {
  Serial.println(F("Long op: start (12 seconds)"));
  bool wasEnabled = Watchdog.isEnabled();
  if (wasEnabled) {
    Watchdog.disable();
    Serial.println(F("  WDT disabled for the duration"));
  }

  for (int i = 1; i <= 12; i++) {
    Serial.print(F("  step "));
    Serial.print(i);
    Serial.println(F("/12"));
    delay(1000);
  }

  if (wasEnabled) {
    Watchdog.enable();
    Serial.println(F("  WDT re-enabled"));
  }
  Serial.println(F("Long op: done"));
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { /* spin */ }

  uint8_t cause = Watchdog.lastResetCause();
  Serial.print(F("Reset cause: 0x"));
  Serial.println(cause, HEX);
  if (cause & 0x20) {
    Serial.println(F("  WARNING: Previous boot ended in a WDT reset — bracket may be missing somewhere"));
  }

  Watchdog.enable(WdtPeriod_4s);   // tighter than default to demonstrate
  Serial.println(F("Watchdog armed (4s) — long op starts in 2s"));
  delay(2000);

  simulateLongOperation();
  Serial.println(F("Returning to main loop with WDT still armed."));
}

void loop() {
  static uint32_t kicks = 0;
  Watchdog.kick();
  if ((kicks++ % 20) == 0) {
    Serial.print(F("kick #"));
    Serial.println(kicks);
  }
  delay(100);
}
