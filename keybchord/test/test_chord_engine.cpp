#include "chord_engine_fixture.h"


// Note lifecycle per play mode (FR-C3 / FR-C10) plus the release-buffer
// behavior for combinations (release debounce).

TEST_F(ChordEngineTest, HeldNewChordReplacesPrevious) {
    engine_->handleKeyEvent(key(0x17, true), 0);    // T = C major
    EXPECT_EQ(midi_.noteOnCount(), 3);
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    engine_->handleKeyEvent(key(0x17, false), 0);   // release: Held sustains
    EXPECT_EQ(midi_.noteOffCount(), 0);

    engine_->handleKeyEvent(key(0x1C, true), 0);    // Y = G major (replaces C)
    EXPECT_EQ(midi_.noteOffCount(), 3);
    EXPECT_EQ(midi_.noteOnCount(), 6);
    EXPECT_FALSE(state_.isNoteActive(1, 60));
    EXPECT_TRUE(state_.isNoteActive(1, 67));
}

TEST_F(ChordEngineTest, HeldCombinationProducesMaj7) {
    engine_->handleKeyEvent(key(0x17, true), 0);   // T = Major col 5 (C)
    midi_.clear();
    engine_->handleKeyEvent(key(0x05, true), 0);   // B = Seventh col 5 -> Cmaj7

    auto notes = noteOnNotes();
    EXPECT_EQ(notes, (std::vector<uint8_t>{60, 64, 67, 71}));
}

TEST_F(ChordEngineTest, PressToPlayNoteLifecycle) {
    state_.pendingChord.play_mode = PlayMode::PressToPlay;

    engine_->handleKeyEvent(key(0x17, true), 0);   // press: note-on
    EXPECT_EQ(midi_.noteOnCount(), 3);
    EXPECT_EQ(midi_.noteOffCount(), 0);
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    engine_->handleKeyEvent(key(0x17, false), 0);  // release: debounced
    engine_->update(25000);                        // buffer expires -> note-off
    EXPECT_EQ(midi_.noteOffCount(), 3);
    EXPECT_FALSE(state_.isNoteActive(1, 60));
    EXPECT_FALSE(state_.isNoteActive(1, 64));
    EXPECT_FALSE(state_.isNoteActive(1, 67));
}

TEST_F(ChordEngineTest, ArpeggioStepsAndLoops) {
    state_.pendingChord.play_mode = PlayMode::Arpeggio;

    engine_->handleKeyEvent(key(0x17, true), 0);   // C major {60,64,67}
    EXPECT_EQ(midi_.noteOnCount(), 1);
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    engine_->update(500000);   // well past a step (stepUs(120) = 125000)
    EXPECT_TRUE(state_.isNoteActive(1, 64));
    EXPECT_FALSE(state_.isNoteActive(1, 60));

    engine_->update(1000000);
    EXPECT_TRUE(state_.isNoteActive(1, 67));

    engine_->update(1500000);  // loop back to 60
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    engine_->handleKeyEvent(key(0x17, false), 2000000);  // release
    engine_->update(2100000);                            // buffer expires -> stop
    EXPECT_FALSE(state_.isNoteActive(1, 60));
    EXPECT_FALSE(state_.isNoteActive(1, 64));
    EXPECT_FALSE(state_.isNoteActive(1, 67));
}

// Arpeggio advances at the stored rhythm tempo (stepUs(tempo)) even when the
// rhythm and clock are both off — not the chord note_duration_ms.
TEST_F(ChordEngineTest, ArpeggioUsesStoredTempo) {
    state_.pendingChord.play_mode = PlayMode::Arpeggio;
    state_.pendingRhythm.enabled = false;
    state_.pendingRhythm.tempo = 60;  // stepUs(60) = 250000 us

    engine_->handleKeyEvent(key(0x17, true), 0);  // C major {60,64,67}
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    engine_->update(200000);  // before one step elapses
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    engine_->update(250000);  // one step later -> 64
    EXPECT_TRUE(state_.isNoteActive(1, 64));
    EXPECT_FALSE(state_.isNoteActive(1, 60));
}

TEST_F(ChordEngineTest, SilentModeEmitsNoNotes) {
    state_.pendingChord.play_mode = PlayMode::Silent;

    engine_->handleKeyEvent(key(0x17, true), 0);
    EXPECT_EQ(midi_.noteOnCount(), 0);
    EXPECT_EQ(midi_.noteOffCount(), 0);
}

