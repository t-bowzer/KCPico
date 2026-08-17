#include <gtest/gtest.h>
#include "midi_router.h"
#include "adapters_null.h"
#include "recording_midi_out.h"
#include "state.h"
#include "midimsg.h"


class MidiRouterTest : public ::testing::Test {
protected:
    void SetUp() override {
        nullMidi_.begin();
        state_.loadDefaults();
        router_ = new MidiRouter(nullMidi_, state_);
    }
    void TearDown() override {
        delete router_;
    }

    NullMidiOutAdapter nullMidi_;
    StateManager state_;
    MidiRouter* router_ = nullptr;
};

TEST_F(MidiRouterTest, NoteOnTracksInState) {
    router_->noteOn(1, 60, 100);
    EXPECT_TRUE(state_.isNoteActive(1, 60));
}

TEST_F(MidiRouterTest, NoteOffRemovesFromState) {
    router_->noteOn(1, 60, 100);
    router_->noteOff(1, 60);
    EXPECT_FALSE(state_.isNoteActive(1, 60));
}

TEST_F(MidiRouterTest, CcDoesNotAffectNotes) {
    router_->cc(1, midi::CC_VOLUME, 100);
    EXPECT_FALSE(state_.isNoteActive(1, 0));
}

TEST_F(MidiRouterTest, SendTestNote) {
    router_->sendTestNote();
    EXPECT_TRUE(state_.isNoteActive(1, 60));
}

TEST_F(MidiRouterTest, AllNotesOffSendsCC) {
    router_->allNotesOff(1);
    // Should not crash with null adapter
    SUCCEED();
}

TEST_F(MidiRouterTest, MultipleNotes) {
    router_->noteOn(1, 60, 100);
    router_->noteOn(1, 64, 100);
    router_->noteOn(1, 67, 100);
    EXPECT_TRUE(state_.isNoteActive(1, 60));
    EXPECT_TRUE(state_.isNoteActive(1, 64));
    EXPECT_TRUE(state_.isNoteActive(1, 67));

    state_.allNotesOff();
    EXPECT_FALSE(state_.isNoteActive(1, 60));
}

// Panic (FR-C11 / AC-17): All-Sound-Off (CC120) + All-Notes-Off (CC123) on all
// 16 channels, and clears active-note state.
TEST(MidiRouter, PanicSendsCcOnAllChannelsAndClearsState) {
    RecordingMidiOutAdapter rec;
    rec.begin();
    StateManager state;
    state.loadDefaults();
    MidiRouter router(rec, state);

    router.noteOn(1, 60, 100);
    router.noteOn(10, 40, 100);
    EXPECT_TRUE(state.isNoteActive(1, 60));
    EXPECT_TRUE(state.isNoteActive(10, 40));

    router.panic();

    EXPECT_TRUE(state.activeNotes.empty());

    // 16 channels x (CC120 + CC123) = 32 control-change messages.
    int cc120 = 0, cc123 = 0;
    std::vector<bool> seen120(17), seen123(17);
    for (const auto& m : rec.messages()) {
        if ((m.status & 0xF0) != midi::STATUS_CONTROL_CHANGE) continue;
        uint8_t ch = (m.status & 0x0F) + 1;
        if (m.data1 == midi::CC_ALL_SOUND_OFF) { cc120++; seen120[ch] = true; }
        if (m.data1 == midi::CC_ALL_NOTES_OFF) { cc123++; seen123[ch] = true; }
    }
    EXPECT_EQ(cc120, 16);
    EXPECT_EQ(cc123, 16);
    for (int ch = 1; ch <= 16; ch++) {
        EXPECT_TRUE(seen120[ch]) << "CC120 missing on channel " << ch;
        EXPECT_TRUE(seen123[ch]) << "CC123 missing on channel " << ch;
    }
}
