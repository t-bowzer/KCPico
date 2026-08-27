#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "display_manager.h"
#include "edit_engine.h"
#include "rhythm.h"
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

    void installTwoDrumPattern() {
        pattern_.name = "Test";
        pattern_.steps_per_bar = 16;
        pattern_.tracks = {
            {36, "kick", {}},
            {38, "snare", {}},
            {42, "hihat", {}},
        };
        state_.pendingRhythm.pattern = 0;
        edit_->setPatternProvider([this](int idx) -> const RhythmPattern* {
            return idx == 0 ? &pattern_ : nullptr;
        });
    }

    StateManager state_;
    RhythmPattern pattern_;
    EditRecordingLcd lcd_;
    DisplayManager* display_ = nullptr;
    EditEngine* edit_ = nullptr;
    int modeChanged_ = 0;
    int patternChanged_ = 0;
};


TEST_F(EditEngineTest, MenuToggleEnterAndExit) {
    EXPECT_EQ(state_.editMenu, EditMenu::None);

    edit_->handleKeyEvent(key(0x44, true), 0);   // F11 -> Rhythm
    EXPECT_EQ(state_.editMenu, EditMenu::Rhythm);
    edit_->handleKeyEvent(key(0x44, true), 0);   // F11 again -> exit
    EXPECT_EQ(state_.editMenu, EditMenu::None);

    edit_->handleKeyEvent(key(0x43, true), 0);   // F10 -> Strum
    EXPECT_EQ(state_.editMenu, EditMenu::Strum);
    edit_->handleKeyEvent(key(0x42, true), 0);   // F9 -> switch to Chord
    EXPECT_EQ(state_.editMenu, EditMenu::Chord);
}

TEST_F(EditEngineTest, BassMenuViaF12) {
    edit_->handleKeyEvent(key(0x45, true), 0);   // F12 -> Bass edit
    EXPECT_EQ(state_.editMenu, EditMenu::Bass);
}

TEST_F(EditEngineTest, SuperF11OpensDrumMenu) {
    edit_->handleKeyEvent(key(0x44, true, 0x08), 0);   // Super+F11
    EXPECT_EQ(state_.editMenu, EditMenu::Drum);
}

TEST_F(EditEngineTest, DrumSubMenuFromRhythmF8) {
    edit_->handleKeyEvent(key(0x44, true), 0);   // F11 -> Rhythm
    EXPECT_EQ(state_.editMenu, EditMenu::Rhythm);
    edit_->handleKeyEvent(key(0x41, true), 0);   // F8 -> Drum sub-menu
    EXPECT_EQ(state_.editMenu, EditMenu::Drum);
}

TEST_F(EditEngineTest, F1CyclesChordMode) {
    edit_->handleKeyEvent(key(0x3A, true), 0);   // F1
    EXPECT_EQ(state_.pendingChord.play_mode, PlayMode::PressToPlay);
    EXPECT_EQ(modeChanged_, 1);
}

TEST_F(EditEngineTest, F2CyclesArpMode) {
    EXPECT_EQ(state_.pendingChord.arp_mode, ArpMode::Up);
    edit_->handleKeyEvent(key(0x3B, true), 0);   // F2
    EXPECT_EQ(state_.pendingChord.arp_mode, ArpMode::Down);
}

TEST_F(EditEngineTest, F3CyclesBassPattern) {
    EXPECT_EQ(state_.pendingBass.pattern, BassPattern::Walking);
    edit_->handleKeyEvent(key(0x3C, true), 0);   // F3
    EXPECT_EQ(state_.pendingBass.pattern, BassPattern::Whole);
}

TEST_F(EditEngineTest, F4CyclesRhythmPattern) {
    edit_->handleKeyEvent(key(0x3D, true), 0);   // F4
    EXPECT_EQ(state_.pendingRhythm.pattern, 1);
    EXPECT_EQ(patternChanged_, 1);
}

TEST_F(EditEngineTest, F5TogglesRhythmEnable) {
    EXPECT_FALSE(state_.pendingRhythm.enabled);
    edit_->handleKeyEvent(key(0x3E, true), 0);   // F5
    EXPECT_TRUE(state_.pendingRhythm.enabled);
    edit_->handleKeyEvent(key(0x3E, true), 0);
    EXPECT_FALSE(state_.pendingRhythm.enabled);
}

