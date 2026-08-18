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
}

TEST_F(ChordEngineTest, ArpModeUp) {
    setupArp(*engine_, state_, ArpMode::Up);
    EXPECT_EQ(soundingNote(state_, 1), 60);

    state_.rhythmClock.stepAbs = 1; engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 64);
    state_.rhythmClock.stepAbs = 2; engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 67);
    state_.rhythmClock.stepAbs = 3; engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 60);  // wraps
}

TEST_F(ChordEngineTest, ArpModeDown) {
    setupArp(*engine_, state_, ArpMode::Down);
    EXPECT_EQ(soundingNote(state_, 1), 67);

    state_.rhythmClock.stepAbs = 1; engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 64);
    state_.rhythmClock.stepAbs = 2; engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 60);
    state_.rhythmClock.stepAbs = 3; engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 67);  // wraps
}

TEST_F(ChordEngineTest, ArpModeUpDown) {
    setupArp(*engine_, state_, ArpMode::UpDown);
    EXPECT_EQ(soundingNote(state_, 1), 60);

    state_.rhythmClock.stepAbs = 1; engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 64);
    state_.rhythmClock.stepAbs = 2; engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 67);
    state_.rhythmClock.stepAbs = 3; engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 64);  // down
    state_.rhythmClock.stepAbs = 4; engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 60);  // back to root
}

TEST_F(ChordEngineTest, ArpModeAlternating) {
    setupArp(*engine_, state_, ArpMode::Alternating);
    // Sequence [0,2,1] for a 3-note chord: 60, 67, 64.
    EXPECT_EQ(soundingNote(state_, 1), 60);

    state_.rhythmClock.stepAbs = 1; engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 67);
    state_.rhythmClock.stepAbs = 2; engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 64);
    state_.rhythmClock.stepAbs = 3; engine_->update(0);
    EXPECT_EQ(soundingNote(state_, 1), 60);  // wraps
}

TEST_F(ChordEngineTest, ArpModeRandomPlaysChordNote) {
    setupArp(*engine_, state_, ArpMode::Random);
    for (int i = 0; i < 8; i++) {
        state_.rhythmClock.stepAbs = static_cast<uint32_t>(i + 1);
        engine_->update(0);
        uint8_t n = soundingNote(state_, 1);
        EXPECT_TRUE(n == 60 || n == 64 || n == 67);  // always a chord note
    }
}