TEST_F(ChordEngineTest, AllNotesOffClearsState) {
    engine_->handleKeyEvent(key(0x17, true), 0);
    EXPECT_EQ(midi_.noteOnCount(), 3);

    engine_->allNotesOff();
    EXPECT_EQ(midi_.noteOffCount(), 3);
    EXPECT_FALSE(state_.isNoteActive(1, 60));
    EXPECT_FALSE(state_.isNoteActive(1, 64));
    EXPECT_FALSE(state_.isNoteActive(1, 67));
}

TEST_F(ChordEngineTest, HeldReleaseDoesNotChangeChord) {
    // T+B = Cmaj7. Releasing a key keeps the chord latched; notes are released
    // only when another chord is played (FR-C10 held semantics).
    engine_->handleKeyEvent(key(0x17, true), 0);   // T = C major
    engine_->handleKeyEvent(key(0x05, true), 0);   // B = Cmaj7
    midi_.clear();

    engine_->handleKeyEvent(key(0x17, false), 0);  // release T, B still held
    EXPECT_EQ(midi_.noteOnCount(), 0);
    EXPECT_EQ(midi_.noteOffCount(), 0);
    EXPECT_TRUE(state_.isNoteActive(1, 71));       // Cmaj7 still sounding

    engine_->handleKeyEvent(key(0x05, false), 0);  // release B
    EXPECT_EQ(midi_.noteOffCount(), 0);            // still latched

    // Playing a new chord releases the old notes and sounds the new chord.
    engine_->handleKeyEvent(key(0x1C, true), 0);   // Y = G major
    EXPECT_EQ(midi_.noteOffCount(), 4);            // Cmaj7 released
    EXPECT_FALSE(state_.isNoteActive(1, 60));      // C (in Cmaj7, not G major)
    EXPECT_TRUE(state_.isNoteActive(1, 67));
}

TEST_F(ChordEngineTest, InvalidComboKeepsChordUntilNewPress) {
    // T (C major) + Y (G major) is an invalid combination. While both are held
    // nothing happens; releasing keys keeps C major until a fresh chord is played.
    engine_->handleKeyEvent(key(0x17, true), 0);   // T = C major
    engine_->handleKeyEvent(key(0x1C, true), 0);   // Y = invalid combo
    EXPECT_TRUE(state_.isNoteActive(1, 60));       // C major still sounding
    midi_.clear();

    engine_->handleKeyEvent(key(0x17, false), 0);  // release T, Y still held
    EXPECT_EQ(midi_.noteOnCount(), 0);             // Held: no re-resolution
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    // A fresh press of Y takes effect.
    engine_->handleKeyEvent(key(0x1C, false), 0);  // release Y
    engine_->handleKeyEvent(key(0x1C, true), 0);   // press Y = G major
    EXPECT_EQ(noteOnNotes(), (std::vector<uint8_t>{67, 71, 74}));
    EXPECT_FALSE(state_.isNoteActive(1, 60));
    EXPECT_TRUE(state_.isNoteActive(1, 67));
}

TEST_F(ChordEngineTest, ArrowKeysToggleExtensions) {
    engine_->handleKeyEvent(key(0x17, true), 0);   // C major {60,64,67}
    midi_.clear();

    engine_->handleKeyEvent(key(0x50, true), 0);   // Left arrow -> add9 on
    EXPECT_TRUE(state_.pendingChord.add9);

    // Re-trigger (release then press): extension applies on next trigger.
    engine_->handleKeyEvent(key(0x17, false), 0);
    engine_->handleKeyEvent(key(0x17, true), 0);   // C major + add9
    EXPECT_EQ(noteOnNotes(), (std::vector<uint8_t>{60, 64, 67, 74}));
}

TEST_F(ChordEngineTest, ArrowKeysToggleExtensionsIndependently) {
    // Down = add11, Right = add13.
    engine_->handleKeyEvent(key(0x51, true), 0);   // Down -> add11
    engine_->handleKeyEvent(key(0x4F, true), 0);   // Right -> add13
    EXPECT_TRUE(state_.pendingChord.add11);
    EXPECT_TRUE(state_.pendingChord.add13);
    EXPECT_FALSE(state_.pendingChord.add9);

    engine_->handleKeyEvent(key(0x17, true), 0);   // C major + add11 + add13
    EXPECT_EQ(noteOnNotes(), (std::vector<uint8_t>{60, 64, 67, 77, 81}));
}