TEST_F(EditEngineTest, F6TogglesRhythmMute) {
    edit_->handleKeyEvent(key(0x3F, true), 0);   // F6
    EXPECT_TRUE(state_.pendingRhythm.muted);
    edit_->handleKeyEvent(key(0x3F, true), 0);
    EXPECT_FALSE(state_.pendingRhythm.muted);
}

TEST_F(EditEngineTest, F7TogglesBassEnable) {
    edit_->handleKeyEvent(key(0x40, true), 0);   // F7
    EXPECT_TRUE(state_.pendingBass.enabled);
    edit_->handleKeyEvent(key(0x40, true), 0);
    EXPECT_FALSE(state_.pendingBass.enabled);
}

TEST_F(EditEngineTest, F8TogglesClockOut) {
    EXPECT_FALSE(state_.config.midi_clock_enabled);
    edit_->handleKeyEvent(key(0x41, true), 0);   // F8
    EXPECT_TRUE(state_.config.midi_clock_enabled);
    edit_->handleKeyEvent(key(0x41, true), 0);
    EXPECT_FALSE(state_.config.midi_clock_enabled);
}

TEST_F(EditEngineTest, InversionKeysSetDirectly) {
    edit_->handleKeyEvent(key(0x46, true), 0);   // PrtSc
    EXPECT_EQ(state_.pendingChord.inversion, InversionMode::First);
    edit_->handleKeyEvent(key(0x47, true), 0);   // ScLk
    EXPECT_EQ(state_.pendingChord.inversion, InversionMode::Second);
    edit_->handleKeyEvent(key(0x48, true), 0);   // Pause
    EXPECT_EQ(state_.pendingChord.inversion, InversionMode::Third);
}

TEST_F(EditEngineTest, ChordOctaveKeys) {
    edit_->handleKeyEvent(key(0x2E, true), 0);   // =
    EXPECT_EQ(state_.pendingChord.octave, 1);
    edit_->handleKeyEvent(key(0x2D, true), 0);   // -
    EXPECT_EQ(state_.pendingChord.octave, 0);
}

TEST_F(EditEngineTest, StrumOctaveKeys) {
    edit_->handleKeyEvent(key(0x57, true), 0);   // Kp+
    EXPECT_EQ(state_.pendingStrum.octave, 2);
    edit_->handleKeyEvent(key(0x56, true), 0);   // Kp-
    EXPECT_EQ(state_.pendingStrum.octave, 1);
}

TEST_F(EditEngineTest, CtrlF1CyclesVoicing) {
    EXPECT_EQ(state_.pendingChord.voicing_mode, VoicingMode::RootPosition);
    edit_->handleKeyEvent(key(0x3A, true, 0x01), 0);   // Ctrl+F1
    EXPECT_EQ(state_.pendingChord.voicing_mode, VoicingMode::Smart);
    edit_->handleKeyEvent(key(0x3A, true, 0x01), 0);
    EXPECT_EQ(state_.pendingChord.voicing_mode, VoicingMode::Down);
    edit_->handleKeyEvent(key(0x3A, true, 0x01), 0);
    EXPECT_EQ(state_.pendingChord.voicing_mode, VoicingMode::Up);
    edit_->handleKeyEvent(key(0x3A, true, 0x01), 0);
    EXPECT_EQ(state_.pendingChord.voicing_mode, VoicingMode::RootPosition);
}

TEST_F(EditEngineTest, CtrlF2CyclesInversion) {
    edit_->handleKeyEvent(key(0x3B, true, 0x01), 0);   // Ctrl+F2
    EXPECT_EQ(state_.pendingChord.inversion, InversionMode::First);
    edit_->handleKeyEvent(key(0x3B, true, 0x01), 0);
    EXPECT_EQ(state_.pendingChord.inversion, InversionMode::Second);
}

TEST_F(EditEngineTest, CtrlF4TogglesRoll) {
    EXPECT_EQ(state_.pendingChord.chord_roll_ms, 0);
    edit_->handleKeyEvent(key(0x3D, true, 0x01), 0);   // Ctrl+F4
    EXPECT_EQ(state_.pendingChord.chord_roll_ms, 50);
    edit_->handleKeyEvent(key(0x3D, true, 0x01), 0);
    EXPECT_EQ(state_.pendingChord.chord_roll_ms, 0);
}

TEST_F(EditEngineTest, CtrlF5TogglesBeatLed) {
    EXPECT_TRUE(state_.config.bpm_indicator);
    edit_->handleKeyEvent(key(0x3E, true, 0x01), 0);   // Ctrl+F5
    EXPECT_FALSE(state_.config.bpm_indicator);
    edit_->handleKeyEvent(key(0x3E, true, 0x01), 0);
    EXPECT_TRUE(state_.config.bpm_indicator);
}

