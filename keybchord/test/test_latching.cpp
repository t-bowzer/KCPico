#include "chord_engine_fixture.h"


// Held-mode latching (FR-C9 / VR-5): editing a chord parameter while a chord is
// sounding must not alter the currently-held notes; it applies on next trigger.

TEST_F(ChordEngineTest, HeldChordSustainsAfterRelease) {
    engine_->handleKeyEvent(key(0x17, true), 0);   // T = C major
    EXPECT_EQ(midi_.noteOnCount(), 3);

    engine_->handleKeyEvent(key(0x17, false), 0);  // release T
    // Held mode: chord keeps sounding, no note-offs.
    EXPECT_EQ(midi_.noteOnCount(), 3);
    EXPECT_EQ(midi_.noteOffCount(), 0);
    EXPECT_TRUE(state_.isNoteActive(1, 60));
    EXPECT_TRUE(state_.isNoteActive(1, 64));
    EXPECT_TRUE(state_.isNoteActive(1, 67));
}

TEST_F(ChordEngineTest, HeldChordLocksVelocity) {
    engine_->handleKeyEvent(key(0x17, true), 0);   // T = C major, vel 100
    EXPECT_EQ(midi_.noteOnCount(), 3);
    int msgCount = static_cast<int>(midi_.messages().size());

    // Simulate a velocity edit (e.g. F3) — pending only.
    state_.pendingChord.velocity = 50;
    EXPECT_EQ(static_cast<int>(midi_.messages().size()), msgCount);  // no new MIDI
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    // New chord press uses the edited velocity (release T first: Held sustains).
    engine_->handleKeyEvent(key(0x17, false), 0);  // release T
    engine_->handleKeyEvent(key(0x1C, true), 0);   // Y = G major
    // 3 note-offs (old C) + 3 note-ons (new G, vel 50).
    EXPECT_EQ(midi_.noteOffCount(), 3);
    EXPECT_EQ(midi_.noteOnCount(), 6);

    int vel50 = 0;
    for (const auto& m : midi_.messages()) {
        if ((m.status & 0xF0) == midi::STATUS_NOTE_ON && m.data2 > 0 && m.data2 == 50) vel50++;
    }
    EXPECT_EQ(vel50, 3);  // G major notes at the new velocity
    EXPECT_FALSE(state_.isNoteActive(1, 60));
}

TEST_F(ChordEngineTest, HeldChordLocksOctave) {
    engine_->handleKeyEvent(key(0x17, true), 0);   // C major = {60,64,67}
    auto first = noteOnNotes();
    ASSERT_EQ(first, (std::vector<uint8_t>{60, 64, 67}));

    // Octave edit (pending only) does not transpose the sounding chord.
    state_.pendingChord.octave = 1;              // (EditEngine: = on number row)
    EXPECT_EQ(state_.pendingChord.octave, 1);
    EXPECT_EQ(state_.activeChord.octave, 0);
    EXPECT_EQ(midi_.noteOnCount(), 3);  // unchanged

    // Next chord is transposed by the new octave (release T first).
    engine_->handleKeyEvent(key(0x17, false), 0);  // release T
    engine_->handleKeyEvent(key(0x17, true), 0);   // C major at octave +1 = {72,76,79}
    EXPECT_EQ(state_.activeChord.octave, 1);
    bool has72 = false;
    for (const auto& m : midi_.messages()) {
        if ((m.status & 0xF0) == midi::STATUS_NOTE_ON && m.data2 > 0 && m.data1 == 72) has72 = true;
    }
    EXPECT_TRUE(has72);  // root of C major up one octave
    EXPECT_FALSE(state_.isNoteActive(1, 60));
}

TEST_F(ChordEngineTest, HeldChordLocksExtension) {
    engine_->handleKeyEvent(key(0x17, true), 0);   // C major = {60,64,67}
    EXPECT_EQ(noteOnNotes(), (std::vector<uint8_t>{60, 64, 67}));

    // Toggle add9 (Left arrow) — pending only, sounding chord unchanged.
    state_.pendingChord.add9 = true;             // (EditEngine: Left arrow)
    EXPECT_TRUE(state_.pendingChord.add9);
    EXPECT_FALSE(state_.activeChord.add9);
    EXPECT_EQ(midi_.noteOnCount(), 3);
}
