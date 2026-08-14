#include "strum_engine_fixture.h"


// Full layout: strumming the number row plays the active chord's note pool.
TEST_F(StrumEngineTest, NumberRowPlaysChordPool) {
    chord_->handleKeyEvent(key(0x17, true), 0);  // T = C major (sets selectedChord)

    strum_->handleKeyEvent(key(0x1E, true), 0);  // number row 1 -> pool[0]
    EXPECT_EQ(lastNoteOnOnChannel(2), 72);       // anchor 72 (default strum octave 1)

    strum_->handleKeyEvent(key(0x27, true), 0);  // number row 0 -> pool[9]
    EXPECT_EQ(lastNoteOnOnChannel(2), 108);
}

// Full layout numpad ordering: 0 . 1 2 ... 9.
TEST_F(StrumEngineTest, NumpadFullOrdering) {
    chord_->handleKeyEvent(key(0x17, true), 0);  // C major

    strum_->handleKeyEvent(key(0x62, true), 0);  // Keypad 0 -> pool[0] = 72
    EXPECT_EQ(lastNoteOnOnChannel(2), 72);

    strum_->handleKeyEvent(key(0x63, true), 0);  // Keypad . -> pool[1] = 76
    EXPECT_EQ(lastNoteOnOnChannel(2), 76);

    strum_->handleKeyEvent(key(0x59, true), 0);  // Keypad 1 -> pool[2] = 79
    EXPECT_EQ(lastNoteOnOnChannel(2), 79);

    strum_->handleKeyEvent(key(0x61, true), 0);  // Keypad 9 -> pool[10] = 112
    EXPECT_EQ(lastNoteOnOnChannel(2), 112);
}

// Limited layout: 0 . 2 3 5 6 8 9 / * (number row excluded).
TEST_F(StrumEngineTest, LimitedLayoutOrdering) {
    state_.pendingStrum.limited_keys = true;
    chord_->handleKeyEvent(key(0x17, true), 0);  // C major

    strum_->handleKeyEvent(key(0x62, true), 0);  // 0 -> pool[0] = 72
    EXPECT_EQ(lastNoteOnOnChannel(2), 72);

    strum_->handleKeyEvent(key(0x54, true), 0);  // / -> pool[8] = 103
    EXPECT_EQ(lastNoteOnOnChannel(2), 103);

    strum_->handleKeyEvent(key(0x55, true), 0);  // * -> pool[9] = 108
    EXPECT_EQ(lastNoteOnOnChannel(2), 108);

    // Number row is not a strum key in limited mode.
    midi_.clear();
    strum_->handleKeyEvent(key(0x1E, true), 0);
    EXPECT_EQ(lastNoteOnOnChannel(2), -1);
}

// Strummed notes release after note_duration_ms.
TEST_F(StrumEngineTest, NoteOffAfterDuration) {
    chord_->handleKeyEvent(key(0x17, true), 0);  // C major

    strum_->handleKeyEvent(key(0x62, true), 0);  // Keypad 0
    EXPECT_EQ(lastNoteOnOnChannel(2), 72);
    EXPECT_EQ(noteOffCountOnChannel(2), 0);

    strum_->update(300000);  // note_duration_ms = 300 -> 300000 us
    EXPECT_EQ(noteOffCountOnChannel(2), 1);
    EXPECT_FALSE(state_.isNoteActive(2, 72));
}

// FR-S5: strum picks up parameter edits immediately, with no latching.
TEST_F(StrumEngineTest, ImmediatePickupOfStrumOctave) {
    chord_->handleKeyEvent(key(0x17, true), 0);  // C major
    strum_->handleKeyEvent(key(0x62, true), 0);  // pool[0] = 72 (octave 1)
    EXPECT_EQ(lastNoteOnOnChannel(2), 72);

    state_.pendingStrum.octave = 0;              // edit octave immediately
    strum_->handleKeyEvent(key(0x62, true), 0);  // pool[0] = 60
    EXPECT_EQ(lastNoteOnOnChannel(2), 60);
}