TEST_F(EditEngineTest, CtrlEqualsMinusStepsChordVelocity) {
    edit_->handleKeyEvent(key(0x2E, true, 0x01), 0);   // Ctrl+=
    EXPECT_EQ(state_.pendingChord.velocity, 101);
    edit_->handleKeyEvent(key(0x2D, true, 0x01), 0);   // Ctrl+-
    EXPECT_EQ(state_.pendingChord.velocity, 100);
}

TEST_F(EditEngineTest, AltKpEnterCyclesStrumMode) {
    EXPECT_EQ(state_.pendingStrum.mode, StrumMode::FollowChord);
    edit_->handleKeyEvent(key(0x58, true, 0x04), 0);   // Alt+KpEnter
    EXPECT_EQ(state_.pendingStrum.mode, StrumMode::Scale);
}

TEST_F(EditEngineTest, AltKpPlusMinusStepsStrumRoot) {
    edit_->handleKeyEvent(key(0x57, true, 0x04), 0);   // Alt+Kp+
    EXPECT_EQ(state_.pendingStrum.root_pc, 1);
    edit_->handleKeyEvent(key(0x56, true, 0x04), 0);   // Alt+Kp-
    EXPECT_EQ(state_.pendingStrum.root_pc, 0);
}

TEST_F(EditEngineTest, AltEqualsMinusStepsBassOctave) {
    edit_->handleKeyEvent(key(0x2E, true, 0x04), 0);   // Alt+= -> bass octave +1
    EXPECT_EQ(state_.pendingBass.octave, 0);
    edit_->handleKeyEvent(key(0x2D, true, 0x04), 0);   // Alt+- -> bass octave -1
    EXPECT_EQ(state_.pendingBass.octave, -1);
}

TEST_F(EditEngineTest, ArpModeCycleFiresCallback) {
    int arpChanged = 0;
    edit_->setArpModeChangedCallback([&]() { arpChanged++; });

    edit_->handleKeyEvent(key(0x3B, true), 0);   // F2 -> cycle arp mode
    EXPECT_EQ(arpChanged, 1);

    // Cycling a non-arp param does not fire it.
    edit_->handleKeyEvent(key(0x3A, true), 0);   // F1 -> chord mode
    EXPECT_EQ(arpChanged, 1);
}

TEST_F(EditEngineTest, DrumMuteTogglesVelocity) {
    installTwoDrumPattern();

    // Set a fixed kick velocity so we can verify restore.
    state_.pendingRhythm.drums.kick_vel = 90;

    edit_->handleKeyEvent(key(0x3A, true, 0x04), 0);   // Alt+F1 -> mute kick
    EXPECT_EQ(state_.pendingRhythm.drums.kick_vel, param_bounds::DRUM_VELOCITY_OFF);

    edit_->handleKeyEvent(key(0x3A, true, 0x04), 0);   // Alt+F1 -> unmute
    EXPECT_EQ(state_.pendingRhythm.drums.kick_vel, 90);
}

TEST_F(EditEngineTest, DrumMuteDisplaysName) {
    installTwoDrumPattern();
    display_->update(0);
    lcd_.reset();

    edit_->handleKeyEvent(key(0x3A, true, 0x04), 0);   // Alt+F1 -> mute kick
    display_->update(0);
    ASSERT_GE(lcd_.frames().size(), 1u);
    EXPECT_EQ(lcd_.frames().back().l1.substr(0, 9), std::string("Kick Mute"));
    EXPECT_EQ(lcd_.frames().back().l2.substr(0, 2), std::string("On"));
}

TEST_F(EditEngineTest, DrumMuteBeyondTrackCountIsNoOp) {
    installTwoDrumPattern();
    edit_->handleKeyEvent(key(0x3D, true, 0x04), 0);   // Alt+F4 (no 4th track)
    EXPECT_EQ(state_.pendingRhythm.drums.kick_vel, 0);
    EXPECT_EQ(state_.pendingRhythm.drums.snare_vel, 0);
}

