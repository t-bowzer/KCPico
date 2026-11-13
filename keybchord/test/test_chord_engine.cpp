#include "chord_engine_fixture.h"


// Note lifecycle per play mode (FR-C3 / FR-C10) plus the release-buffer
// behavior for combinations (release debounce).

TEST_F(ChordEngineTest, HeldNewChordReplacesPrevious) {
    press(0x17, 0);    // T = C major
    EXPECT_EQ(midi_.noteOnCount(), 3);
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    release(0x17, 0);   // release: Held sustains
    EXPECT_EQ(midi_.noteOffCount(), 0);

    press(0x1C, 0);    // Y = G major (replaces C)
    EXPECT_EQ(midi_.noteOffCount(), 3);
    EXPECT_EQ(midi_.noteOnCount(), 6);
    EXPECT_FALSE(state_.isNoteActive(1, 60));
    EXPECT_TRUE(state_.isNoteActive(1, 67));
}

TEST_F(ChordEngineTest, NearSimultaneousCombinationTriggersOnlyFinalChord) {
    // T then B 1 ms apart: the first chord must not sound on its own.
    engine_->handleKeyEvent(key(0x17, true), 0);      // T = C major
    engine_->handleKeyEvent(key(0x05, true), 1000);   // B = seventh
    engine_->update(1000 + ChordEngine::PRESS_GROUP_US);

    EXPECT_EQ(noteOnNotes(), (std::vector<uint8_t>{60, 64, 67, 71}));
}

TEST_F(ChordEngineTest, QuickCombinationTapTriggersOneChord) {
    engine_->handleKeyEvent(key(0x17, true), 0);
    engine_->handleKeyEvent(key(0x05, true), 2000);
    engine_->handleKeyEvent(key(0x17, false), 6000);
    engine_->handleKeyEvent(key(0x05, false), 8000);

    EXPECT_EQ(noteOnNotes(), (std::vector<uint8_t>{60, 64, 67, 71}));
}

TEST_F(ChordEngineTest, HeldCombinationProducesMaj7) {
    press(0x17, 0);   // T = Major col 5 (C)
    midi_.clear();
    press(0x05, 0);   // B = Seventh col 5 -> Cmaj7

    auto notes = noteOnNotes();
    EXPECT_EQ(notes, (std::vector<uint8_t>{60, 64, 67, 71}));
}

TEST_F(ChordEngineTest, PressToPlayNoteLifecycle) {
    state_.pendingChord.play_mode = PlayMode::PressToPlay;

    press(0x17, 0);   // press: note-on
    EXPECT_EQ(midi_.noteOnCount(), 3);
    EXPECT_EQ(midi_.noteOffCount(), 0);
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    release(0x17, 0);  // release: debounced
    engine_->update(25000);                        // buffer expires -> note-off
    EXPECT_EQ(midi_.noteOffCount(), 3);
    EXPECT_FALSE(state_.isNoteActive(1, 60));
    EXPECT_FALSE(state_.isNoteActive(1, 64));
    EXPECT_FALSE(state_.isNoteActive(1, 67));
}

TEST_F(ChordEngineTest, ArpeggioStepsAndLoops) {
    state_.pendingChord.play_mode = PlayMode::Arpeggio;

    press(0x17, 0);   // C major {60,64,67}
    EXPECT_EQ(midi_.noteOnCount(), 1);
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    state_.rhythmClock.tickAbs = 6;    // 16th boundary
    engine_->update(0);
    EXPECT_TRUE(state_.isNoteActive(1, 64));
    EXPECT_FALSE(state_.isNoteActive(1, 60));

    state_.rhythmClock.tickAbs = 12;
    engine_->update(0);
    EXPECT_TRUE(state_.isNoteActive(1, 67));

    state_.rhythmClock.tickAbs = 18;   // loop back to 60
    engine_->update(0);
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    release(0x17, 2000000);  // release
    engine_->update(2100000);                            // buffer expires -> stop
    EXPECT_FALSE(state_.isNoteActive(1, 60));
    EXPECT_FALSE(state_.isNoteActive(1, 64));
    EXPECT_FALSE(state_.isNoteActive(1, 67));
}

