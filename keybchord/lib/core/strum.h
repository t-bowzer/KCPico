#pragma once

#include <cstdint>
#include <vector>

#include "chords.h"


enum class StrumLayout : uint8_t {
    Full = 0,
    Limited,
    COUNT
};

// Largest strum key set across layouts (full numpad = 11 keys).
constexpr int STRUM_MAX_KEYS = 11;

// Maps a raw HID usage to its strum position (0..N-1) for the given layout, or
// -1 if the usage is not a strum key in that layout. Num-Lock independent: the
// keypad digits/decimal are matched by their raw keypad usages and the number
// row by its own usages; Num Lock is never asserted. Keypad + / - are never
// strum keys (reserved for octave/value).
int strumIndexFor(uint8_t hid_usage, StrumLayout layout);

// Number of strum keys in a layout (full = 11 to cover the numpad superset,
// limited = 10).
int strumKeyCount(StrumLayout layout);

// Ascending note pool derived from a resolved chord's pitch classes, placed at
// base_root_midi + 12 * strumOctave and spread across octaves. Returns exactly
// `count` notes, clamped to MIDI 0..127. Integer only (spec section 6.6).
std::vector<uint8_t> buildNotePool(const ResolvedChord& chord,
                                   uint8_t base_root_midi,
                                   int strumOctave,
                                   size_t count);
