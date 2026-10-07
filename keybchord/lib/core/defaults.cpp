#include "defaults.h"

#include "base.h"
#include "bass.h"
#include "chordpattern.h"
#include "config.h"
#include "keymap_config.h"
#include "presets.h"
#include "rhythm.h"


namespace {

// Shipped rhythm patterns (spec 7.1), embedded in code (pretty-printed) so the
// filesystem can be self-provisioned on first boot — there is no shipped
// filesystem image in M9.
const char* const kRhythmJson[RHYTHM_COUNT] = {
    // Rock 1
    R"({
  "name": "Rock 1",
  "steps_per_bar": 16,
  "swing": 0,
  "tracks": [
    { "note": 36, "name": "kick",  "pattern": [1,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0] },
    { "note": 38, "name": "snare", "pattern": [0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0] },
    { "note": 42, "name": "hihat", "pattern": [1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0] }
  ]
})",

    // Rock 2
    R"({
  "name": "Rock 2",
  "steps_per_bar": 16,
  "swing": 0,
  "tracks": [
    { "note": 36, "name": "kick",     "pattern": [1,0,0,0, 0,0,1,0, 1,0,0,0, 0,0,0,0] },
    { "note": 38, "name": "snare",    "pattern": [0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0] },
    { "note": 42, "name": "hihat",    "pattern": [1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0] },
    { "note": 46, "name": "open_hat", "pattern": [0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,1,0] }
  ]
})",

    // Waltz
    R"({
  "name": "Waltz",
  "steps_per_bar": 12,
  "swing": 0,
  "tracks": [
    { "note": 36, "name": "kick",  "pattern": [1,0,0,0, 0,0,0,0, 0,0,0,0] },
    { "note": 38, "name": "snare", "pattern": [0,0,0,0, 0,0,0,0, 1,0,0,0] },
    { "note": 42, "name": "hihat", "pattern": [1,0,1,0, 1,0,1,0, 1,0,1,0] }
  ]
})",

    // Swing
    R"({
  "name": "Swing",
  "steps_per_bar": 16,
  "swing": 50,
  "tracks": [
    { "note": 36, "name": "kick",  "pattern": [1,0,0,0, 0,0,1,0, 1,0,0,0, 0,0,0,0] },
    { "note": 38, "name": "snare", "pattern": [0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0] },
    { "note": 42, "name": "hihat", "pattern": [1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0] }
  ]
})",

    // Slow Rock
    R"({
  "name": "Slow Rock",
  "steps_per_bar": 16,
  "swing": 25,
  "tracks": [
    { "note": 36, "name": "kick",  "pattern": [1,0,0,0, 1,0,0,0, 0,0,0,0, 0,0,0,0] },
    { "note": 38, "name": "snare", "pattern": [0,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0] },
    { "note": 42, "name": "hihat", "pattern": [1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0] }
  ]
})",

    // Bossa Nova
    R"({
  "name": "Bossa Nova",
  "steps_per_bar": 16,
  "swing": 0,
  "tracks": [
    { "note": 36, "name": "kick",    "pattern": [1,0,0,0, 0,0,1,0, 0,0,0,0, 1,0,0,0] },
    { "note": 37, "name": "rimshot", "pattern": [0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0] },
    { "note": 51, "name": "ride",    "pattern": [1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0] },
    { "note": 82, "name": "shaker",  "pattern": [0,1,0,1, 0,1,0,1, 0,1,0,1, 0,1,0,1] }
  ]
})",

    // Rhumba
    R"({
  "name": "Rhumba",
  "steps_per_bar": 16,
  "swing": 0,
  "tracks": [
    { "note": 36, "name": "kick",     "pattern": [1,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0] },
    { "note": 62, "name": "conga_lo", "pattern": [0,0,1,0, 0,1,0,0, 0,0,1,0, 0,1,0,0] },
    { "note": 63, "name": "conga_hi", "pattern": [0,1,0,0, 0,0,0,1, 0,1,0,0, 0,0,0,1] },
    { "note": 75, "name": "clave",    "pattern": [1,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0] }
  ]
})",

    // Tango
    R"({
  "name": "Tango",
  "steps_per_bar": 16,
  "swing": 0,
  "tracks": [
    { "note": 36, "name": "kick",  "pattern": [1,0,0,0, 1,0,0,0, 1,0,0,0, 0,0,0,0] },
    { "note": 38, "name": "snare", "pattern": [0,0,0,0, 0,0,0,0, 0,0,0,0, 1,0,0,0] },
    { "note": 42, "name": "hihat", "pattern": [1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0] },
    { "note": 39, "name": "clap",  "pattern": [0,0,0,0, 0,0,0,1, 0,0,0,0, 0,0,0,1] }
  ]
})",

    // March
    R"({
  "name": "March",
  "steps_per_bar": 16,
  "swing": 0,
  "tracks": [
    { "note": 36, "name": "kick",  "pattern": [1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,0] },
    { "note": 38, "name": "snare", "pattern": [0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0] },
    { "note": 42, "name": "hihat", "pattern": [1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0] },
    { "note": 49, "name": "crash", "pattern": [1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0] }
  ]
})",

    // Samba
    R"({
  "name": "Samba",
  "steps_per_bar": 16,
  "swing": 0,
  "tracks": [
    { "note": 36, "name": "kick",   "pattern": [1,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0] },
    { "note": 38, "name": "snare",  "pattern": [0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0] },
    { "note": 82, "name": "shaker", "pattern": [1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0] },
    { "note": 61, "name": "bongo",  "pattern": [0,0,0,1, 0,0,0,1, 0,0,0,1, 0,0,0,1] }
  ]
})",

    // Disco
    R"({
  "name": "Disco",
  "steps_per_bar": 16,
  "swing": 0,
  "tracks": [
    { "note": 36, "name": "kick",     "pattern": [1,0,1,0, 1,0,0,0, 1,0,1,0, 1,0,0,0] },
    { "note": 38, "name": "snare",    "pattern": [0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0] },
    { "note": 42, "name": "hihat",    "pattern": [0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0] },
    { "note": 46, "name": "open_hat", "pattern": [0,0,1,0, 0,0,1,0, 0,0,1,0, 0,0,1,0] }
  ]
})",

    // Foxtrot
    R"({
  "name": "Foxtrot",
  "steps_per_bar": 16,
  "swing": 25,
  "tracks": [
    { "note": 36, "name": "kick",  "pattern": [1,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0] },
    { "note": 38, "name": "snare", "pattern": [0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0] },
    { "note": 42, "name": "hihat", "pattern": [1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0] },
    { "note": 51, "name": "ride",  "pattern": [0,0,0,0, 0,0,1,0, 0,0,0,0, 0,0,1,0] }
  ]
})",
};

