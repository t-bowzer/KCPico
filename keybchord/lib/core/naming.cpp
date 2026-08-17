#include "naming.h"

#include <cstdio>

#include "rhythm.h"


const char* playModeShort(PlayMode mode) {
    switch (mode) {
        case PlayMode::Held:        return "Held";
        case PlayMode::PressToPlay: return "Press";
        case PlayMode::Arpeggio:    return "Arp";
        case PlayMode::Rhythm:      return "Rhythm";
        case PlayMode::Silent:      return "Silent";
        default:                    return "Held";
    }
}

const char* voicingModeName(VoicingMode mode) {
    switch (mode) {
        case VoicingMode::RootPosition: return "Root";
        case VoicingMode::Smart:        return "Smart";
        default:                        return "Root";
    }
}

const char* rhythmShortCode(int index) {
    static const char* const kCodes[RHYTHM_COUNT] = {
        "Rk", "R2", "Wz", "Sw", "SR", "BN",
        "Rb", "Tg", "Mr", "Sb", "Ds", "Fx",
    };
    if (index < 0 || index >= RHYTHM_COUNT) return kCodes[0];
    return kCodes[index];
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