// The arp follows the free-running master clock even with the rhythm stopped,
// so its notes stay locked to the beat LED.
TEST_F(ChordEngineTest, ArpeggioUsesMasterClockWhenRhythmStopped) {
    state_.pendingChord.play_mode = PlayMode::Arpeggio;
    state_.pendingRhythm.enabled = false;

    press(0x17, 0);  // C major {60,64,67}
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    state_.rhythmClock.tickAbs = 5;   // just before the 16th boundary
    engine_->update(0);
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    state_.rhythmClock.tickAbs = 6;
    engine_->update(0);
    EXPECT_TRUE(state_.isNoteActive(1, 64));
    EXPECT_FALSE(state_.isNoteActive(1, 60));
}

TEST_F(ChordEngineTest, SilentModeEmitsNoNotes) {
    state_.pendingChord.play_mode = PlayMode::Silent;

    press(0x17, 0);
    EXPECT_EQ(midi_.noteOnCount(), 0);
    EXPECT_EQ(midi_.noteOffCount(), 0);
}

TEST_F(ChordEngineTest, AllNotesOffClearsState) {
    press(0x17, 0);
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
    press(0x17, 0);   // T = C major
    press(0x05, 0);   // B = Cmaj7
    midi_.clear();

    release(0x17, 0);  // release T, B still held
    EXPECT_EQ(midi_.noteOnCount(), 0);
    EXPECT_EQ(midi_.noteOffCount(), 0);
    EXPECT_TRUE(state_.isNoteActive(1, 71));       // Cmaj7 still sounding

    release(0x05, 0);  // release B
    EXPECT_EQ(midi_.noteOffCount(), 0);            // still latched

    // Playing a new chord releases the old notes and sounds the new chord.
    press(0x1C, 0);   // Y = G major
    EXPECT_EQ(midi_.noteOffCount(), 4);            // Cmaj7 released
    EXPECT_FALSE(state_.isNoteActive(1, 60));      // C (in Cmaj7, not G major)
    EXPECT_TRUE(state_.isNoteActive(1, 67));
}

TEST_F(ChordEngineTest, InvalidComboKeepsChordUntilNewPress) {
    // T (C major) + Y (G major) is an invalid combination. While both are held
    // nothing happens; releasing the new key keeps C major until a fresh chord
    // is played.
    press(0x17, 0);   // T = C major
    press(0x1C, 0);   // Y = invalid combo
    EXPECT_TRUE(state_.isNoteActive(1, 60));       // C major still sounding
    midi_.clear();

    release(0x1C, 0);  // release Y first
    EXPECT_EQ(midi_.noteOnCount(), 0);             // C major kept
    EXPECT_TRUE(state_.isNoteActive(1, 60));
    EXPECT_FALSE(state_.isNoteActive(1, 71));

    // Releasing the old key then pressing Y fresh takes effect.
    release(0x17, 0);  // release T
    press(0x1C, 0);   // press Y = G major
    EXPECT_EQ(noteOnNotes(), (std::vector<uint8_t>{67, 71, 74}));
    EXPECT_FALSE(state_.isNoteActive(1, 60));
    EXPECT_TRUE(state_.isNoteActive(1, 67));
}

TEST_F(ChordEngineTest, ModeChangeReleasesLatchedChord) {
    press(0x17, 0);   // T = C major (Held)
    release(0x17, 0);  // release: Held sustains
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
    press(0x17, 0);   // T = C major, held
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
    press(0x17, 0);
    release(0x17, 0);
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

    press(0x17, 0);  // C major {60,64,67}
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    // Simulate the Core 1 scheduler firing a step (6 clock ticks per 16th).
    state_.rhythmClock.running = true;
    state_.rhythmClock.tickAbs = 6;
    engine_->update(0);  // no time passes, but a rhythm step edge fires
    EXPECT_TRUE(state_.isNoteActive(1, 64));
    EXPECT_FALSE(state_.isNoteActive(1, 60));

    state_.rhythmClock.tickAbs = 12;
    engine_->update(0);
    EXPECT_TRUE(state_.isNoteActive(1, 67));
    EXPECT_FALSE(state_.isNoteActive(1, 64));
}