TEST_F(EditEngineTest, UpDownNavigateParamsInMenu) {
    edit_->handleKeyEvent(key(0x42, true), 0);   // F9 -> Chord (F1 = Octave)
    EXPECT_EQ(state_.editParam, 0);

    edit_->handleKeyEvent(key(0x51, true), 0);   // Down -> Mode
    EXPECT_EQ(state_.editParam, 1);
    edit_->handleKeyEvent(key(0x52, true), 0);   // Up -> Octave
    EXPECT_EQ(state_.editParam, 0);
    edit_->handleKeyEvent(key(0x52, true), 0);   // Up wraps to ArpMode (last)
    EXPECT_EQ(state_.editParam, 10);
}

TEST_F(EditEngineTest, LeftRightStepValueInMenu) {
    edit_->handleKeyEvent(key(0x44, true), 0);   // F11 -> Rhythm (F1 = Tempo)
    edit_->handleKeyEvent(key(0x4F, true), 0);   // Right -> tempo 121
    EXPECT_EQ(state_.pendingRhythm.tempo, 121);
    edit_->handleKeyEvent(key(0x50, true), 0);   // Left -> tempo 120
    EXPECT_EQ(state_.pendingRhythm.tempo, 120);
}

TEST_F(EditEngineTest, FKeysSelectParam) {
    edit_->handleKeyEvent(key(0x43, true), 0);   // F10 -> Strum
    EXPECT_EQ(state_.editParam, 0);              // F1 = Octave

    edit_->handleKeyEvent(key(0x3B, true), 0);   // F2 -> Duration
    EXPECT_EQ(state_.editParam, 1);

    edit_->handleKeyEvent(key(0x3D, true), 0);   // F4 -> Layout
    EXPECT_EQ(state_.editParam, 3);
}

TEST_F(EditEngineTest, PlusMinusStepsSelectedParamInMenu) {
    edit_->handleKeyEvent(key(0x44, true), 0);   // F11 -> Rhythm (F1 = Tempo)
    edit_->handleKeyEvent(key(0x2E, true), 0);   // = -> tempo 121
    EXPECT_EQ(state_.pendingRhythm.tempo, 121);
    edit_->handleKeyEvent(key(0x2D, true), 0);   // - -> tempo 120
    EXPECT_EQ(state_.pendingRhythm.tempo, 120);
}

TEST_F(EditEngineTest, EscExitsMenu) {
    edit_->handleKeyEvent(key(0x42, true), 0);   // F9 -> Chord
    EXPECT_EQ(state_.editMenu, EditMenu::Chord);
    edit_->handleKeyEvent(key(0x29, true), 0);   // Esc -> exit
    EXPECT_EQ(state_.editMenu, EditMenu::None);
}

TEST_F(EditEngineTest, ValueShownThenRevertsToMenu) {
    edit_->handleKeyEvent(key(0x44, true), 0);   // F11 -> Rhythm
    display_->update(0);
    lcd_.reset();

    edit_->handleKeyEvent(key(0x4F, true), 1000);  // Right -> tempo 121
    display_->update(1000);
    ASSERT_EQ(lcd_.frames().size(), 1u);
    EXPECT_EQ(lcd_.frames()[0].l1, std::string("Rhythm Tempo") + std::string(4, ' '));
    EXPECT_EQ(lcd_.frames()[0].l2, std::string("121") + std::string(13, ' '));

    lcd_.reset();
    display_->update(1000 + 1500UL * 1000);
    ASSERT_EQ(lcd_.frames().size(), 1u);
    EXPECT_EQ(lcd_.frames()[0].l1, std::string("Rhythm Edit") + std::string(5, ' '));
    EXPECT_EQ(lcd_.frames()[0].l2, std::string("Tempo") + std::string(11, ' '));
}

TEST_F(EditEngineTest, ChordAndStrumKeysAreForwarded) {
    EXPECT_FALSE(edit_->handleKeyEvent(key(0x17, true), 0));  // T (chord key)
    EXPECT_FALSE(edit_->handleKeyEvent(key(0x1E, true), 0));  // 1 (strum key)
    EXPECT_FALSE(edit_->handleKeyEvent(key(0x50, true), 0));  // Left (held ext)
}

TEST_F(EditEngineTest, ChordAndStrumKeysForwardedInMenu) {
    edit_->handleKeyEvent(key(0x42, true), 0);   // F9 -> Chord Edit
    EXPECT_FALSE(edit_->handleKeyEvent(key(0x17, true), 0));  // T (chord) forwarded
    EXPECT_FALSE(edit_->handleKeyEvent(key(0x1E, true), 0));  // 1 (strum) forwarded
    EXPECT_FALSE(edit_->handleKeyEvent(key(0x35, true), 0));  // ` (backtick) forwarded

    // Non-chord/strum keys are still consumed.
    EXPECT_TRUE(edit_->handleKeyEvent(key(0x28, true), 0));   // Enter consumed
}

