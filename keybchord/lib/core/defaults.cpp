#include "defaults.h"

#include "base.h"
#include "config.h"
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

} // namespace


void provisionDefaults(StorageAdapter& storage) {
    storage.mkdir("/presets");
    storage.mkdir("/rhythms");

    // Global config (defaults).
    AppConfig::defaults().save(storage);

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
}
