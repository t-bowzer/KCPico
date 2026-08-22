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
    // nothing happens; releasing the new key keeps C major until a fresh chord
    // is played.
    engine_->handleKeyEvent(key(0x17, true), 0);   // T = C major
    engine_->handleKeyEvent(key(0x1C, true), 0);   // Y = invalid combo
    EXPECT_TRUE(state_.isNoteActive(1, 60));       // C major still sounding
    midi_.clear();

    engine_->handleKeyEvent(key(0x1C, false), 0);  // release Y first
    EXPECT_EQ(midi_.noteOnCount(), 0);             // C major kept
    EXPECT_TRUE(state_.isNoteActive(1, 60));
    EXPECT_FALSE(state_.isNoteActive(1, 71));

    // Releasing the old key then pressing Y fresh takes effect.
    engine_->handleKeyEvent(key(0x17, false), 0);  // release T
    engine_->handleKeyEvent(key(0x1C, true), 0);   // press Y = G major
    EXPECT_EQ(noteOnNotes(), (std::vector<uint8_t>{67, 71, 74}));
    EXPECT_FALSE(state_.isNoteActive(1, 60));
    EXPECT_TRUE(state_.isNoteActive(1, 67));
}

TEST_F(ChordEngineTest, ModeChangeReleasesLatchedChord) {
    engine_->handleKeyEvent(key(0x17, true), 0);   // T = C major (Held)
    engine_->handleKeyEvent(key(0x17, false), 0);  // release: Held sustains
    EXPECT_TRUE(state_.isNoteActive(1, 60));
    EXPECT_EQ(midi_.noteOffCount(), 0);

    // EditEngine changes the mode and then notifies; no key held -> release.
    state_.pendingChord.play_mode = PlayMode::PressToPlay;
    engine_->onModeChanged();
    EXPECT_EQ(midi_.noteOffCount(), 3);
    EXPECT_FALSE(state_.isNoteActive(1, 60));
    EXPECT_FALSE(state_.isNoteActive(1, 64));
    EXPECT_FALSE(state_.isNoteActive(1, 67));
}

