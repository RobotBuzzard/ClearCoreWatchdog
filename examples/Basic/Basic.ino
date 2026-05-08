// ======================================================================
// Basic.ino — Minimal ClearCoreWatchdog smoke test
// ======================================================================
// Arms the watchdog at 8 seconds, kicks it once per second, prints the
// reset cause from the previous boot. Open the Serial Monitor at
// 115200 baud to watch.
//
// Expected behavior: a counter prints every second forever. The
// watchdog is satisfied because kick() is called every loop iteration.
// ======================================================================
#include <ClearCoreWatchdog.h>

void setup() {
  Serial.begin(115200);
  // Wait briefly for the host to attach a Serial Monitor (optional).
  while (!Serial && millis() < 3000) { /* spin */ }

  uint8_t cause = Watchdog.lastResetCause();
  Serial.print(F("Reset cause: 0x"));
  Serial.println(cause, HEX);
  if (cause & 0x20) {
    Serial.println(F("  -> Recovered from a watchdog reset"));
  }

  Watchdog.enable();   // 8 seconds (default)
  Serial.println(F("Watchdog armed (8s)"));
}

void loop() {
  static uint32_t tick = 0;
  Serial.print(F("tick "));
  Serial.println(tick++);

  Watchdog.kick();
  delay(1000);
}
