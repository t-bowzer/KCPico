#pragma once

#include <cstdint>
#include <vector>

#include "chords.h"
#include "params.h"


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

// Full voicing pipeline (M10): base voicing (root/smart) + manual inversion +
// minimum-interval spread + minimum-note padding. All integer (spec 6.4 VR-6..9).
struct VoicingConfig {
    VoicingMode   voicing_mode = VoicingMode::RootPosition;
    InversionMode inversion    = InversionMode::Root;
    uint8_t       min_notes    = 3;
    uint8_t       min_interval = 0;   // 0 = off
};

std::vector<uint8_t> voiceChord(const ResolvedChord& chord,
                                uint8_t base_root_midi,
                                int octave,
                                uint8_t low,
                                uint8_t high,
                                const VoicingConfig& cfg,
                                const std::vector<uint8_t>& previous);
