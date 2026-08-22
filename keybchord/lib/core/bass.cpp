#include "bass.h"


namespace {

// Four-beat interval blueprint per chord type (spec 6.8). Beat 4 uses the
// chord's 7th when present, else the 6th (offset 9).
constexpr int8_t kBlueprints[][4] = {
    {0, 4, 7, 9},    // Major
    {0, 3, 7, 9},    // Minor
    {0, 4, 7, 10},   // Dom7
    {0, 4, 7, 11},   // Maj7
    {0, 3, 7, 10},   // Min7
    {0, 3, 6, 9},    // Dim
    {0, 3, 6, 9},    // Dim7
    {0, 4, 8, 9},    // Aug
    {0, 5, 7, 9},    // Sus4
    {0, 2, 7, 9},    // Sus2
    {0, 3, 6, 10},   // Min7b5
};

} // namespace


int bassOffsetForBeat(ChordType type, int beat, int beatsPerBar) {
    if (beatsPerBar <= 0) beatsPerBar = 4;
    if (beat < 0) beat = 0;
    if (beat >= beatsPerBar) beat = beatsPerBar - 1;

    int t = static_cast<int>(type);
    if (t < 0 || t >= static_cast<int>(ChordType::COUNT)) t = 0;
    return kBlueprints[t][beat];
}

int bassOffsetForPattern(BassPattern pattern, ChordType type, int beat, int beatsPerBar) {
    if (beatsPerBar <= 0) beatsPerBar = 4;
    if (beat < 0) beat = 0;
    if (beat >= beatsPerBar) beat = beatsPerBar - 1;

    switch (pattern) {
        case BassPattern::Whole:
            return (beat == 0) ? 0 : -1;
        case BassPattern::Half:
            return (beat % 2 == 0) ? 0 : -1;
        case BassPattern::Quarter:
            return 0;
        case BassPattern::HalfAlt:
            // Half notes alternating root/5th: root on the first half, 5th on
            // the second.
            return (beat % 2 == 0) ? (((beat / 2) % 2 == 0) ? 0 : 7) : -1;
        case BassPattern::QuarterAlt:
            // Quarter notes alternating root/5th.
            return (beat % 2 == 0) ? 0 : 7;
        case BassPattern::ThreeFourAlt:
            // Root on beat 1, 5th on the last beat of the bar.
            if (beat == 0) return 0;
            if (beat == beatsPerBar - 1) return 7;
            return -1;
        case BassPattern::Hold:
            return 0;  // root, handled separately (not beat-driven)
        case BassPattern::WalkNoSixth:
            // Root (half note) -> 3rd (quarter) -> 5th (quarter) -> repeat.
            switch (beat % 4) {
                case 0: return 0;
                case 1: return -1;
                case 2: return bassOffsetForBeat(type, 1, beatsPerBar);  // 3rd
                default: return bassOffsetForBeat(type, 2, beatsPerBar); // 5th
            }
        case BassPattern::Walking:
        default:
            return bassOffsetForBeat(type, beat, beatsPerBar);
    }
}

int bassSustainBeats(BassPattern pattern, int beat, int beatsPerBar) {
    if (beatsPerBar <= 0) beatsPerBar = 4;
    switch (pattern) {
        case BassPattern::Whole:
            return beatsPerBar;
        case BassPattern::Half:
        case BassPattern::HalfAlt:
            return 2;
        case BassPattern::WalkNoSixth:
            return (beat % 4 == 0) ? 2 : 0;  // root = half note, 3rd/5th percussive
        default:
            return 0;  // percussive: use note_duration_ms
    }
}

int bassNote(ChordType type, int rootPc, uint8_t base_root_midi,
             int bassOctave, int beat, int beatsPerBar) {
    int root = rootMidi(rootPc, base_root_midi, bassOctave);
    return root + bassOffsetForBeat(type, beat, beatsPerBar);
}
