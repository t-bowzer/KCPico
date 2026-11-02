#include "chord_engine_fixture.h"


// Arpeggio mode sequencing (AC-28): the voicing steps in the specified order.
// Arp is advanced on the rhythm clock's step edges (FR-R4).

static uint8_t soundingNote(const StateManager& s, uint8_t channel) {
    for (const auto& an : s.activeNotes) {
        if (an.channel == channel) return an.note;
    }
    return 0xFF;
}

// Trigger C major {60,64,67} in arp mode; returns the engine for stepping.
static void setupArp(ChordEngine& e, StateManager& s, ArpMode mode) {
    s.pendingChord.play_mode = PlayMode::Arpeggio;
    s.pendingChord.arp_mode  = mode;
    s.pendingRhythm.enabled  = true;
    s.rhythmClock.running    = true;
    e.handleKeyEvent({0x17, true, 0}, 0);   // T = C major
    e.update(ChordEngine::PRESS_GROUP_US);
}

TEST_F(ChordEngineTest, ArpModeUp) {
    setupArp(*engine_, state_, ArpMode::Up);
    EXPECT_EQ(soundingNote(state_, 1), 60);

    state_.rhythmClock.stepAbs = 1; state_.rhythmClock.tickAbs = static_cast<uint64_t>(1) * 6; engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 64);
    state_.rhythmClock.stepAbs = 2; state_.rhythmClock.tickAbs = static_cast<uint64_t>(2) * 6; engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 67);
    state_.rhythmClock.stepAbs = 3; state_.rhythmClock.tickAbs = static_cast<uint64_t>(3) * 6; engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 60);  // wraps
}

TEST_F(ChordEngineTest, ArpModeDown) {
    setupArp(*engine_, state_, ArpMode::Down);
    EXPECT_EQ(soundingNote(state_, 1), 67);

    state_.rhythmClock.stepAbs = 1; state_.rhythmClock.tickAbs = static_cast<uint64_t>(1) * 6; engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 64);
    state_.rhythmClock.stepAbs = 2; state_.rhythmClock.tickAbs = static_cast<uint64_t>(2) * 6; engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 60);
    state_.rhythmClock.stepAbs = 3; state_.rhythmClock.tickAbs = static_cast<uint64_t>(3) * 6; engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 67);  // wraps
}

TEST_F(ChordEngineTest, ArpModeUpDown) {
    setupArp(*engine_, state_, ArpMode::UpDown);
    EXPECT_EQ(soundingNote(state_, 1), 60);

    state_.rhythmClock.stepAbs = 1; state_.rhythmClock.tickAbs = static_cast<uint64_t>(1) * 6; engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 64);
    state_.rhythmClock.stepAbs = 2; state_.rhythmClock.tickAbs = static_cast<uint64_t>(2) * 6; engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 67);
    state_.rhythmClock.stepAbs = 3; state_.rhythmClock.tickAbs = static_cast<uint64_t>(3) * 6; engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 64);  // down
    state_.rhythmClock.stepAbs = 4; state_.rhythmClock.tickAbs = static_cast<uint64_t>(4) * 6; engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 60);  // back to root
}

// Up-Down bounces at both endpoints without repeating them at the wrap:
// C E G E | C E G E ... (not C E G E C C E ...).
TEST_F(ChordEngineTest, ArpModeUpDownDoesNotRepeatRootAtWrap) {
    setupArp(*engine_, state_, ArpMode::UpDown);
    const uint8_t expected[] = {60, 64, 67, 64, 60, 64, 67, 64, 60};
    for (uint32_t step = 1; step <= 8; step++) {
        state_.rhythmClock.tickAbs = static_cast<uint64_t>(step) * 6;
        engine_->update(0);
        EXPECT_EQ(soundingNote(state_, 1), expected[step]);
    }
}

TEST_F(ChordEngineTest, ArpModeAlternating) {
    setupArp(*engine_, state_, ArpMode::Alternating);
    // Sequence [0,2,1] for a 3-note chord: 60, 67, 64.
    EXPECT_EQ(soundingNote(state_, 1), 60);

    state_.rhythmClock.stepAbs = 1; state_.rhythmClock.tickAbs = static_cast<uint64_t>(1) * 6; engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 67);
    state_.rhythmClock.stepAbs = 2; state_.rhythmClock.tickAbs = static_cast<uint64_t>(2) * 6; engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 64);
    state_.rhythmClock.stepAbs = 3; state_.rhythmClock.tickAbs = static_cast<uint64_t>(3) * 6; engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 60);  // wraps
}

TEST_F(ChordEngineTest, ArpModeRandomPlaysChordNote) {
    setupArp(*engine_, state_, ArpMode::Random);
    for (int i = 0; i < 8; i++) {
        state_.rhythmClock.stepAbs = static_cast<uint32_t>(i + 1); state_.rhythmClock.tickAbs = static_cast<uint64_t>(static_cast<uint32_t>(i + 1)) * 6;
        engine_->update(0);
        uint8_t n = soundingNote(state_, 1);
        EXPECT_TRUE(n == 60 || n == 64 || n == 67);  // always a chord note
    }
}

