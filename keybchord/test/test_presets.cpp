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
    BassParams b = BassParams::defaults();
    RhythmParams r = RhythmParams::defaults();
    c.channel = 5;
    s.channel = 7;
    b.channel = 9;
    r.channel = 12;

    PresetSlot p = makePreset(c, s, b, r, "Built");
    EXPECT_EQ(p.name, "Built");
    EXPECT_EQ(p.chord.channel, 5);
    EXPECT_EQ(p.strum.channel, 7);
    EXPECT_EQ(p.bass.channel, 9);
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

TEST(Presets, DrumVelocityRoundTrip) {
    StorageStub storage;

    PresetSlot orig = PresetSlot::defaults();
    orig.rhythm.drums.kick_vel = 100;
    orig.rhythm.drums.snare_vel = 90;
    EXPECT_TRUE(savePreset(storage, 2, 2, orig));

    PresetSlot loaded = loadPreset(storage, 2, 2);
    EXPECT_EQ(loaded.rhythm.drums.kick_vel, 100);
    EXPECT_EQ(loaded.rhythm.drums.snare_vel, 90);
}

TEST(Presets, BassBlockRoundTrip) {
    StorageStub storage;

    PresetSlot orig = PresetSlot::defaults();
    orig.bass.enabled = true;
    orig.bass.octave = -2;
    orig.bass.note_duration_ms = 200;
    orig.bass.velocity = 110;
    orig.bass.channel = 5;
    orig.bass.pattern = BassPattern::QuarterAlt;
    EXPECT_TRUE(savePreset(storage, 3, 0, orig));

    PresetSlot loaded = loadPreset(storage, 3, 0);
    EXPECT_TRUE(loaded.bass.enabled);
    EXPECT_EQ(loaded.bass.octave, -2);
    EXPECT_EQ(loaded.bass.note_duration_ms, 200);
    EXPECT_EQ(loaded.bass.velocity, 110);
    EXPECT_EQ(loaded.bass.channel, 5);
    EXPECT_EQ(loaded.bass.pattern, BassPattern::QuarterAlt);
}

TEST(Presets, BassPatternNameFallback) {
    StorageStub storage;
    storage.writeFile("/presets/bank1.json",
        "[{\"bass\":{\"pattern\":\"not_a_pattern\"}}]");
    PresetSlot p = loadPreset(storage, 0, 0);
    EXPECT_EQ(p.bass.pattern, BassPattern::Walking);
}

TEST(Presets, WalkNoSixthPatternRoundTrip) {
    StorageStub storage;
    storage.writeFile("/presets/bank1.json",
        "[{\"bass\":{\"pattern\":\"walk_no_6th\"}}]");
    PresetSlot p = loadPreset(storage, 0, 0);
    EXPECT_EQ(p.bass.pattern, BassPattern::WalkNoSixth);
}

TEST(Presets, ExtendedDrumMapRoundTrip) {
    StorageStub storage;

    PresetSlot orig = PresetSlot::defaults();
    orig.rhythm.drums.rimshot = 40;
    orig.rhythm.drums.clap = 41;
    orig.rhythm.drums.crash = 50;
    orig.rhythm.drums.ride = 52;
    orig.rhythm.drums.bongo = 60;
    orig.rhythm.drums.conga_lo = 65;
    orig.rhythm.drums.conga_hi = 66;
    orig.rhythm.drums.clave = 76;
    orig.rhythm.drums.shaker = 83;
    orig.rhythm.drums.shaker_vel = 100;
    EXPECT_TRUE(savePreset(storage, 2, 3, orig));

    PresetSlot loaded = loadPreset(storage, 2, 3);
    EXPECT_EQ(loaded.rhythm.drums.rimshot, 40);
    EXPECT_EQ(loaded.rhythm.drums.clap, 41);
    EXPECT_EQ(loaded.rhythm.drums.crash, 50);
    EXPECT_EQ(loaded.rhythm.drums.ride, 52);
    EXPECT_EQ(loaded.rhythm.drums.bongo, 60);
    EXPECT_EQ(loaded.rhythm.drums.conga_lo, 65);
    EXPECT_EQ(loaded.rhythm.drums.conga_hi, 66);
    EXPECT_EQ(loaded.rhythm.drums.clave, 76);
    EXPECT_EQ(loaded.rhythm.drums.shaker, 83);
    EXPECT_EQ(loaded.rhythm.drums.shaker_vel, 100);
}

TEST(Presets, ChordUpgradesRoundTrip) {
    StorageStub storage;

    PresetSlot orig = PresetSlot::defaults();
    orig.chord.chord_roll_ms = -120;
    orig.chord.min_notes = 5;
    orig.chord.min_interval = 4;
    orig.chord.inversion = InversionMode::Second;
    orig.chord.arp_mode = ArpMode::Random;
    EXPECT_TRUE(savePreset(storage, 4, 0, orig));

    PresetSlot loaded = loadPreset(storage, 4, 0);
    EXPECT_EQ(loaded.chord.chord_roll_ms, -120);
    EXPECT_EQ(loaded.chord.min_notes, 5);
    EXPECT_EQ(loaded.chord.min_interval, 4);
    EXPECT_EQ(loaded.chord.inversion, InversionMode::Second);
    EXPECT_EQ(loaded.chord.arp_mode, ArpMode::Random);
}

TEST(Presets, StrumModeRoundTrip) {
    StorageStub storage;

    PresetSlot orig = PresetSlot::defaults();
    orig.strum.mode = StrumMode::Scale;
    orig.strum.root_pc = 7;
    orig.strum.scale_type = ScaleType::Blues;
    EXPECT_TRUE(savePreset(storage, 4, 1, orig));

    PresetSlot loaded = loadPreset(storage, 4, 1);
    EXPECT_EQ(loaded.strum.mode, StrumMode::Scale);
    EXPECT_EQ(loaded.strum.root_pc, 7);
    EXPECT_EQ(loaded.strum.scale_type, ScaleType::Blues);
}

TEST(Presets, LegacyPlayModeRhythmRemappedToArpeggio) {
    StorageStub storage;

    // Old int play_mode: 3 = Rhythm (now removed) -> Arpeggio.
    storage.writeFile("/presets/bank1.json",
        "[{\"chord\":{\"play_mode\":3}}]");
    PresetSlot p = loadPreset(storage, 0, 0);
    EXPECT_EQ(p.chord.play_mode, PlayMode::Arpeggio);

    // Old 4 = Silent -> Silent.
    storage.writeFile("/presets/bank1.json",
        "[{\"chord\":{\"play_mode\":4}}]");
    p = loadPreset(storage, 0, 0);
    EXPECT_EQ(p.chord.play_mode, PlayMode::Silent);
}
