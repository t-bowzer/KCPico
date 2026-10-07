#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "chords.h"


// Number of shipped chord patterns (spec 6.10). These are always loaded first
// (indices 0..CHORD_PATTERN_COUNT-1); user-provided /chordpatterns/*.json files
// are appended after them, mirroring the rhythm/bass lists.
constexpr int CHORD_PATTERN_COUNT = 9;

// Hard cap on the total number of chord patterns (built-in + user) in memory.
constexpr int MAX_CHORD_PATTERNS = 32;

// Chord-degree bit codes used in a pattern's `steps` array. A step is a bitmask:
// bit 0 = root, bit 1 = 3rd, bit 2 = 5th, bit 3 = 7th. 0 = rest. Multiple bits
// in one step are played together (polyphonic). Degree 3 (bit 3) resolves to the
// chord's 7th when present, otherwise to the root an octave up ("wrap").
constexpr uint8_t CHORD_DEGREE_ROOT  = 0x01;
constexpr uint8_t CHORD_DEGREE_THIRD = 0x02;
constexpr uint8_t CHORD_DEGREE_FIFTH = 0x04;
constexpr uint8_t CHORD_DEGREE_SEVENTH = 0x08;

// Convenience masks.
constexpr uint8_t CHORD_MASK_TRIAD  = CHORD_DEGREE_ROOT | CHORD_DEGREE_THIRD | CHORD_DEGREE_FIFTH;  // 0x07
constexpr uint8_t CHORD_MASK_SEVENTH = CHORD_MASK_TRIAD | CHORD_DEGREE_SEVENTH;                    // 0x0F


struct ChordPattern {
    std::string name;
    int steps_per_bar = 16;
    std::vector<uint8_t> steps;          // bitmask per step (CHORD_DEGREE_*)
    std::vector<uint8_t> sustain_steps;  // optional; 0 = percussive

    int beatsPerBar() const { return steps_per_bar / 4; }
    bool valid() const { return !steps.empty(); }
};

struct ChordPatternLibrary {
    std::vector<ChordPattern> patterns;
    std::vector<std::string>  names;
};


// Parses a chord pattern JSON document (spec 6.10). Returns false and leaves
// `out` untouched on malformed input or when the pattern has no note data.
bool parseChordPattern(const std::string& json, ChordPattern& out);

// Semitone offsets (ascending) for the degrees set in `mask`, per chord type.
// Degree 3 uses the chord's 7th when present, else 12 (root an octave up).
std::vector<int> chordPatternOffsets(ChordType type, uint8_t mask);

// Number of 16th-note steps the note(s) at `step` sustain (0 = percussive).
int chordPatternStepSustain(const ChordPattern& p, int step);

// Chord pattern list (spec 6.10): index <-> name lookups, mirroring bass.h.
const char* chordPatternName(int index);
int chordPatternIndex(const std::string& name);
int chordPatternCount();

// Installs the full runtime name list (built-ins + user files).
void installChordPatternNames(std::vector<std::string> names);
void clearChordPatternNames();

// LittleFS file name (no directory) for a *built-in* chord pattern by index.
const char* chordPatternFileName(int index);
