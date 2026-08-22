#include "strum.h"

#include <algorithm>


namespace {

// Full layout, number row: 1 2 3 4 5 6 7 8 9 0 -> positions 0..9.
constexpr uint8_t kNumberRowFull[10] = {
    0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
};

// Full layout, numpad: 0 . 1 2 3 4 5 6 7 8 9 NumLock / * -> positions 0..13.
// Num Lock, / and * sit at the top of the numpad and extend the full strum
// plate (FR-S7/S8).
constexpr uint8_t kNumpadFull[14] = {
    0x62, 0x63, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61,
    0x53, 0x54, 0x55,
};

// Limited layout, numpad center path: 0 . 2 3 5 6 8 9 / * -> positions 0..9.
constexpr uint8_t kNumpadLimited[10] = {
    0x62, 0x63, 0x5A, 0x5B, 0x5D, 0x5E, 0x60, 0x61, 0x54, 0x55,
};

int indexOf(const uint8_t* table, size_t size, uint8_t usage) {
    for (size_t i = 0; i < size; i++) {
        if (table[i] == usage) return static_cast<int>(i);
    }
    return -1;
}

// Scale/mode interval tables (semitones from root).
constexpr uint8_t kScaleIntervals[][7] = {
    {0, 2, 4, 5, 7, 9, 11},   // Ionian (major)
    {0, 2, 3, 5, 7, 9, 10},   // Dorian
    {0, 1, 3, 5, 7, 8, 10},   // Phrygian
    {0, 2, 4, 6, 7, 9, 11},   // Lydian
    {0, 2, 4, 5, 7, 9, 10},   // Mixolydian
    {0, 2, 3, 5, 7, 8, 10},   // Aeolian (natural minor)
    {0, 1, 3, 5, 6, 8, 10},   // Locrian
    {0, 2, 3, 5, 7, 8, 11},   // Harmonic minor
    {0, 2, 3, 5, 7, 9, 11},   // Melodic minor
    {0, 2, 4, 7, 9},          // Major pentatonic
    {0, 3, 5, 7, 10},         // Minor pentatonic
    {0, 3, 5, 6, 7, 10},      // Blues
};

constexpr uint8_t kScaleIntervalCounts[] = {
    7, 7, 7, 7, 7, 7, 7, 7, 7, 5, 5, 6,
};

const char* const kScaleNames[] = {
    "ionian", "dorian", "phrygian", "lydian", "mixolydian", "aeolian",
    "locrian", "harmonic_minor", "melodic_minor",
    "major_pentatonic", "minor_pentatonic", "blues",
};

const char* const kScaleShortNames[] = {
    "Ionian", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Aeolian",
    "Locrian", "Harm Min", "Mel Min", "Maj Pent", "Min Pent", "Blues",
};

} // namespace


int strumIndexFor(uint8_t hid_usage, StrumLayout layout) {
    switch (layout) {
        case StrumLayout::Full: {
            int np = indexOf(kNumpadFull, 14, hid_usage);
            if (np >= 0) return np;
            return indexOf(kNumberRowFull, 10, hid_usage);
        }
        case StrumLayout::Limited:
            return indexOf(kNumpadLimited, 10, hid_usage);
        default:
            return -1;
    }
}

int strumKeyCount(StrumLayout layout) {
    switch (layout) {
        case StrumLayout::Full:    return 14;
        case StrumLayout::Limited: return 10;
        default:                   return 0;
    }
}

const uint8_t* scaleIntervals(ScaleType type) {
    int i = static_cast<int>(type);
    if (i < 0 || i >= static_cast<int>(ScaleType::COUNT)) i = 0;
    return kScaleIntervals[i];
}

int scaleIntervalCount(ScaleType type) {
    int i = static_cast<int>(type);
    if (i < 0 || i >= static_cast<int>(ScaleType::COUNT)) i = 0;
    return kScaleIntervalCounts[i];
}

const char* scaleTypeName(ScaleType type) {
    int i = static_cast<int>(type);
    if (i < 0 || i >= static_cast<int>(ScaleType::COUNT)) i = 0;
    return kScaleNames[i];
}

const char* scaleTypeShortName(ScaleType type) {
    int i = static_cast<int>(type);
    if (i < 0 || i >= static_cast<int>(ScaleType::COUNT)) i = 0;
    return kScaleShortNames[i];
}

std::vector<uint8_t> buildNotePool(const ResolvedChord& chord,
                                   uint8_t base_root_midi,
                                   int strumOctave,
                                   size_t count) {
    std::vector<int> pcs = chordPitchClasses(chord);
    std::vector<uint8_t> out;
    if (pcs.empty() || count == 0) return out;
    out.reserve(count);

    int n = static_cast<int>(pcs.size());
    int anchor = static_cast<int>(base_root_midi) + 12 * strumOctave;

    for (size_t k = 0; k < count; k++) {
        int octave = static_cast<int>(k / static_cast<size_t>(n));
        int idx    = static_cast<int>(k % static_cast<size_t>(n));
        int note   = anchor + 12 * octave + pcs[idx];
        out.push_back(static_cast<uint8_t>(std::max(0, std::min(127, note))));
    }
    return out;
}

std::vector<uint8_t> buildScalePool(ScaleType type, uint8_t root_pc,
                                    uint8_t base_root_midi, int strumOctave,
                                    size_t count) {
    int n = scaleIntervalCount(type);
    const uint8_t* iv = scaleIntervals(type);
    std::vector<uint8_t> out;
    if (n <= 0 || count == 0) return out;
    out.reserve(count);

    int anchor = static_cast<int>(base_root_midi) + 12 * strumOctave + root_pc;

    for (size_t k = 0; k < count; k++) {
        int octave = static_cast<int>(k / static_cast<size_t>(n));
        int idx    = static_cast<int>(k % static_cast<size_t>(n));
        int note   = anchor + 12 * octave + iv[idx];
        out.push_back(static_cast<uint8_t>(std::max(0, std::min(127, note))));
    }
    return out;
}

std::vector<uint8_t> buildPianoPool(uint8_t root_pc, uint8_t base_root_midi,
                                    int strumOctave, size_t count) {
    std::vector<uint8_t> out;
    if (count == 0) return out;
    out.reserve(count);

    int anchor = static_cast<int>(base_root_midi) + 12 * strumOctave + root_pc;

    for (size_t k = 0; k < count; k++) {
        int note = anchor + static_cast<int>(k);
        out.push_back(static_cast<uint8_t>(std::max(0, std::min(127, note))));
    }
    return out;
}