// Shipped bass patterns (spec 6.8/6.9), embedded in code (pretty-printed) so the
// filesystem can be self-provisioned on first boot. `steps` are chord-degree
// codes (-1 = rest, 0 = root, 1 = 3rd, 2 = 5th, 3 = 6th/7th) on the 16th-note
// grid; `sustain_steps` (optional) gives a note's length in steps.
const char* const kBassJson[BASS_COUNT] = {
    // Walking: root-3rd-5th-6th/7th on beats 1-4.
    R"({
  "name": "Walking",
  "steps_per_bar": 16,
  "steps": [0,-1,-1,-1, 1,-1,-1,-1, 2,-1,-1,-1, 3,-1,-1,-1]
})",

    // Whole: root, whole note.
    R"({
  "name": "Whole",
  "steps_per_bar": 16,
  "steps": [0,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1],
  "sustain_steps": [16,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0]
})",

    // Half: root, half notes.
    R"({
  "name": "Half",
  "steps_per_bar": 16,
  "steps": [0,-1,-1,-1, -1,-1,-1,-1, 0,-1,-1,-1, -1,-1,-1,-1],
  "sustain_steps": [8,0,0,0, 0,0,0,0, 8,0,0,0, 0,0,0,0]
})",

    // Quarter: root, quarter notes.
    R"({
  "name": "Quarter",
  "steps_per_bar": 16,
  "steps": [0,-1,-1,-1, 0,-1,-1,-1, 0,-1,-1,-1, 0,-1,-1,-1]
})",

    // Half Alt: root/5th alternating half notes.
    R"({
  "name": "Half Alt",
  "steps_per_bar": 16,
  "steps": [0,-1,-1,-1, -1,-1,-1,-1, 2,-1,-1,-1, -1,-1,-1,-1],
  "sustain_steps": [8,0,0,0, 0,0,0,0, 8,0,0,0, 0,0,0,0]
})",

    // Quarter Alt: root/5th alternating quarter notes.
    R"({
  "name": "Quarter Alt",
  "steps_per_bar": 16,
  "steps": [0,-1,-1,-1, 2,-1,-1,-1, 0,-1,-1,-1, 2,-1,-1,-1]
})",

    // 3/4 Alt: root on beat 1, 5th on the last beat of the bar.
    R"({
  "name": "3/4 Alt",
  "steps_per_bar": 16,
  "steps": [0,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, 2,-1,-1,-1]
})",

    // Hold: root sustains while the chord is sounding (not beat-driven).
    R"({
  "name": "Hold",
  "steps_per_bar": 16,
  "hold": true,
  "steps": [0]
})",

    // No 6th: root (half) -> 3rd (quarter) -> 5th (quarter) -> repeat.
    R"({
  "name": "No 6th",
  "steps_per_bar": 16,
  "steps": [0,-1,-1,-1, -1,-1,-1,-1, 1,-1,-1,-1, 2,-1,-1,-1],
  "sustain_steps": [8,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0]
})",
};