// Arp-hold latches the arpeggio on key release: it keeps stepping until the
// next chord is pressed or all-sound-off (Esc) fires.
TEST_F(ChordEngineTest, ArpHoldKeepsArpingAfterRelease) {
    state_.pendingChord.play_mode = PlayMode::ArpHold;
    state_.pendingRhythm.enabled  = true;
    state_.rhythmClock.running    = true;
    press(0x17, 0, 0);   // T = C major
    EXPECT_EQ(soundingNote(state_, 1), 60);

    release(0x17, 0, 0);  // release: latches (no stop)
    EXPECT_EQ(midi_.noteOffCount(), 0);

    state_.rhythmClock.stepAbs = 1; state_.rhythmClock.tickAbs = static_cast<uint64_t>(1) * 6; engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 64);
    state_.rhythmClock.stepAbs = 2; state_.rhythmClock.tickAbs = static_cast<uint64_t>(2) * 6; engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 67);
}

TEST_F(ChordEngineTest, ArpHoldReplacedByNextChord) {
    state_.pendingChord.play_mode = PlayMode::ArpHold;
    state_.pendingRhythm.enabled  = true;
    state_.rhythmClock.running    = true;
    press(0x17, 0, 0);   // C major arp-hold
    release(0x17, 0, 0);  // release, latched
    EXPECT_EQ(soundingNote(state_, 1), 60);

    press(0x1C, 0, 0);   // Y = G major replaces C
    EXPECT_FALSE(state_.isNoteActive(1, 60));
    EXPECT_TRUE(state_.isNoteActive(1, 67));       // G major root
}

TEST_F(ChordEngineTest, ArpHoldStopsOnAllNotesOff) {
    state_.pendingChord.play_mode = PlayMode::ArpHold;
    state_.pendingRhythm.enabled  = true;
    state_.rhythmClock.running    = true;
    press(0x17, 0, 0);   // C major arp-hold
    release(0x17, 0, 0);  // release, latched
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    engine_->allNotesOff();                        // Esc path
    EXPECT_FALSE(state_.isNoteActive(1, 60));
    EXPECT_FALSE(engine_->isSounding());
}

// Arp rate selects the master-clock subdivision (24 PPQN): quarter = 24 ticks,
// eighth = 12, 32nd = 3.
TEST_F(ChordEngineTest, ArpRateQuarterStepsEvery24Ticks) {
    state_.pendingChord.arp_rate = ArpRate::Quarter;
    setupArp(*engine_, state_, ArpMode::Up);
    EXPECT_EQ(soundingNote(state_, 1), 60);

    state_.rhythmClock.tickAbs = 23;
    engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 60);

    state_.rhythmClock.tickAbs = 24;
    engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 64);

    state_.rhythmClock.tickAbs = 48;
    engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 67);
}

TEST_F(ChordEngineTest, ArpRateEighthStepsEvery12Ticks) {
    state_.pendingChord.arp_rate = ArpRate::Eighth;
    setupArp(*engine_, state_, ArpMode::Up);

    state_.rhythmClock.tickAbs = 11;
    engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 60);

    state_.rhythmClock.tickAbs = 12;
    engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 64);
}

TEST_F(ChordEngineTest, ArpRate32ndStepsEvery3Ticks) {
    state_.pendingChord.arp_rate = ArpRate::ThirtySecond;
    setupArp(*engine_, state_, ArpMode::Up);

    state_.rhythmClock.tickAbs = 2;
    engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 60);

    state_.rhythmClock.tickAbs = 3;
    engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 64);

    state_.rhythmClock.tickAbs = 6;
    engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 67);
}

TEST_F(ChordEngineTest, ArpRateWholeWaitsFourBeats) {
    state_.pendingChord.arp_rate = ArpRate::Whole;
    setupArp(*engine_, state_, ArpMode::Up);

    state_.rhythmClock.tickAbs = 95;
    engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 60);

    state_.rhythmClock.tickAbs = 96;
    engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 64);
}

TEST_F(ChordEngineTest, ArpRateAppliesWhenRhythmStopped) {
    state_.pendingChord.play_mode = PlayMode::Arpeggio;
    state_.pendingChord.arp_rate = ArpRate::Half;
    state_.pendingRhythm.enabled = false;
    state_.rhythmClock.running = false;

    press(0x17, 0, 0);
    EXPECT_EQ(soundingNote(state_, 1), 60);

    state_.rhythmClock.tickAbs = 47;   // half note = 48 master-clock ticks
    engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 60);
    state_.rhythmClock.tickAbs = 48;
    engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 64);
}


