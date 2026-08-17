#include <gtest/gtest.h>

#include "defaults.h"
#include "storage_stub.h"
#include "config.h"
#include "presets.h"
#include "rhythm.h"


TEST(Defaults, ProvisionsConfigPresetsAndRhythms) {
    StorageStub storage;
    provisionDefaults(storage);

    // Config written and loadable.
    EXPECT_TRUE(storage.exists("/config.json"));

    // 10 banks x 8 slots of presets written (each bank is one JSON file).
    for (int bank = 0; bank < NUM_BANKS; bank++) {
        char path[32];
        snprintf(path, sizeof(path), "/presets/bank%d.json", bank + 1);
        EXPECT_TRUE(storage.exists(path));
        for (int slot = 0; slot < NUM_SLOTS; slot++) {
            PresetSlot p = loadPreset(storage, bank, slot);
            EXPECT_EQ(p.chord.channel, DEFAULT_CHORD_CHANNEL);
            EXPECT_EQ(p.strum.channel, DEFAULT_STRUM_CHANNEL);
            EXPECT_EQ(p.rhythm.channel, DEFAULT_RHYTHM_CHANNEL);
        }
    }

    // All 12 rhythms written and parseable.
    for (int i = 0; i < RHYTHM_COUNT; i++) {
        std::string path = "/rhythms/" + std::string(rhythmFileName(i));
        EXPECT_TRUE(storage.exists(path)) << path;
        RhythmPattern p;
        EXPECT_TRUE(parseRhythmPattern(storage.readFile(path), p)) << path;
        EXPECT_FALSE(p.tracks.empty()) << path;
        EXPECT_EQ(p.name, rhythmName(i)) << path;
    }
}