// Held extension (FR-C7): pressing Left while a chord sounds adds add9 to the
// currently-sounding chord (latching onto it), even after the chord key is
// released — but only the extension note changes, the base chord is not
// re-attacked.
TEST_F(ChordEngineTest, HeldExtensionAddsToSoundingChord) {
    press(0x17, 0);   // T = C major {60,64,67}
    EXPECT_EQ(noteOnNotes(), (std::vector<uint8_t>{60, 64, 67}));
    release(0x17, 0);  // release: Held sustains
    midi_.clear();

    press(0x50, 0);   // Left arrow held -> add9
    EXPECT_EQ(noteOnNotes(), (std::vector<uint8_t>{74}));  // only the 9th added
    EXPECT_EQ(midi_.noteOffCount(), 0);            // base chord untouched
    EXPECT_TRUE(state_.isNoteActive(1, 60));
    EXPECT_TRUE(state_.isNoteActive(1, 64));
    EXPECT_TRUE(state_.isNoteActive(1, 67));
    EXPECT_TRUE(state_.isNoteActive(1, 74));

    // Releasing Left removes only the 9th.
    midi_.clear();
    release(0x50, 0);
    EXPECT_TRUE(noteOnNotes().empty());
    EXPECT_EQ(midi_.noteOffCount(), 1);
    EXPECT_FALSE(state_.isNoteActive(1, 74));
    EXPECT_TRUE(state_.isNoteActive(1, 60));
}

// Manual inversion (VR-8): PrtSc selects 1st inversion — the 3rd becomes the
// lowest note.
TEST_F(ChordEngineTest, ManualInversionFirst) {
    state_.pendingChord.inversion = InversionMode::First;
    press(0x17, 0);   // C major, 1st inversion
    // Root-position {60,64,67}; 1st inversion rotates the root up an octave.
    EXPECT_EQ(noteOnNotes(), (std::vector<uint8_t>{64, 67, 72}));
}

// An overlapped chord switch in Held mode (new key pressed before the old key is
// lifted) must land on the new chord once the stale key is released.
TEST_F(ChordEngineTest, HeldOverlappedSwitchLandsOnNewChord) {
    press(0x17, 0);    // T = C major
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    press(0x1C, 0);    // Y = G major (overlap)
    EXPECT_TRUE(state_.isNoteActive(1, 60));        // still C (invalid combo)
    EXPECT_FALSE(state_.isNoteActive(1, 71));       // B (3rd of G) not present

    midi_.clear();
    release(0x17, 0);   // release T -> G major
    EXPECT_FALSE(state_.isNoteActive(1, 60));
    EXPECT_TRUE(state_.isNoteActive(1, 71));
    EXPECT_TRUE(state_.isNoteActive(1, 74));
}

// PressToPlay resolves an overlapped switch immediately on release (no 25 ms
// release-buffer delay).
TEST_F(ChordEngineTest, PressToPlayOverlappedSwitchIsImmediate) {
    state_.pendingChord.play_mode = PlayMode::PressToPlay;
    press(0x17, 0);    // T = C major
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    press(0x1C, 0);    // Y = G major (overlap)
    EXPECT_TRUE(state_.isNoteActive(1, 60));        // still C

    midi_.clear();
    release(0x17, 0);   // release T -> G immediately
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
    press(0x17, 0);    // C major arp, starts on 60
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    press(0x1C, 0);    // Y = G major (overlap)
    EXPECT_TRUE(state_.isNoteActive(1, 60));        // still C

    midi_.clear();
    release(0x17, 0);   // release T -> G arp restarts
    EXPECT_FALSE(state_.isNoteActive(1, 60));
    EXPECT_TRUE(state_.isNoteActive(1, 67));        // G major root
}

