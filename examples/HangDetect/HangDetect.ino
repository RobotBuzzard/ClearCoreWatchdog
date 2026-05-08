// ======================================================================
// HangDetect.ino — Demonstrate the watchdog firing after a deliberate hang
// ======================================================================
// After flashing, the device will:
//   1. Print "About to hang..." with the previous reset cause
//   2. Stop kicking the watchdog (an infinite empty loop)
//   3. Reset itself ~8 seconds later
//   4. On the next boot, lastResetCause() will report bit 0x20 set,
//      indicating the watchdog was the trigger
//
// Open the Serial Monitor at 115200 baud and watch the print/reset
// cycle. Expected period: ~9 seconds (8s WDT + ~1s of reboot).
//
// To recover the device after testing: flash any other sketch that
// kicks the watchdog regularly (or doesn't enable it at all).
// ======================================================================
#include <ClearCoreWatchdog.h>

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { /* spin */ }

  uint8_t cause = Watchdog.lastResetCause();
  Serial.print(F("Reset cause: 0x"));
  Serial.println(cause, HEX);
  if (cause & 0x20) {
    Serial.println(F("  -> Confirmed: WDT bit set, recovered from a watchdog reset!"));
  }

  Watchdog.enable(WdtPeriod_8s);
  Serial.println(F("Watchdog armed. Hanging in 2 seconds..."));
  delay(2000);

  Serial.println(F("Hanging now — expect reset in ~8s"));
  Serial.flush();

  // Deliberate infinite loop with no kick. This is what a hung firmware
  // looks like to the WDT.
  while (true) {
    /* nothing — never kick the watchdog */
  }
}

void loop() {
  // Never reached
}
