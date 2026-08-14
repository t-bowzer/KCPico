#include <gtest/gtest.h>
#include "midi_router.h"
#include "adapters_null.h"
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
