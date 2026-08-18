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

int bassNote(ChordType type, int rootPc, uint8_t base_root_midi,
             int bassOctave, int beat, int beatsPerBar) {
    int root = rootMidi(rootPc, base_root_midi, bassOctave);
    return root + bassOffsetForBeat(type, beat, beatsPerBar);
}