// Strum notes use the strum velocity and channel.
TEST_F(StrumEngineTest, StrumVelocityAndChannel) {
    state_.pendingStrum.velocity = 120;
    state_.pendingStrum.channel = 5;

    chord_->handleKeyEvent(key(0x17, true), 0);  // C major
    strum_->handleKeyEvent(key(0x62, true), 0);

    int vel = -1;
    for (const auto& m : midi_.messages()) {
        if ((m.status & 0xF0) == midi::STATUS_NOTE_ON && m.data2 > 0
            && (m.status & 0x0F) == 4) {  // channel 5 (0-indexed 4)
            vel = m.data2;
        }
    }
    EXPECT_EQ(lastNoteOnOnChannel(5), 72);
    EXPECT_EQ(vel, 120);
}

// No chord selected yet -> strum produces nothing.
TEST_F(StrumEngineTest, NoChordNoStrum) {
    strum_->handleKeyEvent(key(0x62, true), 0);
    EXPECT_EQ(lastNoteOnOnChannel(2), -1);
}

// FR-S5: extension toggles are picked up immediately by the strum pool.
TEST_F(StrumEngineTest, ImmediatePickupOfExtensions) {
    chord_->handleKeyEvent(key(0x17, true), 0);  // C major {0,4,7}
    strum_->handleKeyEvent(key(0x63, true), 0);  // Keypad . -> pool[1] = 76
    EXPECT_EQ(lastNoteOnOnChannel(2), 76);

    chord_->handleKeyEvent(key(0x50, true), 0);  // Left arrow -> add9 on
    strum_->handleKeyEvent(key(0x63, true), 0);  // pool now {0,2,4,7}: pool[1] = 74
    EXPECT_EQ(lastNoteOnOnChannel(2), 74);
}

// Alt+F5 toggles limited keys.
TEST_F(StrumEngineTest, AltF5TogglesLimitedKeys) {
    EXPECT_FALSE(state_.pendingStrum.limited_keys);
    strum_->handleKeyEvent(key(0x3E, true, 0x04), 0);  // Alt+F5
    EXPECT_TRUE(state_.pendingStrum.limited_keys);
    strum_->handleKeyEvent(key(0x3E, true, 0x04), 0);  // Alt+F5
    EXPECT_FALSE(state_.pendingStrum.limited_keys);
}

// Alt+F2 arms strum-octave editing; +/- steps it, clamped to [-3, +3].
TEST_F(StrumEngineTest, EditTargetStepsStrumOctave) {
    strum_->handleKeyEvent(key(0x3B, true, 0x04), 0);  // Alt+F2 -> arm strum octave
    EXPECT_EQ(state_.editTarget, EditTarget::StrumOctave);

    strum_->handleKeyEvent(key(0x2E, true), 0);  // + -> octave 1 -> 2
    EXPECT_EQ(state_.pendingStrum.octave, 2);

    strum_->handleKeyEvent(key(0x2D, true), 0);  // - -> octave 2 -> 1
    EXPECT_EQ(state_.pendingStrum.octave, 1);

    state_.pendingStrum.octave = 3;
    strum_->handleKeyEvent(key(0x2E, true), 0);  // clamp at +3
    EXPECT_EQ(state_.pendingStrum.octave, 3);

    state_.pendingStrum.octave = -3;
    strum_->handleKeyEvent(key(0x2D, true), 0);  // clamp at -3
    EXPECT_EQ(state_.pendingStrum.octave, -3);
}

// Esc clears the edit target; +/- then returns to chord octave.
TEST_F(StrumEngineTest, EscClearsEditTarget) {
    strum_->handleKeyEvent(key(0x3B, true, 0x04), 0);  // Alt+F2
    EXPECT_NE(state_.editTarget, EditTarget::None);

    strum_->handleKeyEvent(key(0x29, true), 0);  // Esc
    EXPECT_EQ(state_.editTarget, EditTarget::None);
}

