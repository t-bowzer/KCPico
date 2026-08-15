#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "display_manager.h"
#include "edit_engine.h"
#include "state.h"


class EditRecordingLcd : public LcdAdapter {
public:
    struct Frame {
        std::string l1;
        std::string l2;
    };

    bool begin() override { return true; }
    void write(const std::string& l1, const std::string& l2) override {
        frames_.push_back({l1, l2});
    }
    void clear() override {}

    const std::vector<Frame>& frames() const { return frames_; }
    void reset() { frames_.clear(); }

private:
    std::vector<Frame> frames_;
};


class EditEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        state_.loadDefaults();
        lcd_.reset();
        display_ = new DisplayManager(state_, lcd_);
        edit_ = new EditEngine(state_, *display_);
        modeChanged_ = 0;
        patternChanged_ = 0;
        edit_->setModeChangedCallback([this]() { modeChanged_++; });
        edit_->setPatternChangedCallback([this]() { patternChanged_++; });
    }

    void TearDown() override {
        delete edit_;
        delete display_;
    }

    KeyEvent key(uint8_t usage, bool pressed, uint8_t mods = 0) {
        return {usage, pressed, mods};
    }

    StateManager state_;
    EditRecordingLcd lcd_;
    DisplayManager* display_ = nullptr;
    EditEngine* edit_ = nullptr;
    int modeChanged_ = 0;
    int patternChanged_ = 0;
};


TEST_F(EditEngineTest, MenuToggleEnterAndExit) {
    EXPECT_EQ(state_.editMenu, EditMenu::None);

    edit_->handleKeyEvent(key(0xE1, true), 0);   // Ctrl -> Rhythm
    EXPECT_EQ(state_.editMenu, EditMenu::Rhythm);
    edit_->handleKeyEvent(key(0xE1, true), 0);   // Ctrl again -> exit
    EXPECT_EQ(state_.editMenu, EditMenu::None);

    edit_->handleKeyEvent(key(0xE3, true), 0);   // Alt -> Strum
    EXPECT_EQ(state_.editMenu, EditMenu::Strum);
    edit_->handleKeyEvent(key(0x65, true), 0);   // Menu -> switch to Chord
    EXPECT_EQ(state_.editMenu, EditMenu::Chord);
}

TEST_F(EditEngineTest, F10FallsBackToChordEdit) {
    edit_->handleKeyEvent(key(0x43, true), 0);   // F10 -> enter chord edit
    EXPECT_EQ(state_.editMenu, EditMenu::Chord);
}

TEST_F(EditEngineTest, MenuTitleAndParamRendered) {
    edit_->handleKeyEvent(key(0xE3, true), 0);   // Alt -> Strum Edit
    display_->update(0);

    ASSERT_EQ(lcd_.frames().size(), 1u);
    EXPECT_EQ(lcd_.frames()[0].l1, std::string("Strum Edit") + std::string(6, ' '));
    EXPECT_EQ(lcd_.frames()[0].l2, std::string("Octave") + std::string(10, ' '));
}

TEST_F(EditEngineTest, FKeysSelectParam) {
    edit_->handleKeyEvent(key(0xE3, true), 0);   // Alt -> Strum
    EXPECT_EQ(state_.editParam, 0);              // F1 = Octave

    edit_->handleKeyEvent(key(0x3B, true), 0);   // F2 -> Duration
    EXPECT_EQ(state_.editParam, 1);

    edit_->handleKeyEvent(key(0x3D, true), 0);   // F4 -> Layout
    EXPECT_EQ(state_.editParam, 3);

    // Out-of-range F-key is ignored (Strum has 4 params).
    edit_->handleKeyEvent(key(0x40, true), 0);   // F7 -> index 6, ignored
    EXPECT_EQ(state_.editParam, 3);
}

TEST_F(EditEngineTest, PlusMinusStepsSelectedParam) {
    edit_->handleKeyEvent(key(0xE1, true), 0);   // Ctrl -> Rhythm (F1 = Tempo)
    edit_->handleKeyEvent(key(0x2E, true), 0);   // + -> tempo 121
    EXPECT_EQ(state_.pendingRhythm.tempo, 121);
    edit_->handleKeyEvent(key(0x2D, true), 0);   // - -> tempo 120
    EXPECT_EQ(state_.pendingRhythm.tempo, 120);

    edit_->handleKeyEvent(key(0x3B, true), 0);   // F2 -> Swing
    edit_->handleKeyEvent(key(0x2E, true), 0);   // + -> swing +5
    EXPECT_EQ(state_.pendingRhythm.swing, 5);
}

TEST_F(EditEngineTest, PageUpDownStepsInMenu) {
    edit_->handleKeyEvent(key(0xE1, true), 0);   // Ctrl -> Rhythm (F1 = Tempo)
    edit_->handleKeyEvent(key(0x4B, true), 0);   // Page Up -> tempo 121
    EXPECT_EQ(state_.pendingRhythm.tempo, 121);
    edit_->handleKeyEvent(key(0x4E, true), 0);   // Page Down -> 120
    EXPECT_EQ(state_.pendingRhythm.tempo, 120);
}

