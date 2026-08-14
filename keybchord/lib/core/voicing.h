#pragma once

#include <cstdint>
#include <vector>

#include "chords.h"


// Root-position voicing: root placed within [low, high], intervals stacked
// ascending; result clamped to MIDI 0..127. Integer only (spec section 6.4).
std::vector<uint8_t> voiceRootPosition(const ResolvedChord& chord,
                                       uint8_t base_root_midi,
                                       int octave,
                                       uint8_t low,
                                       uint8_t high);

// Smart voice-leading: choose the inversion nearest the previous voicing by
// integer semitone distance. Falls back to root position when `previous`
// is empty or sizes mismatch. (spec section 6.4, VR-2)
std::vector<uint8_t> voiceSmart(const ResolvedChord& chord,
                                uint8_t base_root_midi,
                                int octave,
                                uint8_t low,
                                uint8_t high,
                                const std::vector<uint8_t>& previous);

