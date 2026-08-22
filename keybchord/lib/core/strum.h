#pragma once

#include <cstdint>
#include <vector>

#include "chords.h"
#include "params.h"


enum class StrumLayout : uint8_t {
    Full = 0,
    Limited,
    COUNT
};

// Largest strum key set across layouts (full numpad = 14 keys incl. Num Lock,
// / and *).
constexpr int STRUM_MAX_KEYS = 14;

// Maps a raw HID usage to its strum position (0..N-1) for the given layout, or
// -1 if the usage is not a strum key in that layout. Num-Lock independent: the
// keypad digits/decimal are matched by their raw keypad usages and the number
// row by its own usages; Num Lock is never asserted. Keypad + / - are never
// strum keys (reserved for octave/value).
int strumIndexFor(uint8_t hid_usage, StrumLayout layout);

// Number of strum keys in a layout (full = 14 to cover the numpad superset
// incl. Num Lock, / and *; limited = 10).
int strumKeyCount(StrumLayout layout);

// Scale/mode interval table (semitones from root) for the strum Scale mode.
const uint8_t* scaleIntervals(ScaleType type);
int            scaleIntervalCount(ScaleType type);

// Human-readable scale/mode names for the LCD and preset JSON.
const char* scaleTypeName(ScaleType type);
const char* scaleTypeShortName(ScaleType type);

// Ascending note pool derived from a resolved chord's pitch classes, placed at
// base_root_midi + 12 * strumOctave and spread across octaves. Returns exactly
// `count` notes, clamped to MIDI 0..127. Integer only (spec section 6.6).
std::vector<uint8_t> buildNotePool(const ResolvedChord& chord,
                                   uint8_t base_root_midi,
                                   int strumOctave,
                                   size_t count);

// Scale-mode pool: the selected scale/mode's ascending notes from a root pitch
// class, spread across octaves. `count` notes, clamped to MIDI 0..127.
std::vector<uint8_t> buildScalePool(ScaleType type, uint8_t root_pc,
                                    uint8_t base_root_midi, int strumOctave,
                                    size_t count);

// Piano-mode pool: chromatic — each successive strum key is one semitone higher
// than the last, anchored at base_root_midi + 12 * strumOctave + root_pc.
std::vector<uint8_t> buildPianoPool(uint8_t root_pc, uint8_t base_root_midi,
                                    int strumOctave, size_t count);
