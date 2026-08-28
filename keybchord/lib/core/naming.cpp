#include "naming.h"

#include <cstdio>

#include "rhythm.h"


const char* playModeShort(PlayMode mode) {
    switch (mode) {
        case PlayMode::Held:        return "Held";
        case PlayMode::PressToPlay: return "Press";
        case PlayMode::Arpeggio:    return "Arp";
        case PlayMode::ArpHold:     return "ArpHld";
        case PlayMode::Silent:      return "Silent";
        default:                    return "Held";
    }
}

const char* voicingModeName(VoicingMode mode) {
    switch (mode) {
        case VoicingMode::RootPosition: return "Root";
        case VoicingMode::Smart:        return "Smart";
        case VoicingMode::Down:         return "Down";
        case VoicingMode::Up:           return "Up";
        default:                        return "Root";
    }
}

namespace {

// First two characters of `s` (verbatim, possibly fewer if shorter).
const char* firstTwoChars(const char* s) {
    static char buf[3];
    int n = 0;
    for (const char* p = s; p && *p && n < 2; ++p) {
        buf[n++] = *p;
    }
    buf[n] = '\0';
    return buf;
}

// Derived 2-char code from a pattern name: first two alphanumeric characters,
// uppercased.
const char* codeFromName(const char* name) {
    static char buf[3];
    int n = 0;
    for (const char* p = name; p && *p && n < 2; ++p) {
        char c = *p;
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            buf[n++] = c;
        }
    }
    if (n == 0) { buf[0] = 'U'; buf[1] = 's'; n = 2; }
    buf[n] = '\0';
    return buf;
}

} // namespace


const char* rhythmShortCode(int index) {
    static const char* const kCodes[RHYTHM_COUNT] = {
        "Rk", "R2", "Wz", "Sw", "SR", "BN",
        "Rb", "Tg", "Mr", "Sb", "Ds", "Fx",
    };
    // Out of range (no such pattern) falls back to the first built-in.
    if (index < 0 || index >= rhythmCount()) return kCodes[0];

    // An authored short_name always wins (truncated to two characters).
    const char* s = rhythmShortName(index);
    if (s && s[0]) return firstTwoChars(s);

    if (index < RHYTHM_COUNT) return kCodes[index];

    // User-defined rhythm without a short_name: derive from its name.
    return codeFromName(rhythmName(index));
}

std::string noteName(uint8_t note) {
    static const char* const kPc[12] = {
        "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B",
    };
    char buf[8];
    int octave = static_cast<int>(note) / 12 - 1;  // MIDI 60 -> C4
    snprintf(buf, sizeof(buf), "%s%d", kPc[note % 12], octave);
    return std::string(buf);
}

std::string ccName(uint8_t cc) {
    switch (cc) {
        case 0:   return "BankMSB";
        case 1:   return "ModWheel";
        case 7:   return "Volume";
        case 10:  return "Pan";
        case 11:  return "Expression";
        case 64:  return "Sustain";
        case 120: return "AllSoundOff";
        case 121: return "ResetAll";
        case 123: return "AllNotesOff";
        default:  return "CC" + std::to_string(cc);
    }
}

const char* messageTypeName(uint8_t status) {
    switch (status & 0xF0) {
        case 0x80: return "NoteOff";
        case 0x90: return "NoteOn";
        case 0xA0: return "PolyPressure";
        case 0xB0: return "CC";
        case 0xC0: return "ProgramChange";
        case 0xD0: return "ChanPressure";
        case 0xE0: return "PitchBend";
        default:
            switch (status) {
                case 0xF8: return "Clock";
                case 0xFA: return "Start";
                case 0xFB: return "Continue";
                case 0xFC: return "Stop";
                default:   return "System";
            }
    }
}
