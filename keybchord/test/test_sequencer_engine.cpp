#include <gtest/gtest.h>

#include "adapters_null.h"
#include "chord_engine.h"
#include "display_manager.h"
#include "midi_router.h"
#include "recording_midi_out.h"
#include "sequencer_engine.h"
#include "state.h"
#include "storage_stub.h"


namespace {

constexpr uint8_t CTRL = 0x01;
constexpr uint8_t HID_T = 0x17;
constexpr uint8_t HID_Y = 0x1C;
constexpr uint8_t HID_2 = 0x1F;
constexpr uint8_t HID_F9 = 0x42;
constexpr uint8_t HID_F10 = 0x43;
constexpr uint8_t HID_F11 = 0x44;
constexpr uint8_t HID_ENTER = 0x28;

constexpr uint64_t STEP_US = 125000;  // 120 BPM, 16th note

} // namespace


class SequencerEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        state_.loadDefaults();
        state_.pendingRhythm.enabled = true;
        state_.pendingRhythm.tempo = 120;
        state_.rhythmClock.running.store(true);
        setStep(0);

        midi_.begin();
        router_ = new MidiRouter(midi_, state_);
        chord_ = new ChordEngine(state_, *router_);
        display_ = new DisplayManager(state_, lcd_);
        seq_ = new SequencerEngine(state_, storage_, *chord_, *display_);
        chord_->setChordTriggeredCallback(
            [this](const ResolvedChord& c, uint64_t t) {
                seq_->onLiveChordTriggered(c, t);
            });
        seq_->loadForPreset(0, 0);
    }

    void TearDown() override {
        delete seq_;
        delete display_;
        delete chord_;
        delete router_;
    }

    void setStep(uint32_t abs) {
        state_.rhythmClock.stepAbs.store(abs);
        state_.rhythmClock.step.store(abs % 16);
        state_.rhythmClock.stepsPerBar.store(16);
        state_.rhythmClock.nextStepUs.store(
            static_cast<uint64_t>(abs + 1) * STEP_US);
    }

    uint64_t atStep(uint32_t abs) const {
        return static_cast<uint64_t>(abs) * STEP_US + 1000;
    }

    KeyEvent key(uint8_t usage, bool pressed, uint8_t mods = 0) {
        return {usage, pressed, mods};
    }

    void advanceTo(uint32_t abs) {
        setStep(abs);
        seq_->update(atStep(abs));
    }

    void playChord(uint8_t usage, uint32_t abs) {
        chord_->handleKeyEvent(key(usage, true), atStep(abs));
    }

    StateManager state_;
    StorageStub storage_;
    NullLcdAdapter lcd_;
    RecordingMidiOutAdapter midi_;
    MidiRouter* router_ = nullptr;
    ChordEngine* chord_ = nullptr;
    DisplayManager* display_ = nullptr;
    SequencerEngine* seq_ = nullptr;
};


TEST_F(SequencerEngineTest, RecordsChordChangesOnTheGridAndSaves) {
    // Arm at step 0; the recording starts on the next bar (step 16).
    ASSERT_TRUE(seq_->handleKeyEvent(key(HID_F9, true, CTRL), atStep(0)));
    EXPECT_TRUE(state_.recordGuideActive.load());
    EXPECT_TRUE(state_.recordGuideHatsActive.load());
    advanceTo(8);           // still counting in
    EXPECT_FALSE(state_.sequencer.recording);
    EXPECT_TRUE(state_.recordGuideActive.load());
    EXPECT_TRUE(state_.recordGuideHatsActive.load());

    advanceTo(16);          // count-in bar complete -> recording
    EXPECT_TRUE(state_.sequencer.recording);

    advanceTo(17);          // downbeat step fires
    playChord(HID_T, 17);   // C major -> step 0
    chord_->handleKeyEvent(key(HID_T, false), atStep(31));
    advanceTo(33);          // step 16 fires
    playChord(HID_Y, 33);   // G major -> step 16
    advanceTo(80);          // four bars recorded (16 + 64)

    const Sequence& s = seq_->library().slots[0];
    ASSERT_EQ(s.events.size(), 2u);
    EXPECT_EQ(s.events[0].step, 0);
    EXPECT_EQ(s.events[0].chord.rootPc, 0);   // C
    EXPECT_EQ(s.events[1].step, 16);
    EXPECT_EQ(s.events[1].chord.rootPc, 7);   // G
    EXPECT_EQ(s.bars, 4);
    EXPECT_TRUE(storage_.exists("/sequences/b1p1.json"));
    EXPECT_FALSE(state_.sequencer.recording);
    EXPECT_FALSE(state_.recordGuideActive.load());
    EXPECT_FALSE(state_.recordGuideHatsActive.load());
}