// Chord roll (VR-9) staggers by milliseconds, not microseconds: a 1000ms roll
// must wait a full second between notes.
TEST_F(ChordEngineTest, ChordRollStaggersByMilliseconds) {
    state_.pendingChord.chord_roll_ms = 1000;  // 1 s per note
    press(0x17, 0);   // C major {60,64,67}
    EXPECT_EQ(midi_.noteOnCount(), 0);             // all notes scheduled, none fired

    engine_->update(ChordEngine::PRESS_GROUP_US);  // first note fires
    EXPECT_EQ(midi_.noteOnCount(), 1);
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    engine_->update(515000);                       // 500 ms later
    EXPECT_EQ(midi_.noteOnCount(), 1);             // still one note

    engine_->update(1015000);                      // 1 s later
    EXPECT_EQ(midi_.noteOnCount(), 2);
    EXPECT_TRUE(state_.isNoteActive(1, 64));

    engine_->update(2015000);                      // 2 s later
    EXPECT_EQ(midi_.noteOnCount(), 3);
    EXPECT_TRUE(state_.isNoteActive(1, 67));
}

// With min_interval active, adding an extension must ignore the interval rule:
// only the extension note is added, the base notes are never re-attacked.
TEST_F(ChordEngineTest, MinIntervalExtensionDoesNotRestructureBase) {
    state_.pendingChord.min_interval = 5;
    press(0x17, 0);   // C major spread {60,67,76}
    EXPECT_EQ(noteOnNotes(), (std::vector<uint8_t>{60, 67, 76}));
    midi_.clear();

    press(0x50, 0);   // Left -> add9
    EXPECT_EQ(noteOnNotes(), (std::vector<uint8_t>{74}));  // only the 9th added
    EXPECT_EQ(midi_.noteOffCount(), 0);
    EXPECT_TRUE(state_.isNoteActive(1, 60));
    EXPECT_TRUE(state_.isNoteActive(1, 67));
    EXPECT_TRUE(state_.isNoteActive(1, 76));
    EXPECT_TRUE(state_.isNoteActive(1, 74));

    // Releasing the extension removes only the 9th, base notes unchanged.
    midi_.clear();
    release(0x50, 0);
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
    press(0x17, 0);   // C major, arp starts on 60
    EXPECT_EQ(midi_.noteOnCount(), 1);
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    press(0x50, 0);   // Left -> add9
    EXPECT_EQ(midi_.noteOffCount(), 0);            // current note not released
    EXPECT_EQ(midi_.noteOnCount(), 1);             // no re-attack
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    // The arp now cycles four notes (root-position + the 9th), stepping on the
    // master-clock 16th boundaries.
    state_.rhythmClock.tickAbs = 6;
    engine_->update(0);
    EXPECT_TRUE(state_.isNoteActive(1, 64));
    state_.rhythmClock.tickAbs = 12;
    engine_->update(0);
    EXPECT_TRUE(state_.isNoteActive(1, 67));
    state_.rhythmClock.tickAbs = 18;
    engine_->update(0);
    EXPECT_TRUE(state_.isNoteActive(1, 74));       // the added 9th
}

// Switching arp mode out of Random while an arpeggio is running must rebuild the
// (empty) step sequence, otherwise stepArpeggio would divide by zero and wedge.
TEST_F(ChordEngineTest, ArpModeChangeOutOfRandomRebuildsSequence) {
    state_.pendingChord.play_mode = PlayMode::Arpeggio;
    state_.pendingChord.arp_mode  = ArpMode::Random;
    press(0x17, 0);   // C major; Random starts on some note
    EXPECT_EQ(midi_.noteOnCount(), 1);

    state_.pendingChord.arp_mode = ArpMode::Up;    // simulate an edit-engine change
    engine_->onArpModeChanged();

    state_.rhythmClock.tickAbs = 6;                // step -> next note (64)
    engine_->update(0);
    EXPECT_TRUE(state_.isNoteActive(1, 64));
    state_.rhythmClock.tickAbs = 12;               // -> 67
    engine_->update(0);
    EXPECT_TRUE(state_.isNoteActive(1, 67));
}