TEST_F(EditEngineTest, ModeChangeFiresCallback) {
    edit_->handleKeyEvent(key(0x3A, true), 0);   // F1 (main menu)
    EXPECT_EQ(state_.pendingChord.play_mode, PlayMode::PressToPlay);
    EXPECT_EQ(modeChanged_, 1);
}

TEST_F(EditEngineTest, TempoAutoRepeatsWhileHeld) {
    edit_->handleKeyEvent(key(0x4B, true), 0);   // PgUp -> tempo 121
    EXPECT_EQ(state_.pendingRhythm.tempo, 121);

    edit_->update(500000ULL - 1);
    EXPECT_EQ(state_.pendingRhythm.tempo, 121);

    edit_->update(500000ULL);
    EXPECT_EQ(state_.pendingRhythm.tempo, 122);
    edit_->update(500000ULL + 80000ULL);
    EXPECT_EQ(state_.pendingRhythm.tempo, 123);

    edit_->handleKeyEvent(key(0x4B, false), 0);
    edit_->update(500000ULL + 80000ULL * 2);
    EXPECT_EQ(state_.pendingRhythm.tempo, 123);
}

TEST_F(EditEngineTest, SmallRangeDoesNotAutoRepeat) {
    edit_->handleKeyEvent(key(0x2E, true), 0);   // = -> chord octave +1
    EXPECT_EQ(state_.pendingChord.octave, 1);

    edit_->update(500000ULL);
    edit_->update(500000ULL + 80000ULL);
    EXPECT_EQ(state_.pendingChord.octave, 1);    // octave does not repeat
}

TEST_F(EditEngineTest, MenuExitsAfterIdleTimeout) {
    edit_->handleKeyEvent(key(0x42, true), 1000);   // F9 -> Chord Edit
    EXPECT_EQ(state_.editMenu, EditMenu::Chord);

    edit_->update(1000 + 10000ULL * 1000 - 1);
    EXPECT_EQ(state_.editMenu, EditMenu::Chord);    // still open

    edit_->update(1000 + 10000ULL * 1000);
    EXPECT_EQ(state_.editMenu, EditMenu::None);     // timed out to main screen
}

TEST_F(EditEngineTest, TapTempoAveragesIntervals) {
    edit_->handleKeyEvent(key(0x2C, true), 0);
    edit_->handleKeyEvent(key(0x2C, true), 1000000);   // 1 s interval -> 60 BPM
    EXPECT_EQ(state_.pendingRhythm.tempo, 60);

    edit_->handleKeyEvent(key(0x2C, true), 4000000);
    EXPECT_EQ(state_.pendingRhythm.tempo, 60);

    edit_->handleKeyEvent(key(0x2C, true), 4500000);   // 0.5 s interval -> 120 BPM
    EXPECT_EQ(state_.pendingRhythm.tempo, 120);
}

TEST_F(EditEngineTest, TapTempoClampsToBounds) {
    edit_->handleKeyEvent(key(0x2C, true), 0);
    edit_->handleKeyEvent(key(0x2C, true), 1900000);
    EXPECT_EQ(state_.pendingRhythm.tempo, 40);
}

TEST_F(EditEngineTest, DrumNoteChangeAuditions) {
    int auditioned = -1;
    edit_->setDrumAuditionCallback([&](uint8_t note) { auditioned = note; });

    edit_->handleKeyEvent(key(0x44, true), 0);   // F11 -> Rhythm
    edit_->handleKeyEvent(key(0x41, true), 0);   // F8 -> Drum menu (F1 = Kick)
    edit_->handleKeyEvent(key(0x4F, true), 0);   // Right -> kick note 37
    EXPECT_EQ(state_.pendingRhythm.drums.kick, 37);
    EXPECT_EQ(auditioned, 37);

    // Velocity changes do not audition.
    auditioned = -1;
    edit_->handleKeyEvent(key(0x3B, true), 0);   // F2 -> Kick Vel
    edit_->handleKeyEvent(key(0x4F, true), 0);   // Right -> kick vel 1
    EXPECT_EQ(state_.pendingRhythm.drums.kick_vel, 1);
    EXPECT_EQ(auditioned, -1);
}