TEST_F(SequencerEngineTest, PlaybackLoopsAndStops) {
    // Seed a sequence directly, then play it from the next bar.
    SequencerLibrary lib;
    lib.slots[0].bars = 1;
    lib.slots[0].steps_per_bar = 16;
    ResolvedChord c;
    c.rootPc = 5;
    c.type = ChordType::Major;
    lib.slots[0].setEvent(0, c);
    ASSERT_TRUE(saveSequenceFile(storage_, 0, 0, lib));
    seq_->loadForPreset(0, 0);
    ASSERT_EQ(seq_->library().slots[0].events.size(), 1u);
    const KeyAction* playAction = state_.keymap.find(
        KMOD_CTRL, hid_usage::PAGE_KEYBOARD, HID_F10);
    ASSERT_NE(playAction, nullptr);
    EXPECT_EQ(playAction->cmd, KeyCmd::SequencePlay);

    ASSERT_TRUE(seq_->handleKeyEvent(key(HID_F10, true, CTRL), atStep(0)));
    EXPECT_TRUE(seq_->armed());
    advanceTo(16);  // arm playback at step 16
    EXPECT_TRUE(state_.sequencer.playing);
    EXPECT_EQ(midi_.noteOnCount(), 0);  // waits for the downbeat to fire

    setStep(17);
    seq_->update(atStep(17));
    EXPECT_EQ(midi_.noteOnCount(), 3);  // step 0 fires

    setStep(18);
    seq_->update(atStep(18));
    EXPECT_EQ(midi_.noteOnCount(), 3);  // no event until the loop wraps

    setStep(33);  // one bar later -> step 0 again
    seq_->update(atStep(33));
    EXPECT_EQ(midi_.noteOnCount(), 6);

    midi_.clear();
    ASSERT_TRUE(seq_->handleKeyEvent(key(HID_F10, true, CTRL), atStep(33)));
    EXPECT_FALSE(state_.sequencer.playing);
    EXPECT_EQ(midi_.noteOffCount(), 3);
}

TEST_F(SequencerEngineTest, LiveChordSuppressesPlaybackThenResumes) {
    SequencerLibrary lib;
    lib.slots[0].bars = 4;
    lib.slots[0].steps_per_bar = 16;
    ResolvedChord c;
    c.rootPc = 5;
    c.type = ChordType::Major;
    lib.slots[0].setEvent(0, c);
    c.rootPc = 0;
    lib.slots[0].setEvent(32, c);
    ASSERT_TRUE(saveSequenceFile(storage_, 0, 0, lib));
    seq_->loadForPreset(0, 0);
    ASSERT_EQ(seq_->library().slots[0].events.size(), 2u);

    ASSERT_TRUE(seq_->handleKeyEvent(key(HID_F10, true, CTRL), atStep(0)));
    advanceTo(16);  // arm playback at the bar boundary
    advanceTo(17);  // downbeat fires
    EXPECT_EQ(midi_.noteOnCount(), 3);

    // A live chord takes over and the sequence skips its step-32 event.
    chord_->handleKeyEvent(key(HID_T, true), atStep(24));
    midi_.clear();
    setStep(49);  // step 32 fires
    seq_->update(atStep(49));
    EXPECT_EQ(midi_.noteOnCount(), 0);

    // Release/clear the live chord, then the next event sounds.
    chord_->handleKeyEvent(key(HID_T, false), atStep(50));
    chord_->allNotesOff();
    midi_.clear();
    setStep(81);  // loop wraps back to step 0
    seq_->update(atStep(81));
    EXPECT_EQ(midi_.noteOnCount(), 3);
}

