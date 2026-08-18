#include <gtest/gtest.h>

#include <vector>

#include "bass_engine.h"
#include "midi_router.h"
#include "recording_midi_out.h"
#include "state.h"


class BassEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        midi_.begin();
        state_.loadDefaults();
        router_ = new MidiRouter(midi_, state_);
        bass_   = new BassEngine(state_, *router_);

        // Select C major and enable the walking bass on channel 3, octave -1.
        ResolvedChord c{0, ChordType::Major};
        state_.selectedChord = c;
        state_.selectedChordValid = true;
        state_.pendingBass.enabled = true;
        state_.pendingBass.channel = 3;
        state_.pendingBass.octave  = -1;
        state_.pendingBass.note_duration_ms = 150;
        state_.pendingBass.velocity = 90;
        state_.pendingChord.play_mode = PlayMode::Held;
    }

    void TearDown() override {
        delete bass_;
        delete router_;
    }

    int lastNoteOn(uint8_t channel) const {
        int note = -1;
        for (const auto& m : midi_.messages()) {
            if ((m.status & 0xF0) == midi::STATUS_NOTE_ON && m.data2 > 0
                && (m.status & 0x0F) == (channel - 1)) {
                note = m.data1;
            }
        }
        return note;
    }

    int noteOffCount(uint8_t channel) const {
        int n = 0;
        for (const auto& m : midi_.messages()) {
            if ((m.status & 0xF0) == midi::STATUS_NOTE_OFF
                && (m.status & 0x0F) == (channel - 1)) n++;
        }
        return n;
    }

    RecordingMidiOutAdapter midi_;
    StateManager state_;
    MidiRouter* router_ = nullptr;
    BassEngine* bass_ = nullptr;
};


// Walking bass cycles root-3rd-5th-6th on the four beats of a 4/4 bar.
TEST_F(BassEngineTest, WalksFourBeatCycle) {
    state_.rhythmClock.running = true;
    state_.rhythmClock.stepsPerBar = 16;
    state_.rhythmClock.step = 0;
    state_.rhythmClock.stepAbs = 0;
    state_.rhythmClock.beat = 0;
    bass_->update(0);   // beat 0 -> root C3 = 48
    EXPECT_EQ(lastNoteOn(3), 48);

    state_.rhythmClock.step = 4;
    state_.rhythmClock.stepAbs = 4;
    state_.rhythmClock.beat = 1;
    bass_->update(0);   // beat 1 -> 3rd = 52
    EXPECT_EQ(lastNoteOn(3), 52);

    state_.rhythmClock.step = 8;
    state_.rhythmClock.stepAbs = 8;
    state_.rhythmClock.beat = 2;
    bass_->update(0);   // beat 2 -> 5th = 55
    EXPECT_EQ(lastNoteOn(3), 55);

    state_.rhythmClock.step = 12;
    state_.rhythmClock.stepAbs = 12;
    state_.rhythmClock.beat = 3;
    bass_->update(0);   // beat 3 -> 6th = 57
    EXPECT_EQ(lastNoteOn(3), 57);
}

// Bass is silent while the rhythm is not running (FR-B4).
TEST_F(BassEngineTest, SilentWhenRhythmStopped) {
    state_.rhythmClock.running = false;
    bass_->update(0);
    EXPECT_EQ(lastNoteOn(3), -1);
}

// Bass is silent when disabled (FR-B1).
TEST_F(BassEngineTest, SilentWhenDisabled) {
    state_.pendingBass.enabled = false;
    state_.rhythmClock.running = true;
    bass_->update(0);
    EXPECT_EQ(lastNoteOn(3), -1);
}

// Bass is silent in Silent chord mode (FR-B1).
TEST_F(BassEngineTest, SilentInSilentChordMode) {
    state_.pendingChord.play_mode = PlayMode::Silent;
    state_.rhythmClock.running = true;
    bass_->update(0);
    EXPECT_EQ(lastNoteOn(3), -1);
}

// Percussive note-off after note_duration_ms.
TEST_F(BassEngineTest, NoteOffAfterDuration) {
    state_.rhythmClock.running = true;
    state_.rhythmClock.step = 0;
    state_.rhythmClock.stepAbs = 0;
    state_.rhythmClock.beat = 0;
    bass_->update(0);
    EXPECT_EQ(noteOffCount(3), 0);

    bass_->update(150000);   // 150 ms later
    EXPECT_EQ(noteOffCount(3), 1);
}

// A new beat releases the previous note (monophonic walking line).
TEST_F(BassEngineTest, NewBeatReleasesPrevious) {
    state_.rhythmClock.running = true;
    state_.rhythmClock.step = 0;
    state_.rhythmClock.stepAbs = 0;
    state_.rhythmClock.beat = 0;
    bass_->update(0);

    // Fire beat 1 before the previous note's duration elapses.
    state_.rhythmClock.step = 4;
    state_.rhythmClock.stepAbs = 4;
    state_.rhythmClock.beat = 1;
    bass_->update(1000);
    EXPECT_EQ(noteOffCount(3), 1);   // previous released
    EXPECT_EQ(lastNoteOn(3), 52);
}