TEST_F(ChordEngineTest, SwitchingModeReleasesHeldChord) {
    engine_->handleKeyEvent(key(0x17, true), 0);   // T = C major (Held)
    engine_->handleKeyEvent(key(0x17, false), 0);  // release: Held sustains
    EXPECT_TRUE(state_.isNoteActive(1, 60));
    EXPECT_EQ(midi_.noteOffCount(), 0);

    // Cycle play mode Held -> PressToPlay (F1). No key held -> release chord.
    engine_->handleKeyEvent(key(0x3A, true), 0);   // F1
    EXPECT_EQ(state_.pendingChord.play_mode, PlayMode::PressToPlay);
    EXPECT_EQ(midi_.noteOffCount(), 3);
    EXPECT_FALSE(state_.isNoteActive(1, 60));
    EXPECT_FALSE(state_.isNoteActive(1, 64));
    EXPECT_FALSE(state_.isNoteActive(1, 67));
}

TEST_F(ChordEngineTest, SwitchingModeWithKeyHeldKeepsChord) {
    engine_->handleKeyEvent(key(0x17, true), 0);   // T = C major, held
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    // Cycle to PressToPlay while T is still held -> chord kept.
    engine_->handleKeyEvent(key(0x3A, true), 0);   // F1
    EXPECT_EQ(state_.pendingChord.play_mode, PlayMode::PressToPlay);
    EXPECT_TRUE(state_.isNoteActive(1, 60));
    EXPECT_EQ(midi_.noteOffCount(), 0);
}

TEST_F(ChordEngineTest, NoStuckNotesAfterFullCycle) {
    // Press-to-play full cycle must leave zero sounding notes (fixes the M2 stuck C4).
    state_.pendingChord.play_mode = PlayMode::PressToPlay;
    engine_->handleKeyEvent(key(0x17, true), 0);
    engine_->handleKeyEvent(key(0x17, false), 0);
    engine_->update(25000);
    EXPECT_EQ(midi_.noteOnCount(), 3);
    EXPECT_EQ(midi_.noteOffCount(), 3);
    EXPECT_TRUE(state_.activeNotes.empty());
}

// AC-6: with rhythm enabled, Arpeggio advances on the rhythm clock's step
// edges instead of its own note-duration timer.
TEST_F(ChordEngineTest, ArpeggioFollowsRhythmClock) {
    state_.pendingChord.play_mode = PlayMode::Arpeggio;
    state_.pendingRhythm.enabled = true;

    engine_->handleKeyEvent(key(0x17, true), 0);  // C major {60,64,67}
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    // Simulate the Core 1 scheduler firing a step.
    state_.rhythmClock.running = true;
    state_.rhythmClock.stepAbs = 1;
    engine_->update(0);  // no time passes, but a rhythm step edge fires
    EXPECT_TRUE(state_.isNoteActive(1, 64));
    EXPECT_FALSE(state_.isNoteActive(1, 60));

    state_.rhythmClock.stepAbs = 2;
    engine_->update(0);
    EXPECT_TRUE(state_.isNoteActive(1, 67));
    EXPECT_FALSE(state_.isNoteActive(1, 64));
}

// AC-6: Rhythm mode steps through the voicing on rhythm steps when rhythm is
// enabled; without rhythm it stays a Held equivalent.
TEST_F(ChordEngineTest, RhythmModeStepsWithRhythmEnabled) {
    state_.pendingChord.play_mode = PlayMode::Rhythm;
    state_.pendingRhythm.enabled = true;

    engine_->handleKeyEvent(key(0x17, true), 0);  // C major {60,64,67}
    EXPECT_TRUE(state_.isNoteActive(1, 60));
    EXPECT_FALSE(state_.isNoteActive(1, 64));  // stepped, not held
    EXPECT_FALSE(state_.isNoteActive(1, 67));

    state_.rhythmClock.running = true;
    state_.rhythmClock.stepAbs = 1;
    engine_->update(0);
    EXPECT_TRUE(state_.isNoteActive(1, 64));
}

TEST_F(ChordEngineTest, RhythmModeWithoutRhythmStepsAtTempo) {
    state_.pendingChord.play_mode = PlayMode::Rhythm;
    state_.pendingRhythm.enabled = false;

    engine_->handleKeyEvent(key(0x17, true), 0);  // C major {60,64,67}
    EXPECT_EQ(midi_.noteOnCount(), 1);  // steps one note at a time
    EXPECT_TRUE(state_.isNoteActive(1, 60));
    EXPECT_FALSE(state_.isNoteActive(1, 64));
    EXPECT_FALSE(state_.isNoteActive(1, 67));
}