TEST_F(EditEngineTest, EscExitsMenu) {
    edit_->handleKeyEvent(key(0x65, true), 0);   // Menu -> Chord
    EXPECT_EQ(state_.editMenu, EditMenu::Chord);
    edit_->handleKeyEvent(key(0x29, true), 0);   // Esc -> exit
    EXPECT_EQ(state_.editMenu, EditMenu::None);
}

TEST_F(EditEngineTest, ValueShownThenRevertsToMenu) {
    edit_->handleKeyEvent(key(0xE1, true), 0);   // Ctrl -> Rhythm
    display_->update(0);
    lcd_.reset();

    edit_->handleKeyEvent(key(0x2E, true), 1000);  // + -> tempo 121
    display_->update(1000);
    ASSERT_EQ(lcd_.frames().size(), 1u);
    EXPECT_EQ(lcd_.frames()[0].l1, std::string("Rhythm Tempo") + std::string(4, ' '));
    EXPECT_EQ(lcd_.frames()[0].l2, std::string("121") + std::string(13, ' '));

    // Reverts to the menu screen after the revert timeout (1500ms).
    lcd_.reset();
    display_->update(1000 + 1500UL * 1000);
    ASSERT_EQ(lcd_.frames().size(), 1u);
    EXPECT_EQ(lcd_.frames()[0].l1, std::string("Rhythm Edit") + std::string(5, ' '));
    EXPECT_EQ(lcd_.frames()[0].l2, std::string("Tempo") + std::string(11, ' '));
}

TEST_F(EditEngineTest, DirectModeCycleFiresCallback) {
    edit_->handleKeyEvent(key(0x3A, true), 0);   // F1 (main menu)
    EXPECT_EQ(state_.pendingChord.play_mode, PlayMode::PressToPlay);
    EXPECT_EQ(modeChanged_, 1);
}

TEST_F(EditEngineTest, DirectShortcuts) {
    // F5 voicing.
    edit_->handleKeyEvent(key(0x3E, true), 0);
    EXPECT_EQ(state_.pendingChord.voicing_mode, VoicingMode::Smart);

    // Arrows -> extensions.
    edit_->handleKeyEvent(key(0x50, true), 0);
    edit_->handleKeyEvent(key(0x51, true), 0);
    edit_->handleKeyEvent(key(0x4F, true), 0);
    EXPECT_TRUE(state_.pendingChord.add9);
    EXPECT_TRUE(state_.pendingChord.add11);
    EXPECT_TRUE(state_.pendingChord.add13);

    // Number-row = / - -> chord octave.
    edit_->handleKeyEvent(key(0x2E, true), 0);
    EXPECT_EQ(state_.pendingChord.octave, 1);
    edit_->handleKeyEvent(key(0x2D, true), 0);
    EXPECT_EQ(state_.pendingChord.octave, 0);

    // Keypad + / - -> strum octave.
    edit_->handleKeyEvent(key(0x57, true), 0);
    EXPECT_EQ(state_.pendingStrum.octave, 2);
    edit_->handleKeyEvent(key(0x56, true), 0);
    EXPECT_EQ(state_.pendingStrum.octave, 1);

    // Page Up/Down -> tempo.
    edit_->handleKeyEvent(key(0x4B, true), 0);
    EXPECT_EQ(state_.pendingRhythm.tempo, 121);

    // F7/F8/F9 -> rhythm toggles.
    edit_->handleKeyEvent(key(0x40, true), 0);
    EXPECT_TRUE(state_.pendingRhythm.enabled);
    edit_->handleKeyEvent(key(0x41, true), 0);
    EXPECT_EQ(state_.pendingRhythm.pattern, 1);
    EXPECT_EQ(patternChanged_, 1);
    edit_->handleKeyEvent(key(0x42, true), 0);
    EXPECT_TRUE(state_.pendingRhythm.muted);
}

TEST_F(EditEngineTest, ChordAndStrumKeysAreForwarded) {
    EXPECT_FALSE(edit_->handleKeyEvent(key(0x17, true), 0));  // T (chord key)
    EXPECT_FALSE(edit_->handleKeyEvent(key(0x1E, true), 0));  // 1 (strum key)
}

TEST_F(EditEngineTest, ChordAndStrumKeysForwardedInMenu) {
    edit_->handleKeyEvent(key(0x65, true), 0);   // Menu -> Chord Edit
    EXPECT_FALSE(edit_->handleKeyEvent(key(0x17, true), 0));  // T (chord) forwarded
    EXPECT_FALSE(edit_->handleKeyEvent(key(0x1E, true), 0));  // 1 (strum) forwarded
    EXPECT_FALSE(edit_->handleKeyEvent(key(0x35, true), 0));  // ` (backtick) forwarded

    // Non-chord/strum keys are still consumed.
    EXPECT_TRUE(edit_->handleKeyEvent(key(0x28, true), 0));   // Enter consumed
}

TEST_F(EditEngineTest, ModeChangeInMenuFiresCallback) {
    edit_->handleKeyEvent(key(0x65, true), 0);   // Chord Edit
    edit_->handleKeyEvent(key(0x3B, true), 0);   // F2 -> Mode
    edit_->handleKeyEvent(key(0x2E, true), 0);   // + -> next mode
    EXPECT_EQ(state_.pendingChord.play_mode, PlayMode::PressToPlay);
    EXPECT_EQ(modeChanged_, 1);
}
