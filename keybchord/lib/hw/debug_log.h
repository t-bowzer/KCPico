#pragma once

#include <cstdint>

#ifndef KEYBCHORD_NATIVE
#include <Arduino.h>
#endif


inline void logInit() {
#ifndef KEYBCHORD_NATIVE
    Serial.begin(115200);
    delay(200);
    Serial.println("KeybChord Pico -- M2 Core Framework");
#endif
}

inline void logKeyEvent(uint8_t usage, bool pressed, uint8_t mods) {
#ifndef KEYBCHORD_NATIVE
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
#ifndef KEYBCHORD_NATIVE
    unsigned long t = millis();
    Serial.print("[");
    Serial.print(t);
    Serial.print("] MIDI OUT st=0x");
    Serial.print(status, HEX);
    Serial.print(" d1=");
    Serial.print(data1);
    Serial.print(" d2=");
    Serial.println(data2);
#endif
}

inline void logStrumPlay(uint8_t idx, uint8_t note, uint8_t channel, int16_t duration_ms) {
#ifndef KEYBCHORD_NATIVE
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
#ifndef KEYBCHORD_NATIVE
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
#ifndef KEYBCHORD_NATIVE
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
#ifndef KEYBCHORD_NATIVE
    unsigned long t = millis();
    Serial.print("[");
    Serial.print(t);
    Serial.print("] LED COMPLETE dev=");
    Serial.print(dev_addr);
    Serial.print(" len=");
    Serial.println(len);  // 0 = transfer failed/stalled; >0 = success
#endif
}

inline void logInfo(const char* msg) {
#ifndef KEYBCHORD_NATIVE
    Serial.print("[INFO] ");
    Serial.println(msg);
#endif
}

inline void logWarn(const char* msg) {
#ifndef KEYBCHORD_NATIVE
    Serial.print("[WARN] ");
    Serial.println(msg);
#endif
}

inline void logError(const char* msg) {
#ifndef KEYBCHORD_NATIVE
    Serial.print("[ERROR] ");
    Serial.println(msg);
#endif
}