// Voicing reset clears the voice-leading memory: the next chord returns to its
// root-position voicing instead of voice-leading from the previous chord.
TEST_F(ChordEngineTest, ResetVoicingReturnsToRootPosition) {
    state_.pendingChord.voicing_mode = VoicingMode::Smart;

    press(0x17, 0);   // C major -> root {60,64,67}
    EXPECT_TRUE(state_.isNoteActive(1, 60));
    EXPECT_TRUE(state_.isNoteActive(1, 64));
    EXPECT_TRUE(state_.isNoteActive(1, 67));

    release(0x17, 0);  // release (Held latches)

    press(0x15, 0);   // F major -> smart {60,65,69}
    EXPECT_TRUE(state_.isNoteActive(1, 60));
    EXPECT_TRUE(state_.isNoteActive(1, 65));
    EXPECT_TRUE(state_.isNoteActive(1, 69));

    engine_->resetVoicing();
    release(0x15, 0);

    press(0x17, 0);   // C major -> root again
    EXPECT_TRUE(state_.isNoteActive(1, 60));
    EXPECT_TRUE(state_.isNoteActive(1, 64));
    EXPECT_TRUE(state_.isNoteActive(1, 67));
    EXPECT_FALSE(state_.isNoteActive(1, 69));
}

// A mapped voicing edit updates pending params and notifies the engine. The
// next live trigger must use the new mode (FR-C9: the sounding chord is
// untouched, but the next trigger re-snapshots).
TEST_F(ChordEngineTest, VoicingUpAppliesOnNextTrigger) {
    press(0x17, 0);    // C major root {60,64,67}
    release(0x17, 0);  // Held latches

    state_.pendingChord.voicing_mode = VoicingMode::Up;
    engine_->onVoicingChanged();

    midi_.clear();
    press(0x1C, 0);    // G major -> nearest voicing above C
    EXPECT_EQ(noteOnNotes(), (std::vector<uint8_t>{62, 67, 71}));
}

// Even when the next trigger resolves to the same chord through a non-forced
// path, a pending voicing change forces a re-trigger so the new mode is heard.
TEST_F(ChordEngineTest, VoicingChangeForcesSameChordReTrigger) {
    press(0x17, 0);    // C major root
    release(0x17, 0);

    state_.pendingChord.voicing_mode = VoicingMode::Down;
    engine_->onVoicingChanged();
    midi_.clear();

    press(0x17, 0);    // same chord; fresh presses already force a trigger
    EXPECT_EQ(noteOnNotes(), (std::vector<uint8_t>{55, 60, 64}));
}

// The voice-leading reference is the unpadded chord core. With min_notes above
// the triad size, Up/Down must still move instead of falling back to root.
TEST_F(ChordEngineTest, DirectionalVoicingIgnoresMinNotesPadding) {
    state_.pendingChord.min_notes = 4;
    press(0x17, 0);    // C major {60,64,67,72}
    release(0x17, 0);

    state_.pendingChord.voicing_mode = VoicingMode::Down;
    engine_->onVoicingChanged();
    midi_.clear();

    press(0x1C, 0);    // G major; previous core is C {60,64,67}
    EXPECT_EQ(noteOnNotes(), (std::vector<uint8_t>{59, 62, 67, 71}));
}

// --- Chord patterns (M13) --------------------------------------------------

namespace {

// Alberti demo: root-5th-3rd-5th on quarter-note beats (spec 6.10).
ChordPattern albertiPattern() {
    ChordPattern p;
    p.name = "Alberti";
    p.steps_per_bar = 16;
    p.steps = {1,0,0,0, 4,0,0,0, 2,0,0,0, 4,0,0,0};
    return p;
}

// Block: full chord (0x0F) held for the bar.
ChordPattern blockPattern() {
    ChordPattern p;
    p.name = "Block";
    p.steps_per_bar = 16;
    p.steps = {15,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};
    p.sustain_steps = {16,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};
    return p;
}

} // namespace

