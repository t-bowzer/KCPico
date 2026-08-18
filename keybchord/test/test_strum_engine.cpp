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

// A tap (press then release) sounds for the minimum duration.
TEST_F(StrumEngineTest, NoteOffAfterDuration) {
    chord_->handleKeyEvent(key(0x17, true), 0);  // C major

    strum_->handleKeyEvent(key(0x62, true), 0);   // Keypad 0 press
    strum_->handleKeyEvent(key(0x62, false), 0);  // release (tap)
    EXPECT_EQ(lastNoteOnOnChannel(2), 72);
    EXPECT_EQ(noteOffCountOnChannel(2), 0);

    strum_->update(300000);  // note_duration_ms = 300 -> 300000 us
    EXPECT_EQ(noteOffCountOnChannel(2), 1);
    EXPECT_FALSE(state_.isNoteActive(2, 72));
}

// Duration is a minimum: a held key sustains past the duration.
TEST_F(StrumEngineTest, HeldKeySustainsPastDuration) {
    state_.pendingStrum.note_duration_ms = 300;
    chord_->handleKeyEvent(key(0x17, true), 0);  // C major

    strum_->handleKeyEvent(key(0x62, true), 0);  // press, held
    EXPECT_TRUE(state_.isNoteActive(2, 72));

    strum_->update(400000);  // past the 300ms duration, still held
    EXPECT_TRUE(state_.isNoteActive(2, 72));
    EXPECT_EQ(noteOffCountOnChannel(2), 0);

    strum_->handleKeyEvent(key(0x62, false), 400000);  // release after deadline
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

// Held extensions (FR-C7) are picked up immediately by the strum pool via the
// chord engine's merged selected chord.
TEST_F(StrumEngineTest, ImmediatePickupOfExtensions) {
    chord_->handleKeyEvent(key(0x17, true), 0);  // C major {0,4,7}
    strum_->handleKeyEvent(key(0x63, true), 0);  // Keypad . -> pool[1] = 76
    EXPECT_EQ(lastNoteOnOnChannel(2), 76);

    chord_->handleKeyEvent(key(0x50, true), 0);  // Left arrow -> add9 held
    strum_->handleKeyEvent(key(0x63, true), 0);  // pool {0,2,4,7}: pool[1] = 74
    EXPECT_EQ(lastNoteOnOnChannel(2), 74);
}

// Scale mode: a selectable root + scale/mode drives the pool instead of the
// chord's pitch classes.
TEST_F(StrumEngineTest, ScaleModePool) {
    state_.pendingStrum.mode = StrumMode::Scale;
    state_.pendingStrum.scale_type = ScaleType::Ionian;
    state_.pendingStrum.root_pc = 0;   // C
    state_.pendingStrum.octave = 1;    // anchor 72

    strum_->handleKeyEvent(key(0x62, true), 0);  // pool[0] = 72 (C)
    EXPECT_EQ(lastNoteOnOnChannel(2), 72);

    strum_->handleKeyEvent(key(0x63, true), 0);  // pool[1] = 74 (D)
    EXPECT_EQ(lastNoteOnOnChannel(2), 74);

    strum_->handleKeyEvent(key(0x59, true), 0);  // pool[2] = 76 (E)
    EXPECT_EQ(lastNoteOnOnChannel(2), 76);
}

// Piano mode: chromatic — each strum key is one semitone higher.
TEST_F(StrumEngineTest, PianoModePool) {
    state_.pendingStrum.mode = StrumMode::Piano;
    state_.pendingStrum.root_pc = 0;
    state_.pendingStrum.octave = 1;    // anchor 72

    strum_->handleKeyEvent(key(0x62, true), 0);  // pool[0] = 72
    EXPECT_EQ(lastNoteOnOnChannel(2), 72);

    strum_->handleKeyEvent(key(0x63, true), 0);  // pool[1] = 73
    EXPECT_EQ(lastNoteOnOnChannel(2), 73);
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

    ResolvedChord c;
    c.type = ChordType::Major;
    c.rootPc = 0;
    state_.selectedChord = c;
    state_.selectedChordValid = true;

    // 21 distinct strum usages (number row 1..0 + numpad 0 . 1..9), all held:
    // exceeds the 16-slot buffer, forcing eviction.
    const uint8_t usages[21] = {
        0x1E,0x1F,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,  // number row
        0x62,0x63,0x59,0x5A,0x5B,0x5C,0x5D,0x5E,0x5F,0x60,0x61,  // numpad
    };
    for (uint8_t u : usages) strum_->handleKeyEvent(key(u, true), 0);

    strum_->allNotesOff();  // release everything

    int on = 0, off = 0;
    for (const auto& m : midi_.messages()) {
        if ((m.status & 0x0F) == 1) {  // channel 2
            if ((m.status & 0xF0) == midi::STATUS_NOTE_ON && m.data2 > 0) on++;
            if ((m.status & 0xF0) == midi::STATUS_NOTE_OFF) off++;
        }
    }
    EXPECT_EQ(on, 21);
    EXPECT_EQ(off, 21);  // every note-on paired with a note-off
    EXPECT_TRUE(state_.activeNotes.empty());
}

// Re-strumming a still-sounding note must re-articulate: note-off precedes the
// new note-on, so no un-paired duplicate note-on reaches the synth.
TEST_F(StrumEngineTest, RetriggerRearticulatesWithNoteOffFirst) {
    chord_->handleKeyEvent(key(0x17, true), 0);  // C major
    strum_->handleKeyEvent(key(0x59, true), 0);  // numpad 1 -> note 79 (on)

    midi_.clear();
    strum_->handleKeyEvent(key(0x59, true), 1000000);  // re-strum

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
