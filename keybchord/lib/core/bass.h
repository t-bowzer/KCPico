#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "chords.h"
#include "params.h"


// Number of shipped bass patterns (spec 6.8/6.9). These are always loaded first
// (indices 0..BASS_COUNT-1); user-provided /bass/*.json files are appended
// after them, mirroring the rhythm list.
constexpr int BASS_COUNT = 9;

// Hard cap on the total number of bass patterns (built-in + user) in memory.
constexpr int MAX_BASS_PATTERNS = 32;

// Chord-degree codes used in a pattern's `steps` array. A bass note is relative
// to the active chord root, so each step names a chord tone rather than a fixed
// MIDI note: root, 3rd, 5th, or 6th/7th. Resolved through the per-chord-type
// blueprint (bassDegreeOffset) so it follows the chord type (minor 3rd vs major
// 3rd, etc.).
constexpr int8_t BASS_DEGREE_REST           = -1;
constexpr int8_t BASS_DEGREE_ROOT           = 0;
constexpr int8_t BASS_DEGREE_THIRD          = 1;
constexpr int8_t BASS_DEGREE_FIFTH          = 2;
constexpr int8_t BASS_DEGREE_SIXTH_SEVENTH  = 3;


struct BassPattern {
    std::string name;
    int steps_per_bar = 16;
    std::vector<int8_t>  steps;          // chord-degree codes (BASS_DEGREE_*)
    std::vector<uint8_t> sustain_steps;  // optional; 0 = percussive
    bool hold = false;                   // special Hold pattern (not beat-driven)

    int beatsPerBar() const { return steps_per_bar / 4; }
    bool valid() const { return !steps.empty(); }
};

struct BassLibrary {
    std::vector<BassPattern> patterns;
    std::vector<std::string> names;
};


// Parses a bass pattern JSON document (spec 6.8/6.9). Returns false and leaves
// `out` untouched on malformed input or when the pattern has no note data.
bool parseBassPattern(const std::string& json, BassPattern& out);

// Semitone offset above the chord root for a chord-degree code (0..3), per
// chord type (reuses the spec 6.8 walking blueprint). Returns -1 for an
// invalid/out-of-range degree.
int bassDegreeOffset(ChordType type, int degree);

// Semitone offset above the root (or -1 for a rest) at `step` (0-based within
// the bar) for a configurable bass pattern. `Hold` always returns -1: it is
// handled separately by the engine, not beat-driven.
int bassStepOffset(const BassPattern& p, ChordType type, int step);

// Number of 16th-note steps the note at `step` sustains (0 = percussive, using
// the configured note_duration_ms).
int bassStepSustain(const BassPattern& p, int step);

// Bass pattern list (spec 6.9): index <-> name lookups. Resolve against the
// runtime name list installed by installBassNames(); when none is installed
// (the default) they fall back to the built-in table.
const char* bassName(int index);
int bassIndex(const std::string& name);
int bassCount();

// Installs the full runtime bass name list (built-ins + user files) used by
// bassName()/bassIndex()/bassCount(). Pass an empty vector (or call
// clearBassNames()) to revert to the built-in table. The list must stay in sync
// with the pattern vector passed to BassEngine::setPatterns().
void installBassNames(std::vector<std::string> names);
void clearBassNames();

// LittleFS file name (no directory) for a *built-in* bass pattern by index.
const char* bassFileName(int index);
