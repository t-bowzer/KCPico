#pragma once

#include <cstdint>

// Debug log + MIDI monitor (NFR-8).
//
// Two independent gates:
//  * Compile-time: KEYBCHORD_LOG. When undefined (release builds), every log
//    call compiles to a no-op with zero runtime cost.
//  * Run-time: g_debugLogEnabled (key/strum/LED/info lines) and
//    g_midiMonitorEnabled (outgoing MIDI decode), set from config.json at boot.
//
// On the native test host (KEYBCHORD_NATIVE) logging is always compiled out so
// the pure core never touches a serial port.

#if defined(KEYBCHORD_LOG) && !defined(KEYBCHORD_NATIVE)
  #include <Arduino.h>
  #include "naming.h"
  #define KB_LOG_ON 1
#else
  #define KB_LOG_ON 0
#endif

inline bool g_debugLogEnabled   = true;
inline bool g_midiMonitorEnabled = true;

inline void logInit() {
#if KB_LOG_ON
    Serial.begin(115200);
    delay(200);
    Serial.println("KeybChord Pico -- M8");
#endif
}

inline void logKeyEvent(uint8_t usage, bool pressed, uint8_t mods) {
#if KB_LOG_ON
    if (!g_debugLogEnabled) return;
    unsigned long t = millis();
    Serial.print("[");
    Serial.print(t);
    Serial.print("] KEY ");
    Serial.print(pressed ? "DN " : "UP ");
    Serial.print("usage=0x");
    Serial.print(usage, HEX);
    Serial.print(" mods=0x");
    Serial.println(mods, HEX);
#endif
}

inline void logMidiOut(uint8_t status, uint8_t data1, uint8_t data2) {
#if KB_LOG_ON
    if (!g_midiMonitorEnabled) return;
    unsigned long t = millis();
    Serial.print("[");
    Serial.print(t);
    Serial.print("] MIDI ch=");
    Serial.print((status & 0x0F) + 1);
    Serial.print(" ");
    Serial.print(messageTypeName(status));
    if ((status & 0xF0) == 0x90 || (status & 0xF0) == 0x80) {
        Serial.print(" ");
        Serial.print(noteName(data1).c_str());
        Serial.print(" vel=");
        Serial.print(data2);
    } else if ((status & 0xF0) == 0xB0) {
        Serial.print(" ");
        Serial.print(ccName(data1).c_str());
        Serial.print(" val=");
        Serial.print(data2);
    } else {
        Serial.print(" d1=");
        Serial.print(data1);
        Serial.print(" d2=");
        Serial.print(data2);
    }
    Serial.println();
#endif
}

inline void logStrumPlay(uint8_t idx, uint8_t note, uint8_t channel, int16_t duration_ms) {
#if KB_LOG_ON
    if (!g_debugLogEnabled) return;
    unsigned long t = millis();
    Serial.print("[");
    Serial.print(t);
    Serial.print("] STRUM idx=");
    Serial.print(idx);
    Serial.print(" note=");
    Serial.print(note);
    Serial.print(" ch=");
    Serial.print(channel);
    Serial.print(" dur_ms=");
    Serial.println(duration_ms);
#endif
}

inline void logStrumEdit(const char* param, int16_t value) {
#if KB_LOG_ON
    if (!g_debugLogEnabled) return;
    unsigned long t = millis();
    Serial.print("[");
    Serial.print(t);
    Serial.print("] STRUM EDIT ");
    Serial.print(param);
    Serial.print("=");
    Serial.println(value);
#endif
}

inline void logLed(uint8_t usage, bool on, bool ok) {
#if KB_LOG_ON
    if (!g_debugLogEnabled) return;
    unsigned long t = millis();
    Serial.print("[");
    Serial.print(t);
    Serial.print("] LED usage=0x");
    Serial.print(usage, HEX);
    Serial.print(" on=");
    Serial.print(on ? 1 : 0);
    Serial.print(" ok=");
    Serial.println(ok ? 1 : 0);
#endif
}

inline void logLedComplete(uint8_t dev_addr, uint16_t len) {
#if KB_LOG_ON
    if (!g_debugLogEnabled) return;
    unsigned long t = millis();
    Serial.print("[");
    Serial.print(t);
    Serial.print("] LED COMPLETE dev=");
    Serial.print(dev_addr);
    Serial.print(" len=");
    Serial.println(len);  // 0 = transfer failed/stalled; >0 = success
#endif
}

inline void logPerf(const char* label, uint32_t count, uint32_t minUs,
                    uint32_t maxUs, uint32_t avgUs) {
#if KB_LOG_ON
    if (!g_debugLogEnabled) return;
    Serial.print("[PERF] ");
    Serial.print(label);
    Serial.print(" n=");
    Serial.print(count);
    Serial.print(" min=");
    Serial.print(minUs);
    Serial.print("us max=");
    Serial.print(maxUs);
    Serial.print("us avg=");
    Serial.print(avgUs);
    Serial.println("us");
#endif
}

inline void logInfo(const char* msg) {
#if KB_LOG_ON
    if (!g_debugLogEnabled) return;
    Serial.print("[INFO] ");
    Serial.println(msg);
#endif
}

inline void logWarn(const char* msg) {
#if KB_LOG_ON
    if (!g_debugLogEnabled) return;
    Serial.print("[WARN] ");
    Serial.println(msg);
#endif
}

inline void logError(const char* msg) {
#if KB_LOG_ON
    if (!g_debugLogEnabled) return;
    Serial.print("[ERROR] ");
    Serial.println(msg);
#endif
}
