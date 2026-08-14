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
};

class StorageAdapter;

PresetSlot   loadPreset(StorageAdapter& storage, int bank, int slot);
bool         savePreset(StorageAdapter& storage, int bank, int slot, const PresetSlot& preset);
PresetSlot   loadPresetOrDefault(StorageAdapter& storage, int bank, int slot);


