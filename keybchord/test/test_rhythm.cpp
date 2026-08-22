#include <gtest/gtest.h>

#include "rhythm.h"


TEST(Rhythm, ParsePattern) {
    const char* json = R"({
      "name": "Rock 1",
      "steps_per_bar": 16,
      "swing": 0,
      "tracks": [
        { "note": 36, "name": "kick",  "pattern": [1,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0] },
        { "note": 38, "name": "snare", "pattern": [0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0] },
        { "note": 42, "name": "hihat", "pattern": [1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0] }
      ]
    })";

    RhythmPattern p;
    ASSERT_TRUE(parseRhythmPattern(json, p));
    EXPECT_EQ(p.name, "Rock 1");
    EXPECT_EQ(p.steps_per_bar, 16);
    EXPECT_EQ(p.swing, 0);
    ASSERT_EQ(p.tracks.size(), 3u);
    EXPECT_EQ(p.tracks[0].note, 36);
    EXPECT_EQ(p.tracks[0].name, "kick");
    EXPECT_EQ(p.tracks[0].pattern.size(), 16u);
    EXPECT_EQ(p.beatsPerBar(), 4);
}

TEST(Rhythm, ParsePatternDefaultsSwingAndClamps) {
    const char* json = R"({
      "name": "Swing",
      "steps_per_bar": 16,
      "swing": 90,
      "tracks": [
        { "note": 36, "name": "kick", "pattern": [1,0,0,0] }
      ]
    })";
    RhythmPattern p;
    ASSERT_TRUE(parseRhythmPattern(json, p));
    EXPECT_EQ(p.swing, 75);  // clamped to the -75..75 max
    EXPECT_EQ(p.tracks[0].pattern.size(), 4u);
}

TEST(Rhythm, ParsePatternClampsNegativeSwing) {
    const char* json = R"({
      "name": "Rushed",
      "steps_per_bar": 16,
      "swing": -90,
      "tracks": [ { "note": 36, "name": "kick", "pattern": [1,0,0,0] } ]
    })";
    RhythmPattern p;
    ASSERT_TRUE(parseRhythmPattern(json, p));
    EXPECT_EQ(p.swing, -75);  // clamped to the -75..75 min
}

TEST(Rhythm, ParseRejectsMalformed) {
    RhythmPattern p;
    EXPECT_FALSE(parseRhythmPattern("not json", p));
    EXPECT_FALSE(parseRhythmPattern("[1,2,3]", p));  // not an object
}

TEST(Rhythm, StepEventsFireOnPatternHits) {
    RhythmPattern p;
    p.steps_per_bar = 16;
    p.tracks = {
        {36, "kick",  {1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}},
        {42, "hihat", {1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0}},
    };

    auto e0 = stepEvents(p, 0);
    ASSERT_EQ(e0.size(), 2u);
    EXPECT_EQ(e0[0].note, 36);
    EXPECT_EQ(e0[0].velocity, 100);  // "1" maps to default velocity
    EXPECT_EQ(e0[1].note, 42);

    auto e1 = stepEvents(p, 1);   // hihat rest -> nothing
    EXPECT_TRUE(e1.empty());

    auto e2 = stepEvents(p, 2);   // hihat only
    ASSERT_EQ(e2.size(), 1u);
    EXPECT_EQ(e2[0].note, 42);

    EXPECT_TRUE(stepEvents(p, 16).empty());  // out of range
}

TEST(Rhythm, StepEventsPreserveExplicitVelocity) {
    RhythmPattern p;
    p.steps_per_bar = 4;
    p.tracks = {{36, "kick", {64, 0, 0, 0}}};

    auto e = stepEvents(p, 0);
    ASSERT_EQ(e.size(), 1u);
    EXPECT_EQ(e[0].velocity, 64);  // explicit velocity passes through
}

TEST(Rhythm, StepUsIntegerMath) {
    // step_us = 60000000 / (bpm * 4) at 16th-note resolution.
    EXPECT_EQ(stepUs(120), 125000u);
    EXPECT_EQ(stepUs(60), 250000u);
    EXPECT_EQ(stepUs(40), 375000u);
    EXPECT_EQ(stepUs(260), 57692u);
}

TEST(Rhythm, StepUsGuardsZeroBpm) {
    EXPECT_EQ(stepUs(0), stepUs(120));
}

TEST(Rhythm, SwingFixedPointOnOffbeats) {
    // The off-beat 8th of each beat (step 2 within the beat) is delayed by
    // (base * swing) / 100; the downbeat, "e" and "a" 16ths are untouched.
    EXPECT_EQ(stepOffsetUs(0, 1000, 50), 0u);     // beat 1 downbeat, no swing
    EXPECT_EQ(stepOffsetUs(1, 1000, 50), 1000u);  // "e" 16th, no swing
    EXPECT_EQ(stepOffsetUs(2, 1000, 0), 2000u);   // straight off-beat
    EXPECT_EQ(stepOffsetUs(2, 1000, 50), 2500u);  // off-beat +50%
    EXPECT_EQ(stepOffsetUs(2, 1000, 75), 2750u);  // off-beat +75%
    EXPECT_EQ(stepOffsetUs(3, 1000, 50), 3000u);  // "a" 16th, no swing
    EXPECT_EQ(stepOffsetUs(4, 1000, 50), 4000u);  // next beat, no swing
    EXPECT_EQ(stepOffsetUs(6, 1000, 50), 6500u);  // next off-beat +50%
}