// Shipped chord patterns (spec 6.10), embedded in code (pretty-printed). `steps`
// are chord-degree bitmasks (bit0=root, bit1=3rd, bit2=5th, bit3=7th; 0=rest) on
// the 16th-note grid; `sustain_steps` (optional) gives a note's length in steps.
const char* const kChordPatternJson[CHORD_PATTERN_COUNT] = {
    // Block: hold the full chord (all four degrees) for the whole bar.
    R"({
  "name": "Block",
  "steps_per_bar": 16,
  "steps": [15,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0],
  "sustain_steps": [16,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0]
})",

    // Alberti: root-5th-3rd-5th on quarter-note beats (the M13 demo pattern).
    R"({
  "name": "Alberti",
  "steps_per_bar": 16,
  "steps": [1,0,0,0, 4,0,0,0, 2,0,0,0, 4,0,0,0]
})",

    // Broken Up: root-3rd-5th-7th on quarter-note beats.
    R"({
  "name": "Broken Up",
  "steps_per_bar": 16,
  "steps": [1,0,0,0, 2,0,0,0, 4,0,0,0, 8,0,0,0]
})",

    // Broken Down: 7th-5th-3rd-root on quarter-note beats.
    R"({
  "name": "Broken Down",
  "steps_per_bar": 16,
  "steps": [8,0,0,0, 4,0,0,0, 2,0,0,0, 1,0,0,0]
})",

    // Pulse: full triad together on every 8th note.
    R"({
  "name": "Pulse",
  "steps_per_bar": 16,
  "steps": [7,0,7,0, 7,0,7,0, 7,0,7,0, 7,0,7,0]
})",

    // Oom-Pah: root alone, then 3rd+5th together, alternating beats.
    R"({
  "name": "Oom-Pah",
  "steps_per_bar": 16,
  "steps": [1,0,0,0, 6,0,0,0, 1,0,0,0, 6,0,0,0]
})",

    // Roll: root-3rd-5th-3rd on every 8th note (rolling arpeggio).
    R"({
  "name": "Roll",
  "steps_per_bar": 16,
  "steps": [1,0,2,0, 4,0,2,0, 1,0,2,0, 4,0,2,0]
})",

    // Split: root on beat 1, then 3rd+5th+7th together on beats 2-4.
    R"({
  "name": "Split",
  "steps_per_bar": 16,
  "steps": [1,0,0,0, 14,0,0,0, 14,0,0,0, 14,0,0,0]
})",

    // Arpeggio: root-3rd-5th-7th-5th-3rd across each half-bar (16th notes).
    R"({
  "name": "Arpeggio",
  "steps_per_bar": 16,
  "steps": [1,2,4,8, 4,2,0,0, 1,2,4,8, 4,2,0,0]
})",
};

} // namespace


void provisionDefaults(StorageAdapter& storage) {
    storage.mkdir("/presets");
    storage.mkdir("/rhythms");
    storage.mkdir("/bass");
    storage.mkdir("/chordpatterns");
    // Global config (defaults).
    AppConfig::defaults().save(storage);

    // Default keymap (configurable main-menu shortcuts).
    storage.writeFile("/keymap.json", keymapDefaultJson());

    // 10 banks x 8 default preset slots.
    for (int bank = 0; bank < NUM_BANKS; bank++) {
        for (int slot = 0; slot < NUM_SLOTS; slot++) {
            savePreset(storage, bank, slot, PresetSlot::defaults());
        }
    }

    // 12 named rhythm patterns.
    for (int i = 0; i < RHYTHM_COUNT; i++) {
        storage.writeFile("/rhythms/" + std::string(rhythmFileName(i)),
                          kRhythmJson[i]);
    }

    // 9 named bass patterns.
    for (int i = 0; i < BASS_COUNT; i++) {
        storage.writeFile("/bass/" + std::string(bassFileName(i)),
                          kBassJson[i]);
    }

    // 9 named chord patterns.
    for (int i = 0; i < CHORD_PATTERN_COUNT; i++) {
        storage.writeFile("/chordpatterns/" + std::string(chordPatternFileName(i)),
                          kChordPatternJson[i]);
    }
}

const char* defaultRhythmJson(int index) {
    if (index < 0 || index >= RHYTHM_COUNT) return nullptr;
    return kRhythmJson[index];
}

const char* defaultBassJson(int index) {
    if (index < 0 || index >= BASS_COUNT) return nullptr;
    return kBassJson[index];
}

const char* defaultChordPatternJson(int index) {
    if (index < 0 || index >= CHORD_PATTERN_COUNT) return nullptr;
    return kChordPatternJson[index];
}