// Pattern mode resolves degrees to chord tones and steps on the rhythm clock.
TEST_F(ChordEngineTest, PatternAlbertiPlaysRootFifthThirdFifth) {
    engine_->setPatterns({albertiPattern()});
    state_.pendingChord.play_mode = PlayMode::Pattern;
    state_.pendingChord.pattern = 0;
    state_.rhythmClock.running = true;
    state_.rhythmClock.stepsPerBar = 16;
    state_.rhythmClock.stepAbs = 0;

    press(0x17, 0);   // C major -> step 0 = root 60
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    // Advance one 16th step at a time to beat 2 (step 4 = 5th 67).
    for (uint32_t s = 1; s <= 4; s++) {
        state_.rhythmClock.stepAbs = s;
        engine_->update(0);
    }
    EXPECT_TRUE(state_.isNoteActive(1, 67));
    EXPECT_FALSE(state_.isNoteActive(1, 60));

    // Beat 3 (step 8 = 3rd 64).
    for (uint32_t s = 5; s <= 8; s++) {
        state_.rhythmClock.stepAbs = s;
        engine_->update(0);
    }
    EXPECT_TRUE(state_.isNoteActive(1, 64));
    EXPECT_FALSE(state_.isNoteActive(1, 67));

    // Beat 4 (step 12 = 5th 67).
    for (uint32_t s = 9; s <= 12; s++) {
        state_.rhythmClock.stepAbs = s;
        engine_->update(0);
    }
    EXPECT_TRUE(state_.isNoteActive(1, 67));
    EXPECT_FALSE(state_.isNoteActive(1, 64));
}

// A multi-note step fires all its degrees together (polyphonic).
TEST_F(ChordEngineTest, PatternMultiNoteStepFiresChord) {
    ChordPattern p;
    p.name = "Triad";
    p.steps_per_bar = 16;
    p.steps = {7,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};  // root+3rd+5th on step 0
    engine_->setPatterns({p});
    state_.pendingChord.play_mode = PlayMode::Pattern;
    state_.pendingChord.pattern = 0;

    press(0x17, 0);  // C major
    EXPECT_TRUE(state_.isNoteActive(1, 60));
    EXPECT_TRUE(state_.isNoteActive(1, 64));
    EXPECT_TRUE(state_.isNoteActive(1, 67));
}

// Degree 3 wraps to the octave root on a triad, but plays the 7th on a 7th chord.
TEST_F(ChordEngineTest, PatternDegreeThreeWrapsOnTriad) {
    ChordPattern p;
    p.name = "SeventhBit";
    p.steps_per_bar = 16;
    p.steps = {8,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};  // bit 3 = 7th degree
    engine_->setPatterns({p});
    state_.pendingChord.play_mode = PlayMode::Pattern;
    state_.pendingChord.pattern = 0;

    press(0x17, 0);   // C major -> octave root 72
    EXPECT_TRUE(state_.isNoteActive(1, 72));
    engine_->allNotesOff();

    press(0x05, 0);   // Cmaj7 (T+B) -> 7th = 71
    EXPECT_TRUE(state_.isNoteActive(1, 71));
}

// Pattern advances at the stored tempo when the rhythm clock is stopped.
TEST_F(ChordEngineTest, PatternFreeRunsAtStoredTempo) {
    engine_->setPatterns({albertiPattern()});
    state_.pendingChord.play_mode = PlayMode::Pattern;
    state_.pendingChord.pattern = 0;
    state_.pendingRhythm.tempo = 60;  // stepUs(60) = 250000 us

    press(0x17, 0);  // step 0 = root 60
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    engine_->update(265000);   // step 1 = rest
    engine_->update(515000);   // step 2 = rest
    engine_->update(765000);   // step 3 = rest
    engine_->update(1015000);  // step 4 = 5th 67
    EXPECT_TRUE(state_.isNoteActive(1, 67));
}

// PatternHold latches on release; Pattern is momentary.
TEST_F(ChordEngineTest, PatternHoldLatchesPatternIsMomentary) {
    engine_->setPatterns({albertiPattern()});
    state_.pendingChord.play_mode = PlayMode::Pattern;
    state_.pendingChord.pattern = 0;
    state_.rhythmClock.running = true;
    state_.rhythmClock.stepsPerBar = 16;
    state_.rhythmClock.stepAbs = 0;

    press(0x17, 0);
    release(0x17, 0);   // release: momentary stops
    engine_->update(30000);                          // release buffer expires
    EXPECT_FALSE(state_.isNoteActive(1, 60));

    state_.pendingChord.play_mode = PlayMode::PatternHold;
    press(0x17, 0);
    release(0x17, 0);   // release: latches
    engine_->update(30000);
    EXPECT_TRUE(state_.isNoteActive(1, 60));
}