// A chord started just before the next arp boundary waits for that boundary
// instead of playing immediately and then again a few milliseconds later.
TEST_F(ChordEngineTest, ArpEarlyStartQuantizesFirstNoteToBoundary) {
    state_.pendingChord.play_mode = PlayMode::Arpeggio;
    state_.pendingChord.arp_rate = ArpRate::Sixteenth;
    state_.rhythmClock.tickAbs = 5;  // one tick before the 6-tick boundary
    state_.rhythmClock.nextTickUs = 1020000;  // boundary 5 ms after trigger

    engine_->handleKeyEvent({0x17, true, 0}, 1000000);
    engine_->update(1015000);  // press group fires; first note is deferred
    EXPECT_EQ(midi_.noteOnCount(), 0);
    EXPECT_FALSE(state_.isNoteActive(1, 60));

    engine_->update(1019999);
    EXPECT_EQ(midi_.noteOnCount(), 0);

    engine_->update(1020000);  // boundary -> first note
    EXPECT_EQ(midi_.noteOnCount(), 1);
    EXPECT_EQ(soundingNote(state_, 1), 60);

    state_.rhythmClock.tickAbs = 6;  // boundary tick published: no extra note
    engine_->update(1020000);
    EXPECT_EQ(midi_.noteOnCount(), 1);

    state_.rhythmClock.tickAbs = 12;  // full arp interval later -> second note
    engine_->update(1145000);
    EXPECT_EQ(midi_.noteOnCount(), 2);
    EXPECT_EQ(soundingNote(state_, 1), 64);
}

TEST_F(ChordEngineTest, ArpStartOutsideWindowPlaysImmediately) {
    state_.pendingChord.play_mode = PlayMode::Arpeggio;
    state_.pendingChord.arp_rate = ArpRate::Sixteenth;
    state_.rhythmClock.tickAbs = 5;
    state_.rhythmClock.nextTickUs = 1060000;  // boundary 45 ms after trigger

    engine_->handleKeyEvent({0x17, true, 0}, 1000000);
    engine_->update(1015000);
    EXPECT_EQ(midi_.noteOnCount(), 1);
    EXPECT_EQ(soundingNote(state_, 1), 60);
}

TEST_F(ChordEngineTest, ArpEarlyStartQuantizesQuarterRateBoundary) {
    state_.pendingChord.play_mode = PlayMode::Arpeggio;
    state_.pendingChord.arp_rate = ArpRate::Quarter;
    state_.rhythmClock.tickAbs = 23;  // one tick before the 24-tick boundary
    state_.rhythmClock.nextTickUs = 1020000;

    engine_->handleKeyEvent({0x17, true, 0}, 1000000);
    engine_->update(1015000);
    EXPECT_EQ(midi_.noteOnCount(), 0);

    engine_->update(1020000);
    EXPECT_EQ(midi_.noteOnCount(), 1);
    EXPECT_EQ(soundingNote(state_, 1), 60);

    state_.rhythmClock.tickAbs = 24;
    engine_->update(1020000);
    EXPECT_EQ(midi_.noteOnCount(), 1);

    state_.rhythmClock.tickAbs = 48;
    engine_->update(1515000);  // one quarter-note later
    EXPECT_EQ(midi_.noteOnCount(), 2);
    EXPECT_EQ(soundingNote(state_, 1), 64);
}

TEST_F(ChordEngineTest, ArpChordChangeJustBeforeBoundaryIsQuantized) {
    state_.pendingChord.play_mode = PlayMode::Arpeggio;
    state_.pendingChord.arp_rate = ArpRate::Sixteenth;
    state_.rhythmClock.tickAbs = 0;

    engine_->handleKeyEvent({0x17, true, 0}, 0);  // C major starts immediately
    engine_->update(ChordEngine::PRESS_GROUP_US);
    ASSERT_EQ(midi_.noteOnCount(), 1);
    EXPECT_EQ(soundingNote(state_, 1), 60);

    engine_->handleKeyEvent({0x17, false, 0}, 900000);
    engine_->update(925000);

    // Change to G major 5 ms before the next arp boundary.
    state_.rhythmClock.tickAbs = 5;
    state_.rhythmClock.nextTickUs = 1020000;
    engine_->handleKeyEvent({0x1C, true, 0}, 1000000);
    engine_->update(1015000);
    EXPECT_EQ(midi_.noteOnCount(), 1);  // old C stopped, new G deferred
    EXPECT_FALSE(state_.isNoteActive(1, 60));

    engine_->update(1020000);
    EXPECT_EQ(midi_.noteOnCount(), 2);
    EXPECT_EQ(soundingNote(state_, 1), 67);  // G root
}

TEST_F(ChordEngineTest, ArpPendingFirstNoteCancelledByAllNotesOff) {
    state_.pendingChord.play_mode = PlayMode::Arpeggio;
    state_.pendingChord.arp_rate = ArpRate::Sixteenth;
    state_.rhythmClock.tickAbs = 5;
    state_.rhythmClock.nextTickUs = 1020000;

    engine_->handleKeyEvent({0x17, true, 0}, 1000000);
    engine_->update(1015000);
    EXPECT_EQ(midi_.noteOnCount(), 0);

    engine_->allNotesOff();
    engine_->update(1020000);
    EXPECT_EQ(midi_.noteOnCount(), 0);
}