// When a strum edit is armed, +/- must not also change chord octave.
TEST_F(StrumEngineTest, ChordOctaveUnaffectedWhileStrumEditArmed) {
    strum_->handleKeyEvent(key(0x3B, true, 0x04), 0);  // Alt+F2 -> arm strum octave

    chord_->handleKeyEvent(key(0x2E, true), 0);  // + (both engines would see it)
    strum_->handleKeyEvent(key(0x2E, true), 0);

    EXPECT_EQ(state_.pendingChord.octave, 0);       // chord octave untouched
    EXPECT_EQ(state_.pendingStrum.octave, 2);       // strum octave stepped
}

// Keypad +/- are not strum keys: they never produce a strum note.
TEST_F(StrumEngineTest, NumpadPlusMinusNotStrum) {
    chord_->handleKeyEvent(key(0x17, true), 0);  // C major

    strum_->handleKeyEvent(key(0x57, true), 0);  // Keypad +
    strum_->handleKeyEvent(key(0x56, true), 0);  // Keypad -
    EXPECT_EQ(lastNoteOnOnChannel(2), -1);
}

// Regression: strumming more distinct notes than the internal buffer holds must
// not leave any note stuck (evicted slots must emit their note-off first).
TEST_F(StrumEngineTest, NoStuckNotesWhenBufferOverflows) {
    state_.pendingStrum.note_duration_ms = 4000;

    const uint8_t fullNumpad[11] = {0x62, 0x63, 0x59, 0x5A, 0x5B, 0x5C,
                                    0x5D, 0x5E, 0x5F, 0x60, 0x61};

    ResolvedChord c;
    c.type = ChordType::Major;

    // C major pool (11 distinct notes) then Db major pool (11 disjoint notes):
    // 22 concurrent notes exceed the 16-slot buffer.
    c.rootPc = 0;
    state_.selectedChord = c;
    state_.selectedChordValid = true;
    for (uint8_t u : fullNumpad) strum_->handleKeyEvent(key(u, true), 0);

    c.rootPc = 1;
    state_.selectedChord = c;
    for (uint8_t u : fullNumpad) strum_->handleKeyEvent(key(u, true), 0);

    strum_->update(5000000);  // advance well past all deadlines

    int on = 0, off = 0;
    for (const auto& m : midi_.messages()) {
        if ((m.status & 0x0F) == 1) {  // channel 2
            if ((m.status & 0xF0) == midi::STATUS_NOTE_ON && m.data2 > 0) on++;
            if ((m.status & 0xF0) == midi::STATUS_NOTE_OFF) off++;
        }
    }
    EXPECT_EQ(on, 22);
    EXPECT_EQ(off, 22);  // no stuck notes
}

// Duration decrease must apply to a note re-triggered while still sounding.
TEST_F(StrumEngineTest, DurationDecreaseAppliesToRetriggeredNote) {
    state_.pendingStrum.note_duration_ms = 4000;  // long
    chord_->handleKeyEvent(key(0x17, true), 0);   // C major

    strum_->handleKeyEvent(key(0x59, true), 0);   // numpad 1 -> note 79
    EXPECT_TRUE(state_.isNoteActive(2, 79));

    state_.pendingStrum.note_duration_ms = 300;   // decrease while sounding

    strum_->handleKeyEvent(key(0x59, true), 1000000);  // re-play at t=1s

    // 300ms after the re-press (t=1.3s) the note must be off.
    strum_->update(1300000);
    EXPECT_FALSE(state_.isNoteActive(2, 79));
}