TEST_F(ChordEngineTest, ModeChangeWithKeyHeldKeepsChord) {
    engine_->handleKeyEvent(key(0x17, true), 0);   // T = C major, held
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    // Change to PressToPlay while T is still held -> chord kept.
    state_.pendingChord.play_mode = PlayMode::PressToPlay;
    engine_->onModeChanged();
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

// Held extension (FR-C7): pressing Left while a chord sounds adds add9 to the
// currently-sounding chord (latching onto it), even after the chord key is
// released — but only the extension note changes, the base chord is not
// re-attacked.
TEST_F(ChordEngineTest, HeldExtensionAddsToSoundingChord) {
    engine_->handleKeyEvent(key(0x17, true), 0);   // T = C major {60,64,67}
    EXPECT_EQ(noteOnNotes(), (std::vector<uint8_t>{60, 64, 67}));
    engine_->handleKeyEvent(key(0x17, false), 0);  // release: Held sustains
    midi_.clear();

    engine_->handleKeyEvent(key(0x50, true), 0);   // Left arrow held -> add9
    EXPECT_EQ(noteOnNotes(), (std::vector<uint8_t>{74}));  // only the 9th added
    EXPECT_EQ(midi_.noteOffCount(), 0);            // base chord untouched
    EXPECT_TRUE(state_.isNoteActive(1, 60));
    EXPECT_TRUE(state_.isNoteActive(1, 64));
    EXPECT_TRUE(state_.isNoteActive(1, 67));
    EXPECT_TRUE(state_.isNoteActive(1, 74));

    // Releasing Left removes only the 9th.
    midi_.clear();
    engine_->handleKeyEvent(key(0x50, false), 0);
    EXPECT_TRUE(noteOnNotes().empty());
    EXPECT_EQ(midi_.noteOffCount(), 1);
    EXPECT_FALSE(state_.isNoteActive(1, 74));
    EXPECT_TRUE(state_.isNoteActive(1, 60));
}

// Manual inversion (VR-8): PrtSc selects 1st inversion — the 3rd becomes the
// lowest note.
TEST_F(ChordEngineTest, ManualInversionFirst) {
    state_.pendingChord.inversion = InversionMode::First;
    engine_->handleKeyEvent(key(0x17, true), 0);   // C major, 1st inversion
    // Root-position {60,64,67}; 1st inversion rotates the root up an octave.
    EXPECT_EQ(noteOnNotes(), (std::vector<uint8_t>{64, 67, 72}));
}

// An overlapped chord switch in Held mode (new key pressed before the old key is
// lifted) must land on the new chord once the stale key is released.
TEST_F(ChordEngineTest, HeldOverlappedSwitchLandsOnNewChord) {
    engine_->handleKeyEvent(key(0x17, true), 0);    // T = C major
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    engine_->handleKeyEvent(key(0x1C, true), 0);    // Y = G major (overlap)
    EXPECT_TRUE(state_.isNoteActive(1, 60));        // still C (invalid combo)
    EXPECT_FALSE(state_.isNoteActive(1, 71));       // B (3rd of G) not present

    midi_.clear();
    engine_->handleKeyEvent(key(0x17, false), 0);   // release T -> G major
    EXPECT_FALSE(state_.isNoteActive(1, 60));
    EXPECT_TRUE(state_.isNoteActive(1, 71));
    EXPECT_TRUE(state_.isNoteActive(1, 74));
}

// PressToPlay resolves an overlapped switch immediately on release (no 25 ms
// release-buffer delay).
TEST_F(ChordEngineTest, PressToPlayOverlappedSwitchIsImmediate) {
    state_.pendingChord.play_mode = PlayMode::PressToPlay;
    engine_->handleKeyEvent(key(0x17, true), 0);    // T = C major
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    engine_->handleKeyEvent(key(0x1C, true), 0);    // Y = G major (overlap)
    EXPECT_TRUE(state_.isNoteActive(1, 60));        // still C

    midi_.clear();
    engine_->handleKeyEvent(key(0x17, false), 0);   // release T -> G immediately
    EXPECT_FALSE(state_.isNoteActive(1, 60));
    EXPECT_TRUE(state_.isNoteActive(1, 67));
    EXPECT_TRUE(state_.isNoteActive(1, 71));
    EXPECT_TRUE(state_.isNoteActive(1, 74));
    EXPECT_EQ(midi_.noteOffCount(), 3);             // old C major released
}

// Arpeggio resolves an overlapped switch immediately on release (no stutter from
// the release buffer or a trailing old-chord arp step).
TEST_F(ChordEngineTest, ArpOverlappedSwitchIsImmediate) {
    state_.pendingChord.play_mode = PlayMode::Arpeggio;
    engine_->handleKeyEvent(key(0x17, true), 0);    // C major arp, starts on 60
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    engine_->handleKeyEvent(key(0x1C, true), 0);    // Y = G major (overlap)
    EXPECT_TRUE(state_.isNoteActive(1, 60));        // still C

    midi_.clear();
    engine_->handleKeyEvent(key(0x17, false), 0);   // release T -> G arp restarts
    EXPECT_FALSE(state_.isNoteActive(1, 60));
    EXPECT_TRUE(state_.isNoteActive(1, 67));        // G major root
}

// Chord roll (VR-9) staggers by milliseconds, not microseconds: a 1000ms roll
// must wait a full second between notes.
TEST_F(ChordEngineTest, ChordRollStaggersByMilliseconds) {
    state_.pendingChord.chord_roll_ms = 1000;  // 1 s per note
    engine_->handleKeyEvent(key(0x17, true), 0);   // C major {60,64,67}
    EXPECT_EQ(midi_.noteOnCount(), 0);             // all notes scheduled, none fired

    engine_->update(0);                            // first note (deadline 0) fires
    EXPECT_EQ(midi_.noteOnCount(), 1);
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    engine_->update(500000);                       // 500 ms later
    EXPECT_EQ(midi_.noteOnCount(), 1);             // still one note

    engine_->update(1000000);                      // 1 s later
    EXPECT_EQ(midi_.noteOnCount(), 2);
    EXPECT_TRUE(state_.isNoteActive(1, 64));

    engine_->update(2000000);                      // 2 s later
    EXPECT_EQ(midi_.noteOnCount(), 3);
    EXPECT_TRUE(state_.isNoteActive(1, 67));
}

// With min_interval active, adding an extension must ignore the interval rule:
// only the extension note is added, the base notes are never re-attacked.
TEST_F(ChordEngineTest, MinIntervalExtensionDoesNotRestructureBase) {
    state_.pendingChord.min_interval = 5;
    engine_->handleKeyEvent(key(0x17, true), 0);   // C major spread {60,67,76}
    EXPECT_EQ(noteOnNotes(), (std::vector<uint8_t>{60, 67, 76}));
    midi_.clear();

    engine_->handleKeyEvent(key(0x50, true), 0);   // Left -> add9
    EXPECT_EQ(noteOnNotes(), (std::vector<uint8_t>{74}));  // only the 9th added
    EXPECT_EQ(midi_.noteOffCount(), 0);
    EXPECT_TRUE(state_.isNoteActive(1, 60));
    EXPECT_TRUE(state_.isNoteActive(1, 67));
    EXPECT_TRUE(state_.isNoteActive(1, 76));
    EXPECT_TRUE(state_.isNoteActive(1, 74));

    // Releasing the extension removes only the 9th, base notes unchanged.
    midi_.clear();
    engine_->handleKeyEvent(key(0x50, false), 0);
    EXPECT_TRUE(noteOnNotes().empty());
    EXPECT_EQ(midi_.noteOffCount(), 1);
    EXPECT_TRUE(state_.isNoteActive(1, 60));
    EXPECT_TRUE(state_.isNoteActive(1, 67));
    EXPECT_TRUE(state_.isNoteActive(1, 76));
    EXPECT_FALSE(state_.isNoteActive(1, 74));
}

// Adding an extension during arpeggio adds the note to the step sequence without
// re-attacking the currently-sounding arp note.
TEST_F(ChordEngineTest, ArpExtensionDoesNotRetrigger) {
    state_.pendingChord.play_mode = PlayMode::Arpeggio;
    engine_->handleKeyEvent(key(0x17, true), 0);   // C major, arp starts on 60
    EXPECT_EQ(midi_.noteOnCount(), 1);
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    engine_->handleKeyEvent(key(0x50, true), 0);   // Left -> add9
    EXPECT_EQ(midi_.noteOffCount(), 0);            // current note not released
    EXPECT_EQ(midi_.noteOnCount(), 1);             // no re-attack
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    // The arp now cycles four notes (root-position + the 9th). stepUs(120)=125000.
    engine_->update(125000);
    EXPECT_TRUE(state_.isNoteActive(1, 64));
    engine_->update(250000);
    EXPECT_TRUE(state_.isNoteActive(1, 67));
    engine_->update(375000);
    EXPECT_TRUE(state_.isNoteActive(1, 74));       // the added 9th
}
