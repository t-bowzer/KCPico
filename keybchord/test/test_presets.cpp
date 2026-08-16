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

TEST(Presets, SameParamsIgnoresName) {
    PresetSlot a = PresetSlot::defaults();
    PresetSlot b = PresetSlot::defaults();
    b.name = "OtherName";
    EXPECT_NE(a, b);            // operator== includes name
    EXPECT_TRUE(a.sameParams(b)); // sameParams does not

    b.chord.channel = 16;
    EXPECT_FALSE(a.sameParams(b));
}

TEST(Presets, MakePresetCopiesParams) {
    ChordParams c = ChordParams::defaults();
    StrumParams s = StrumParams::defaults();
    RhythmParams r = RhythmParams::defaults();
    c.channel = 5;
    s.channel = 7;
    r.channel = 12;

    PresetSlot p = makePreset(c, s, r, "Built");
    EXPECT_EQ(p.name, "Built");
    EXPECT_EQ(p.chord.channel, 5);
    EXPECT_EQ(p.strum.channel, 7);
    EXPECT_EQ(p.rhythm.channel, 12);
}

TEST(Presets, ParsePresetLocation) {
    int bank = -1, slot = -1;

    EXPECT_TRUE(parsePresetLocation("B1:P1", bank, slot));
    EXPECT_EQ(bank, 0);
    EXPECT_EQ(slot, 0);

    EXPECT_TRUE(parsePresetLocation("B10:P8", bank, slot));
    EXPECT_EQ(bank, 9);
    EXPECT_EQ(slot, 7);

    // Case-insensitive.
    EXPECT_TRUE(parsePresetLocation("b3:p5", bank, slot));
    EXPECT_EQ(bank, 2);
    EXPECT_EQ(slot, 4);
}

TEST(Presets, ParsePresetLocationInvalid) {
    int bank = 0, slot = 0;

    EXPECT_FALSE(parsePresetLocation("", bank, slot));
    EXPECT_FALSE(parsePresetLocation("B0:P1", bank, slot));   // bank 0 out of range
    EXPECT_FALSE(parsePresetLocation("B11:P1", bank, slot));  // bank 11 out of range
    EXPECT_FALSE(parsePresetLocation("B1:P0", bank, slot));   // slot 0 out of range
    EXPECT_FALSE(parsePresetLocation("B1:P9", bank, slot));   // slot 9 out of range
    EXPECT_FALSE(parsePresetLocation("B1P1", bank, slot));    // missing ':'
    EXPECT_FALSE(parsePresetLocation("1:2", bank, slot));     // missing B/P
    EXPECT_FALSE(parsePresetLocation("Bx:P1", bank, slot));   // non-numeric
}

TEST(Presets, LoadPresetOrDefault) {
    StorageStub storage;
    PresetSlot p = loadPresetOrDefault(storage, 0, 0);
    EXPECT_EQ(p, PresetSlot::defaults());
}

TEST(Presets, RhythmPatternNameRoundTrip) {
    StorageStub storage;

    PresetSlot orig = PresetSlot::defaults();
    orig.rhythm.pattern = 3;  // "Swing"
    EXPECT_TRUE(savePreset(storage, 1, 0, orig));

    PresetSlot loaded = loadPreset(storage, 1, 0);
    EXPECT_EQ(loaded.rhythm.pattern, 3);

    // An unknown pattern name falls back to the default (0).
    storage.writeFile("/presets/bank1.json",
        "[{\"rhythm\":{\"pattern\":\"Not A Rhythm\"}}]");
    PresetSlot fallback = loadPreset(storage, 0, 0);
    EXPECT_EQ(fallback.rhythm.pattern, 0);
}

TEST(Presets, RhythmDrumMapRoundTrip) {
    StorageStub storage;

    PresetSlot orig = PresetSlot::defaults();
    orig.rhythm.drums.kick = 35;
    orig.rhythm.drums.snare = 40;
    orig.rhythm.drums.hihat = 44;
    orig.rhythm.drums.open_hat = 45;
    EXPECT_TRUE(savePreset(storage, 2, 1, orig));

    PresetSlot loaded = loadPreset(storage, 2, 1);
    EXPECT_EQ(loaded.rhythm.drums.kick, 35);
    EXPECT_EQ(loaded.rhythm.drums.snare, 40);
    EXPECT_EQ(loaded.rhythm.drums.hihat, 44);
    EXPECT_EQ(loaded.rhythm.drums.open_hat, 45);
}
