#include <gtest/gtest.h>
#include "presets.h"
#include "storage_stub.h"
#include "params.h"


TEST(Presets, DefaultPreset) {
    PresetSlot p = PresetSlot::defaults();
    EXPECT_EQ(p.name, "Default");
    EXPECT_EQ(p.chord.channel, 1);
    EXPECT_EQ(p.strum.channel, 2);
    EXPECT_EQ(p.rhythm.channel, 10);
    EXPECT_EQ(p.chord.play_mode, PlayMode::Held);
    EXPECT_EQ(p.rhythm.tempo, 120);
}

TEST(Presets, DefaultEquality) {
    PresetSlot a = PresetSlot::defaults();
    PresetSlot b = PresetSlot::defaults();
    EXPECT_EQ(a, b);
}

TEST(Presets, Inequality) {
    PresetSlot a = PresetSlot::defaults();
    PresetSlot b = PresetSlot::defaults();
    b.chord.channel = 16;
    EXPECT_NE(a, b);
}

TEST(Presets, LoadMissingPreset) {
    StorageStub storage;
    PresetSlot p = loadPreset(storage, 0, 0);
    EXPECT_EQ(p.name, "Default");
    EXPECT_EQ(p.chord.channel, 1);
}

TEST(Presets, SaveAndLoadRoundTrip) {
    StorageStub storage;

    PresetSlot orig = PresetSlot::defaults();
    orig.name = "TestPreset";
    orig.chord.channel = 5;
    orig.chord.velocity = 110;
    orig.strum.channel = 3;
    orig.strum.limited_keys = true;
    orig.rhythm.tempo = 140;
    orig.rhythm.swing = 50;

    EXPECT_TRUE(savePreset(storage, 0, 3, orig));

    PresetSlot loaded = loadPreset(storage, 0, 3);
    EXPECT_EQ(loaded.name, "TestPreset");
    EXPECT_EQ(loaded.chord.channel, 5);
    EXPECT_EQ(loaded.chord.velocity, 110);
    EXPECT_EQ(loaded.strum.channel, 3);
    EXPECT_TRUE(loaded.strum.limited_keys);
    EXPECT_EQ(loaded.rhythm.tempo, 140);
    EXPECT_EQ(loaded.rhythm.swing, 50);
}

TEST(Presets, InvalidBankSlotReturnsDefault) {
    StorageStub storage;
    PresetSlot p = loadPreset(storage, -1, 0);
    EXPECT_EQ(p, PresetSlot::defaults());

    p = loadPreset(storage, NUM_BANKS, 0);
    EXPECT_EQ(p, PresetSlot::defaults());

    p = loadPreset(storage, 0, -1);
    EXPECT_EQ(p, PresetSlot::defaults());

    p = loadPreset(storage, 0, NUM_SLOTS);
    EXPECT_EQ(p, PresetSlot::defaults());
}

TEST(Presets, LoadPresetOrDefault) {
    StorageStub storage;
    PresetSlot p = loadPresetOrDefault(storage, 0, 0);
    EXPECT_EQ(p, PresetSlot::defaults());
}