TEST(Rhythm, NegativeSwingRushesOffbeats) {
    // Negative swing pulls the off-beat 8th earlier (rushed feel).
    EXPECT_EQ(stepOffsetUs(2, 1000, -50), 1500u);  // 50% earlier
    EXPECT_EQ(stepOffsetUs(2, 1000, -75), 1250u);  // 75% earlier
    EXPECT_EQ(stepOffsetUs(0, 1000, -75), 0u);     // on-beat unaffected
    EXPECT_EQ(stepOffsetUs(4, 1000, -75), 4000u);  // on-beat unaffected
}

TEST(Rhythm, ClockTickUsAt24Ppqn) {
    // 24 PPQN / 4 steps per beat = 6 ticks per step.
    EXPECT_EQ(clockTickUs(120), 125000u / 6);   // 20833
    EXPECT_EQ(clockTickUs(120) * 6, 124998u);
}

TEST(Rhythm, NameIndexRoundTrip) {
    EXPECT_EQ(rhythmIndex("Rock 1"), 0);
    EXPECT_EQ(rhythmIndex("Foxtrot"), 11);
    EXPECT_EQ(rhythmIndex("Nope"), -1);
    EXPECT_STREQ(rhythmName(0), "Rock 1");
    EXPECT_STREQ(rhythmName(11), "Foxtrot");
    EXPECT_STREQ(rhythmName(-1), "Rock 1");  // clamp
}

TEST(Rhythm, FileNameMapping) {
    EXPECT_STREQ(rhythmFileName(0), "rock1.json");
    EXPECT_STREQ(rhythmFileName(11), "foxtrot.json");
}

TEST(Rhythm, DrumMapRemapsStandardCodes) {
    DrumMap m;
    m.kick = 35;      // Bass Drum 2
    m.snare = 40;     // Electric Snare
    m.hihat = 44;     // Pedal Hi-Hat
    m.open_hat = 45;  // Mid Tom (arbitrary) for the test

    EXPECT_EQ(mapDrumNote(36, m), 35);
    EXPECT_EQ(mapDrumNote(38, m), 40);
    EXPECT_EQ(mapDrumNote(42, m), 44);
    EXPECT_EQ(mapDrumNote(46, m), 45);
    // Non-kit notes pass through untouched.
    EXPECT_EQ(mapDrumNote(51, m), 51);
    EXPECT_EQ(mapDrumNote(82, m), 82);
}

TEST(Rhythm, DrumMapDefaultsAreStandardGm) {
    DrumMap d;
    EXPECT_EQ(d.kick, 36);
    EXPECT_EQ(d.snare, 38);
    EXPECT_EQ(d.hihat, 42);
    EXPECT_EQ(d.open_hat, 46);
    // With default mapping, notes are unchanged.
    EXPECT_EQ(mapDrumNote(38, d), 38);
}

// All instruments used across the shipped rhythms are remappable (FR-R9).
TEST(Rhythm, DrumMapRemapsExtendedInstruments) {
    DrumMap m;
    m.rimshot = 40;
    m.clap = 41;
    m.crash = 50;
    m.ride = 52;
    m.bongo = 60;
    m.conga_lo = 65;
    m.conga_hi = 66;
    m.clave = 76;
    m.shaker = 83;

    EXPECT_EQ(mapDrumNote(37, m), 40);  // rimshot
    EXPECT_EQ(mapDrumNote(39, m), 41);  // clap
    EXPECT_EQ(mapDrumNote(49, m), 50);  // crash
    EXPECT_EQ(mapDrumNote(51, m), 52);  // ride
    EXPECT_EQ(mapDrumNote(61, m), 60);  // bongo
    EXPECT_EQ(mapDrumNote(62, m), 65);  // conga_lo
    EXPECT_EQ(mapDrumNote(63, m), 66);  // conga_hi
    EXPECT_EQ(mapDrumNote(75, m), 76);  // clave
    EXPECT_EQ(mapDrumNote(82, m), 83);  // shaker
}

TEST(Rhythm, DrumMapExtendedDefaultsAreIdentity) {
    DrumMap d;
    EXPECT_EQ(mapDrumNote(37, d), 37);
    EXPECT_EQ(mapDrumNote(39, d), 39);
    EXPECT_EQ(mapDrumNote(49, d), 49);
    EXPECT_EQ(mapDrumNote(51, d), 51);
    EXPECT_EQ(mapDrumNote(61, d), 61);
    EXPECT_EQ(mapDrumNote(62, d), 62);
    EXPECT_EQ(mapDrumNote(63, d), 63);
    EXPECT_EQ(mapDrumNote(75, d), 75);
    EXPECT_EQ(mapDrumNote(82, d), 82);
}

TEST(Rhythm, DrumVelocityExtended) {
    DrumMap m;
    m.conga_lo_vel = 90;
    m.shaker_vel = 0;
    EXPECT_EQ(mapDrumVelocity(62, m, 100), 90);  // override
    EXPECT_EQ(mapDrumVelocity(82, m, 100), 100); // no override -> pattern
}

TEST(Rhythm, DrumVelocityOffMutesPiece) {
    DrumMap m;
    m.kick_vel = 128;  // off
    EXPECT_EQ(mapDrumVelocity(36, m, 100), 0);   // muted -> velocity 0
    EXPECT_EQ(mapDrumVelocity(38, m, 100), 100); // other pieces unaffected
}
