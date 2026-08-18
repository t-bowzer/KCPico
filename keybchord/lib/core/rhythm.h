#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "params.h"


// 16th-note step resolution: 4 steps per beat. All timing math is integer.
constexpr int RHYTHM_STEPS_PER_BEAT = 4;

// Standard MIDI clock: 24 PPQN -> 6 clock ticks per 16th-note step.
constexpr int MIDI_PPQN            = 24;
constexpr int CLOCK_TICKS_PER_STEP = MIDI_PPQN / RHYTHM_STEPS_PER_BEAT;

// Number of shipped rhythm patterns (spec section 7.1).
constexpr int RHYTHM_COUNT = 12;

// Velocity that a `1` in a track's pattern maps to (spec 7.2: "1 = default
// velocity"). Values 2..127 are literal velocities.
constexpr uint8_t RHYTHM_DEFAULT_VELOCITY = 100;


struct RhythmTrack {
    uint8_t note = 0;                 // GM percussion note
    std::string name;
    std::vector<uint8_t> pattern;     // 0 = rest, 1..127 = velocity
};

struct RhythmPattern {
    std::string name;
    int steps_per_bar = 16;
    int8_t swing = 0;                 // per-pattern default swing -75..+75
    std::vector<RhythmTrack> tracks;

    int beatsPerBar() const { return steps_per_bar / RHYTHM_STEPS_PER_BEAT; }
};

struct StepEvent {
    uint8_t note;
    uint8_t velocity;
};


// Parses a rhythm pattern JSON document (spec section 7.2). Returns false and
// leaves `out` untouched on malformed input.
bool parseRhythmPattern(const std::string& json, RhythmPattern& out);

// Base 16th-note step duration (us) for a tempo in BPM (integer division).
uint32_t stepUs(uint16_t bpm);

// MIDI clock tick interval (us) for a tempo (24 PPQN, 16th-note steps).
uint32_t clockTickUs(uint16_t bpm);

// Nominal start offset (us) of a step within a bar, including fixed-point swing
// (spec 7.3): the off-beat 8th note of each beat (the "and", i.e. the step at
// RHYTHM_STEPS_PER_BEAT/2 within the beat) is delayed by
// (base_step_us * swing) / 100. Swing is signed: positive pushes the off-beat
// later (laid-back), negative pulls it earlier (rushed).
uint64_t stepOffsetUs(int step, uint32_t base_step_us, int8_t swing);

// The GM notes + velocities that fire at a given step index.
std::vector<StepEvent> stepEvents(const RhythmPattern& p, int step);

// Remaps a pattern's GM percussion note through the preset drum map (FR-R3).
// Standard GM codes (kick/snare/hats) are translated; all others pass through.
uint8_t mapDrumNote(uint8_t note, const DrumMap& drums);

// Velocity override for a standard GM drum code (Upgrade-Plan "velocity per drum
// piece"). A piece whose override is 0 follows the pattern's authored step
// velocity; 1..127 forces a fixed velocity. Non-mapped codes pass through.
uint8_t mapDrumVelocity(uint8_t note, const DrumMap& drums, uint8_t patternVelocity);

// Rhythm list (spec 7.1): index <-> name lookups.
const char* rhythmName(int index);
int rhythmIndex(const std::string& name);

// LittleFS file name (no directory) for a rhythm by index.
const char* rhythmFileName(int index);