TEST_F(SequencerEngineTest, RefusesPlayAndRecordWithoutRhythm) {
    state_.pendingRhythm.enabled = false;

    SequencerLibrary lib;
    lib.slots[0].setEvent(0, ResolvedChord{});
    ASSERT_TRUE(saveSequenceFile(storage_, 0, 0, lib));
    seq_->loadForPreset(0, 0);
    ASSERT_EQ(seq_->library().slots[0].events.size(), 1u);
    KeyAction playAction = state_.keymap.find(
        KMOD_CTRL, hid_usage::PAGE_KEYBOARD, HID_F10)->cmd == KeyCmd::SequencePlay ? KeyAction{} : KeyAction{};
    (void)playAction;

    ASSERT_TRUE(seq_->handleKeyEvent(key(HID_F10, true, CTRL), atStep(0)));
    advanceTo(16);
    EXPECT_FALSE(state_.sequencer.playing);
    EXPECT_EQ(midi_.noteOnCount(), 0);

    ASSERT_TRUE(seq_->handleKeyEvent(key(HID_F9, true, CTRL), atStep(16)));
    advanceTo(32);
    EXPECT_FALSE(state_.sequencer.recording);
}

TEST_F(SequencerEngineTest, CtrlNumberSelectsSlotAndBarsAreSaved) {
    ASSERT_TRUE(seq_->handleKeyEvent(key(HID_2, true, CTRL), atStep(0)));
    EXPECT_EQ(state_.sequencer.slot, 1);

    state_.sequencer.bars = 8;
    seq_->update(atStep(1));
    EXPECT_EQ(seq_->library().slots[1].bars, 8);
    EXPECT_TRUE(storage_.exists("/sequences/b1p1.json"));
}

TEST_F(SequencerEngineTest, ClearPromptRemovesSequence) {
    SequencerLibrary lib;
    lib.slots[0].setEvent(0, ResolvedChord{});
    ASSERT_TRUE(saveSequenceFile(storage_, 0, 0, lib));
    seq_->loadForPreset(0, 0);

    ASSERT_TRUE(seq_->handleKeyEvent(key(HID_F11, true, CTRL), atStep(0)));
    EXPECT_TRUE(seq_->clearPromptActive());
    ASSERT_TRUE(seq_->handleKeyEvent(key(HID_ENTER, true), atStep(0)));
    EXPECT_FALSE(seq_->clearPromptActive());
    EXPECT_TRUE(seq_->library().slots[0].empty());
    EXPECT_FALSE(storage_.exists("/sequences/b1p1.json"));
}

TEST_F(SequencerEngineTest, ClearKeepsOtherSlotsInTheFile) {
    SequencerLibrary lib;
    lib.slots[0].setEvent(0, ResolvedChord{});
    lib.slots[1].setEvent(0, ResolvedChord{});
    ASSERT_TRUE(saveSequenceFile(storage_, 0, 0, lib));
    seq_->loadForPreset(0, 0);

    ASSERT_TRUE(seq_->handleKeyEvent(key(HID_F11, true, CTRL), atStep(0)));
    ASSERT_TRUE(seq_->handleKeyEvent(key(HID_ENTER, true), atStep(0)));

    EXPECT_TRUE(seq_->library().slots[0].empty());
    EXPECT_FALSE(seq_->library().slots[1].empty());
    EXPECT_TRUE(storage_.exists("/sequences/b1p1.json"));
    SequencerLibrary onDisk = loadSequenceFile(storage_, 0, 0);
    EXPECT_TRUE(onDisk.slots[0].empty());
    EXPECT_FALSE(onDisk.slots[1].empty());
}


TEST_F(SequencerEngineTest, CountInIsExactlyOneBarWithNoEarlyHats) {
    // Arm mid-bar: pattern mutes immediately, but hats wait for the next bar.
    advanceTo(8);
    ASSERT_TRUE(seq_->handleKeyEvent(key(HID_F9, true, CTRL), atStep(8)));
    EXPECT_TRUE(state_.recordGuideActive.load());
    EXPECT_TRUE(state_.recordGuideHatsActive.load());
    EXPECT_EQ(state_.recordGuideHatsStartAbs.load(), 16u);
    EXPECT_FALSE(state_.sequencer.recording);

    advanceTo(16);  // next bar boundary -> exactly one count-in bar starts
    EXPECT_TRUE(state_.recordGuideHatsActive.load());
    EXPECT_EQ(state_.recordGuideHatsStartAbs.load(), 16u);
    EXPECT_FALSE(state_.sequencer.recording);

    advanceTo(32);  // count-in bar complete -> recording
    EXPECT_TRUE(state_.sequencer.recording);
    EXPECT_TRUE(state_.recordGuideHatsActive.load());
}
