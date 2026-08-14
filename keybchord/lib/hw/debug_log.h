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