// Block pattern sustains the full chord across the whole bar (past one step).
TEST_F(ChordEngineTest, PatternBlockSustainsBar) {
    engine_->setPatterns({blockPattern()});
    state_.pendingChord.play_mode = PlayMode::Pattern;
    state_.pendingChord.pattern = 0;
    state_.rhythmClock.running = true;
    state_.rhythmClock.stepsPerBar = 16;
    state_.rhythmClock.stepAbs = 0;

    press(0x17, 0);  // full chord on step 0
    EXPECT_TRUE(state_.isNoteActive(1, 60));
    EXPECT_TRUE(state_.isNoteActive(1, 64));
    EXPECT_TRUE(state_.isNoteActive(1, 67));

    // Advance through several rest steps: the chord keeps sustaining.
    for (int s = 1; s <= 8; s++) {
        state_.rhythmClock.stepAbs = static_cast<uint32_t>(s);
        engine_->update(0);
    }
    EXPECT_TRUE(state_.isNoteActive(1, 60));
    EXPECT_TRUE(state_.isNoteActive(1, 64));
    EXPECT_TRUE(state_.isNoteActive(1, 67));
}

TEST_F(ChordEngineTest, InvalidPatternIndexRecovers) {
    engine_->setPatterns({albertiPattern()});
    state_.pendingChord.play_mode = PlayMode::Pattern;
    state_.pendingChord.pattern = 9;  // out of range
    state_.rhythmClock.running = true;
    state_.rhythmClock.stepsPerBar = 16;
    state_.rhythmClock.stepAbs = 0;

    press(0x17, 0);  // C major -> recovered pattern step 0 = root 60
    EXPECT_EQ(state_.pendingChord.pattern, 0);
    EXPECT_TRUE(state_.isNoteActive(1, 60));
}

TEST_F(ChordEngineTest, ZeroStepsPerBarDoesNotStallPattern) {
    ChordPattern broken = albertiPattern();
    broken.steps_per_bar = 0;
    engine_->setPatterns({broken});
    state_.pendingChord.play_mode = PlayMode::Pattern;
    state_.pendingChord.pattern = 0;
    state_.rhythmClock.running = true;
    state_.rhythmClock.stepsPerBar = 16;
    state_.rhythmClock.stepAbs = 0;

    press(0x17, 0);
    EXPECT_TRUE(state_.isNoteActive(1, 60));

    for (uint32_t s = 1; s <= 4; s++) {
        state_.rhythmClock.stepAbs = s;
        engine_->update(0);
    }
    EXPECT_TRUE(state_.isNoteActive(1, 67));  // step 4 = 5th, despite bad meter
}

TEST_F(ChordEngineTest, PatternStartWaitsForNearRhythmStep) {
    engine_->setPatterns({albertiPattern()});
    state_.pendingChord.play_mode = PlayMode::Pattern;
    state_.pendingChord.pattern = 0;
    state_.rhythmClock.running = true;
    state_.rhythmClock.stepsPerBar = 16;
    state_.rhythmClock.stepAbs = 5;
    state_.rhythmClock.nextStepUs = 20000;

    press(0x17, 0);  // begins at 15 ms, 5 ms before the rhythm step
    EXPECT_EQ(midi_.noteOnCount(), 0);

    engine_->update(20000);
    EXPECT_TRUE(state_.isNoteActive(1, 60));
}

TEST_F(ChordEngineTest, PatternStartAlwaysWaitsForNextRhythmStep) {
    engine_->setPatterns({albertiPattern()});
    state_.pendingChord.play_mode = PlayMode::Pattern;
    state_.pendingChord.pattern = 0;
    state_.rhythmClock.running = true;
    state_.rhythmClock.stepsPerBar = 16;
    state_.rhythmClock.stepAbs = 5;
    state_.rhythmClock.nextStepUs = 100000;

    press(0x17, 0);  // begins at 15 ms; still waits for step 100 ms away
    EXPECT_EQ(midi_.noteOnCount(), 0);

    engine_->update(99999);
    EXPECT_EQ(midi_.noteOnCount(), 0);

    engine_->update(100000);
    EXPECT_TRUE(state_.isNoteActive(1, 60));
}
