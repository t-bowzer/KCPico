#include "naming.h"

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
