#pragma once

#include <cstdint>
#include <string>
#include "params.h"


constexpr int NUM_BANKS   = 10;
constexpr int NUM_SLOTS   = 8;
constexpr int NUM_PRESETS = NUM_BANKS * NUM_SLOTS;

constexpr uint8_t DEFAULT_CHORD_CHANNEL  = 1;
constexpr uint8_t DEFAULT_STRUM_CHANNEL  = 2;
constexpr uint8_t DEFAULT_RHYTHM_CHANNEL = 10;

struct PresetSlot {
    std::string name;
    ChordParams chord;
    StrumParams strum;
    RhythmParams rhythm;

    static PresetSlot defaults();
    bool operator==(const PresetSlot& other) const;
    bool operator!=(const PresetSlot& other) const;

    // True when every chord/strum/rhythm field matches, ignoring the name
    // (used for dirty-state tracking, FR-P11 / AC-20).
    bool sameParams(const PresetSlot& other) const;
};

class StorageAdapter;

PresetSlot   loadPreset(StorageAdapter& storage, int bank, int slot);
bool         savePreset(StorageAdapter& storage, int bank, int slot, const PresetSlot& preset);
PresetSlot   loadPresetOrDefault(StorageAdapter& storage, int bank, int slot);

// Builds a PresetSlot from the live pending parameters (name supplied by the
// caller; e.g. the stored slot's name when computing dirty state).
PresetSlot makePreset(const ChordParams& chord, const StrumParams& strum,
                      const RhythmParams& rhythm, const std::string& name);

// Parses a preset location string like "B3:P7" into 0-based bank/slot.
// Returns false on malformed input or out-of-range values (1..10 / 1..8).
bool parsePresetLocation(const std::string& loc, int& bank, int& slot);


