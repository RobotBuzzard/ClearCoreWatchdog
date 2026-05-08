// ======================================================================
// ClearCoreWatchdog.cpp
// ======================================================================
// SAMD51 / SAME53 watchdog timer via direct CMSIS register access.
//
// Clock source: OSC32KCTRL's CLK_OSC32K_1K (1.024 kHz tap from the
// always-on OSCULP32K oscillator). On SAMD51 this is wired internally
// to the WDT — unlike SAMD21, NO GCLK channel needs to be configured.
// The 1k tap is enabled by default at reset; we set EN1K explicitly
// for safety.
//
// Reset controller: SAME53 calls it RSTC, not RSTCTRL as on some
// other SAMD-family parts.
//
// Safety: WDT->CLEAR accepts only the magic key 0xA5
// (WDT_CLEAR_CLEAR_KEY). Writing anything else triggers an immediate
// reset. Always wait on SYNCBUSY.CLEAR before writing, and never
// call kick() from an ISR.
// ======================================================================
#include "ClearCoreWatchdog.h"
// sam.h is pulled in transitively by Arduino.h and provides WDT,
// OSC32KCTRL, MCLK, RSTC pointers + the WDT_*/RSTC_* constants.

// Map our enum to the WDT_CONFIG_PER values. The enum values ARE the
// raw register codes (deliberately) so the conversion is just a cast,
// but going through this function keeps the intent explicit.
static inline uint32_t periodToConfigReg(WdtPeriod p) {
  return (uint32_t)((uint8_t)p) << WDT_CONFIG_PER_Pos;
}

void ClearCoreWatchdogClass::enable(WdtPeriod period) {
  // Make sure the 1 kHz tap from OSCULP32K is on. Default-on at reset
  // but be explicit — something else (low-power transitions, etc.)
  // might have disabled it.
  OSC32KCTRL->OSCULP32K.bit.EN1K = 1;

  // Ensure APBA bus clock to the WDT peripheral is on. Default-on at
  // reset; defensive write.
  MCLK->APBAMASK.bit.WDT_ = 1;

  // If already running, disable so we can reconfigure.
  if (WDT->CTRLA.bit.ENABLE) {
    WDT->CTRLA.bit.ENABLE = 0;
    while (WDT->SYNCBUSY.reg) { /* wait for ENABLE=0 to sync */ }
  }

  // Disable the early-warning interrupt — we don't use it. (Could be
  // wired later to log "about to die" messages via Serial before the
  // reset actually fires.)
  WDT->INTENCLR.bit.EW = 1;

  // Configure period
  WDT->CONFIG.reg = periodToConfigReg(period);

  // Enable
  WDT->CTRLA.reg = WDT_CTRLA_ENABLE;
  while (WDT->SYNCBUSY.reg) { /* wait for ENABLE=1 to sync */ }
}

void ClearCoreWatchdogClass::disable() {
  if (!WDT->CTRLA.bit.ENABLE) return;
  WDT->CTRLA.bit.ENABLE = 0;
  while (WDT->SYNCBUSY.reg) { /* wait */ }
}

void ClearCoreWatchdogClass::kick() {
  // Wait for any in-flight CLEAR sync to complete before writing the
  // key — writing while a clear is still syncing triggers an immediate
  // reset.
  while (WDT->SYNCBUSY.bit.CLEAR) { /* wait */ }
  WDT->CLEAR.reg = WDT_CLEAR_CLEAR_KEY;
}

bool ClearCoreWatchdogClass::isEnabled() {
  return WDT->CTRLA.bit.ENABLE != 0;
}

uint8_t ClearCoreWatchdogClass::lastResetCause() {
  static uint8_t cached = 0;
  static bool cached_valid = false;
  if (!cached_valid) {
    cached = RSTC->RCAUSE.reg;
    cached_valid = true;
  }
  return cached;
}

// Global singleton instance — declared `extern` in the header.
ClearCoreWatchdogClass Watchdog;