// Same but using the actual Alt+F3 / +/- edit mechanism, and re-playing after
// the original note has fully faded out (free-slot path).
TEST_F(StrumEngineTest, DurationEditAppliesToReplayedNoteAfterFadeOut) {
    chord_->handleKeyEvent(key(0x17, true), 0);  // C major

    // Arm strum duration edit and push it up to 1000ms.
    strum_->handleKeyEvent(key(0x3C, true, 0x04), 0);  // Alt+F3
    for (int i = 0; i < 14; i++) strum_->handleKeyEvent(key(0x2E, true), 0);  // +
    EXPECT_EQ(state_.pendingStrum.note_duration_ms, 1000);

    // Play numpad 1 (note 79) at t=0, then let it fade out.
    strum_->handleKeyEvent(key(0x59, true), 0);
    EXPECT_TRUE(state_.isNoteActive(2, 79));
    strum_->update(1000000);  // t=1s >= 1000ms -> note-off
    EXPECT_FALSE(state_.isNoteActive(2, 79));

    // Decrease duration back to 300ms.
    strum_->handleKeyEvent(key(0x3C, true, 0x04), 2000000);  // Alt+F3 (no-op re-arm)
    for (int i = 0; i < 14; i++) strum_->handleKeyEvent(key(0x2D, true), 2000000);  // -
    EXPECT_EQ(state_.pendingStrum.note_duration_ms, 300);

    // Re-play numpad 1 (note 79) at t=2.5s; it must release 300ms later.
    strum_->handleKeyEvent(key(0x59, true), 2500000);
    EXPECT_TRUE(state_.isNoteActive(2, 79));

    strum_->update(2800000);  // t=2.8s
    EXPECT_FALSE(state_.isNoteActive(2, 79));
}

// Re-strumming a still-sounding note must re-articulate: note-off precedes the
// new note-on, so no un-paired duplicate note-on reaches the synth.
TEST_F(StrumEngineTest, RetriggerRearticulatesWithNoteOffFirst) {
    chord_->handleKeyEvent(key(0x17, true), 0);  // C major
    strum_->handleKeyEvent(key(0x59, true), 0);  // numpad 1 -> note 79 (on)

    midi_.clear();
    strum_->handleKeyEvent(key(0x59, true), 1000000);  // re-strum

    // Channel-2 messages for note 79, in order.
    std::vector<std::pair<bool, bool>> ops;  // (isNoteOn, isNote79)
    for (const auto& m : midi_.messages()) {
        if ((m.status & 0x0F) != 1) continue;  // channel 2
        bool isOn = (m.status & 0xF0) == midi::STATUS_NOTE_ON && m.data2 > 0;
        ops.push_back({isOn, m.data1 == 79});
    }
    ASSERT_EQ(ops.size(), 2u);
    EXPECT_TRUE(ops[0].second);   // first is note 79 ...
    EXPECT_FALSE(ops[0].first);   // ... a note-off (re-articulation)
    EXPECT_TRUE(ops[1].second);   // then note 79 ...
    EXPECT_TRUE(ops[1].first);    // ... a note-on
}

// Regression (M4 open bug): numpad 0 and . (the two lowest pool notes) must
// release after a 50ms duration set through the real Alt+F3 / - edit path.
TEST_F(StrumEngineTest, NumpadZeroAndDecimalReleaseAtMinDuration) {
    chord_->handleKeyEvent(key(0x17, true), 0);  // C major

    // Arm duration edit and step 300 -> 50 (five -50 steps).
    strum_->handleKeyEvent(key(0x3C, true, 0x04), 0);  // Alt+F3
    for (int i = 0; i < 5; i++) strum_->handleKeyEvent(key(0x2D, true), 0);  // -
    EXPECT_EQ(state_.pendingStrum.note_duration_ms, 50);

    // Numpad 0 -> pool[0] = 72.
    strum_->handleKeyEvent(key(0x62, true), 0);
    EXPECT_EQ(lastNoteOnOnChannel(2), 72);
    EXPECT_TRUE(state_.isNoteActive(2, 72));

    // Numpad . -> pool[1] = 76.
    strum_->handleKeyEvent(key(0x63, true), 0);
    EXPECT_EQ(lastNoteOnOnChannel(2), 76);
    EXPECT_TRUE(state_.isNoteActive(2, 76));

    // 50ms later both must be off.
    strum_->update(50000);
    EXPECT_FALSE(state_.isNoteActive(2, 72));
    EXPECT_FALSE(state_.isNoteActive(2, 76));
}


