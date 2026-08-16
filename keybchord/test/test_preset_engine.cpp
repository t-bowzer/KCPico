#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "adapters_null.h"
#include "display_manager.h"
#include "edit_engine.h"
#include "preset_engine.h"
#include "state.h"
#include "storage_stub.h"


namespace {

constexpr uint8_t HOME      = 0x4A;
constexpr uint8_t END       = 0x4D;
constexpr uint8_t INSERT    = 0x49;
constexpr uint8_t DELETE    = 0x4C;
constexpr uint8_t ENTER     = 0x28;
constexpr uint8_t ESC       = 0x29;
constexpr uint8_t BACKSPACE = 0x2A;
constexpr uint8_t SUPER     = 0x08;  // LGui

} // namespace


class PresetEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        state_.loadDefaults();
        display_ = new DisplayManager(state_, lcd_);
        engine_  = new PresetEngine(state_, storage_, *display_);
        edit_    = new EditEngine(state_, *display_);

        modeChanged_ = 0;
        engine_->setModeChangedCallback([this]() { modeChanged_++; });
        edit_->setAnyEditCallback([this]() { engine_->recomputeDirty(); });
    }

    void TearDown() override {
        delete edit_;
        delete engine_;
        delete display_;
    }

    KeyEvent key(uint8_t usage, bool pressed, uint8_t mods = 0) {
        return {usage, pressed, mods};
    }

    // Full Core 0 routing: preset -> edit. (Chord/strum engines are not wired
    // here; forwarded chord/strum keys simply report unconsumed.)
    bool route(const KeyEvent& ev, uint64_t now = 0) {
        bool consumed = engine_->handleKeyEvent(ev, now);
        if (!consumed) consumed = edit_->handleKeyEvent(ev, now);
        return consumed;
    }

    void setDistinctive() {
        state_.pendingChord.velocity  = 111;
        state_.pendingStrum.velocity  = 77;
        state_.pendingRhythm.tempo    = 234;
    }

    StateManager state_;
    StorageStub   storage_;
    NullLcdAdapter lcd_;
    DisplayManager* display_ = nullptr;
    PresetEngine*  engine_   = nullptr;
    EditEngine*    edit_     = nullptr;
    int            modeChanged_ = 0;
};


TEST_F(PresetEngineTest, HomeEntersCursorAndWraps) {
    EXPECT_FALSE(state_.cursorActive);

    route(key(HOME, true));
    EXPECT_TRUE(state_.cursorActive);
    EXPECT_EQ(state_.cursorBank, 9);   // wraps to last slot of previous bank
    EXPECT_EQ(state_.cursorSlot, 7);
    // Active slot is unchanged.
    EXPECT_EQ(state_.currentBank, 0);
    EXPECT_EQ(state_.currentSlot, 0);
}

TEST_F(PresetEngineTest, EndMovesCursorForward) {
    route(key(END, true));
    EXPECT_TRUE(state_.cursorActive);
    EXPECT_EQ(state_.cursorBank, 0);
    EXPECT_EQ(state_.cursorSlot, 1);
}

TEST_F(PresetEngineTest, SuperHomeEndMoveBank) {
    route(key(HOME, true, SUPER));
    EXPECT_TRUE(state_.cursorActive);
    EXPECT_EQ(state_.cursorBank, 9);
    EXPECT_EQ(state_.cursorSlot, 0);

    route(key(END, true, SUPER));
    EXPECT_EQ(state_.cursorBank, 0);
    EXPECT_EQ(state_.cursorSlot, 0);
}

TEST_F(PresetEngineTest, HomeLeavesEditMenu) {
    route(key(0xE1, true));   // Ctrl -> Rhythm Edit
    EXPECT_EQ(state_.editMenu, EditMenu::Rhythm);

    route(key(HOME, true));   // Home -> cursor mode, leaves edit menu
    EXPECT_EQ(state_.editMenu, EditMenu::None);
    EXPECT_TRUE(state_.cursorActive);
}

TEST_F(PresetEngineTest, MenuToggleLeavesCursorMode) {
    route(key(HOME, true));
    EXPECT_TRUE(state_.cursorActive);

    route(key(0xE1, true));   // Ctrl -> Rhythm Edit
    EXPECT_FALSE(state_.cursorActive);
    EXPECT_EQ(state_.editMenu, EditMenu::Rhythm);
}

TEST_F(PresetEngineTest, EscExitsCursorMode) {
    route(key(HOME, true));
    EXPECT_TRUE(state_.cursorActive);

    route(key(ESC, true));
    EXPECT_FALSE(state_.cursorActive);
    EXPECT_EQ(state_.currentBank, 0);
    EXPECT_EQ(state_.currentSlot, 0);
}

TEST_F(PresetEngineTest, CursorAutoResetsAfterTimeout) {
    route(key(HOME, true), 1000);
    EXPECT_TRUE(state_.cursorActive);

    engine_->update(1000 + 5000ULL * 1000 - 1);
    EXPECT_TRUE(state_.cursorActive);

    engine_->update(1000 + 5000ULL * 1000);
    EXPECT_FALSE(state_.cursorActive);
}

TEST_F(PresetEngineTest, ChordKeyStaysLiveInCursorMode) {
    route(key(HOME, true));
    EXPECT_TRUE(state_.cursorActive);

    // A chord key is forwarded (not consumed) and does not leave cursor mode.
    EXPECT_FALSE(route(key(0x17, true)));   // T
    EXPECT_TRUE(state_.cursorActive);
}

TEST_F(PresetEngineTest, HotkeyCancelsCursorAndForwards) {
    route(key(HOME, true));
    EXPECT_TRUE(state_.cursorActive);

    // F1 is a hotkey, not a preset/play key: browsing is cancelled and the
    // hotkey is handled normally (play-mode cycle).
    bool consumed = route(key(0x3A, true));   // F1
    EXPECT_FALSE(state_.cursorActive);
    EXPECT_TRUE(consumed);
    EXPECT_EQ(state_.pendingChord.play_mode, PlayMode::PressToPlay);
}

TEST_F(PresetEngineTest, RhythmHotkeyCancelsCursor) {
    route(key(HOME, true));
    EXPECT_TRUE(state_.cursorActive);

    route(key(0x40, true));   // F7 -> enable rhythm
    EXPECT_FALSE(state_.cursorActive);
    EXPECT_TRUE(state_.pendingRhythm.enabled);
}

TEST_F(PresetEngineTest, EnterLoadsCursor) {
    PresetSlot p = PresetSlot::defaults();
    p.name = "Target";
    p.chord.velocity = 99;
    savePreset(storage_, 1, 2, p);   // B2:P3

    state_.cursorActive = true;
    state_.cursorBank   = 1;
    state_.cursorSlot   = 2;

    route(key(ENTER, true));
    EXPECT_FALSE(state_.cursorActive);
    EXPECT_EQ(state_.currentBank, 1);
    EXPECT_EQ(state_.currentSlot, 2);
    EXPECT_EQ(state_.pendingChord.velocity, 99);
    EXPECT_FALSE(state_.dirty);
}

TEST_F(PresetEngineTest, EnterInMainModeDoesNothing) {
    setDistinctive();
    route(key(ENTER, true));
    // Enter with no cursor and no prompt is inert.
    EXPECT_EQ(state_.pendingChord.velocity, 111);
    EXPECT_FALSE(storage_.exists("/presets/bank1.json"));
}

TEST_F(PresetEngineTest, SuperNumberLoadsSlotDirectly) {
    PresetSlot p = PresetSlot::defaults();
    p.chord.velocity = 42;
    savePreset(storage_, 0, 3, p);   // B1:P4

    route(key(0x1E + 3, true, SUPER));   // Super+4 -> load slot 3
    EXPECT_EQ(state_.currentSlot, 3);
    EXPECT_EQ(state_.pendingChord.velocity, 42);
    EXPECT_FALSE(state_.cursorActive);
}

TEST_F(PresetEngineTest, LoadFiresModeChangedWhenModeChanges) {
    PresetSlot p = PresetSlot::defaults();
    p.chord.play_mode = PlayMode::Arpeggio;
    savePreset(storage_, 0, 1, p);   // B1:P2

    state_.cursorActive = true;
    state_.cursorBank   = 0;
    state_.cursorSlot   = 1;

    route(key(ENTER, true));
    EXPECT_EQ(modeChanged_, 1);
    EXPECT_EQ(state_.pendingChord.play_mode, PlayMode::Arpeggio);
}

TEST_F(PresetEngineTest, SavePromptConfirmWritesPending) {
    setDistinctive();
    route(key(INSERT, true));   // Insert -> save prompt
    route(key(ENTER, true));           // confirm

    PresetSlot saved = loadPreset(storage_, 0, 0);
    EXPECT_EQ(saved.chord.velocity, 111);
    EXPECT_EQ(saved.strum.velocity, 77);
    EXPECT_EQ(saved.rhythm.tempo, 234);
    EXPECT_FALSE(state_.dirty);
}

TEST_F(PresetEngineTest, SavePromptBackspaceCancels) {
    setDistinctive();
    route(key(INSERT, true));
    route(key(BACKSPACE, true));   // cancel
    route(key(ENTER, true));       // no prompt active -> inert

    EXPECT_FALSE(storage_.exists("/presets/bank1.json"));
}

TEST_F(PresetEngineTest, SavePromptEscCancels) {
    setDistinctive();
    route(key(INSERT, true));
    route(key(ESC, true));   // cancel
    route(key(ENTER, true));

    EXPECT_FALSE(storage_.exists("/presets/bank1.json"));
}

TEST_F(PresetEngineTest, SavePromptAutoCancels) {
    setDistinctive();
    route(key(INSERT, true), 0);

    engine_->update(5000ULL * 1000 + 1);   // prompt auto-cancel after 5s

    route(key(ENTER, true));   // no prompt -> no save
    EXPECT_FALSE(storage_.exists("/presets/bank1.json"));
}

TEST_F(PresetEngineTest, PlayKeyCancelsPrompt) {
    setDistinctive();
    route(key(INSERT, true));   // save prompt

    EXPECT_FALSE(route(key(0x17, true)));   // chord key cancels + forwards

    route(key(ENTER, true));   // no prompt -> no save
    EXPECT_FALSE(storage_.exists("/presets/bank1.json"));
}

TEST_F(PresetEngineTest, ClearResetsLiveParamsAndWritesDefaults) {
    state_.pendingChord.velocity  = 111;
    state_.pendingStrum.velocity  = 77;
    state_.pendingRhythm.tempo    = 234;

    route(key(DELETE, true));   // Delete -> clear prompt
    route(key(ENTER, true));           // confirm

    EXPECT_EQ(state_.pendingChord.velocity, ChordParams::defaults().velocity);
    EXPECT_EQ(state_.pendingStrum.velocity, StrumParams::defaults().velocity);
    EXPECT_EQ(state_.pendingRhythm.tempo, RhythmParams::defaults().tempo);
    EXPECT_FALSE(state_.dirty);

    PresetSlot stored = loadPreset(storage_, 0, 0);
    EXPECT_TRUE(stored.sameParams(PresetSlot::defaults()));
}

TEST_F(PresetEngineTest, DirtyAppearsOnEditAndClearsOnSave) {
    EXPECT_FALSE(state_.dirty);

    route(key(0x3E, true));   // F5 -> voicing Smart (differs from default)
    EXPECT_EQ(state_.pendingChord.voicing_mode, VoicingMode::Smart);
    EXPECT_TRUE(state_.dirty);

    route(key(INSERT, true));   // save
    route(key(ENTER, true));
    EXPECT_FALSE(state_.dirty);
}

TEST_F(PresetEngineTest, DirtyClearsOnLoad) {
    route(key(0x3E, true));   // voicing Smart -> dirty
    EXPECT_TRUE(state_.dirty);

    route(key(0x1E, true, SUPER));   // Super+1 -> reload slot 0 (default)
    EXPECT_FALSE(state_.dirty);
    EXPECT_EQ(state_.pendingChord.voicing_mode, VoicingMode::RootPosition);
}

TEST_F(PresetEngineTest, DirtyUnaffectedByCursorMovement) {
    route(key(0x3E, true));   // dirty
    EXPECT_TRUE(state_.dirty);

    route(key(END, true));    // browse cursor
    EXPECT_TRUE(state_.dirty);

    route(key(ESC, true));    // exit cursor
    EXPECT_TRUE(state_.dirty);
}

TEST_F(PresetEngineTest, StartupPresetLoads) {
    PresetSlot p = PresetSlot::defaults();
    p.rhythm.tempo = 180;
    savePreset(storage_, 2, 5, p);   // B3:P6

    state_.config.startup_preset = "B3:P6";
    engine_->loadStartupPreset();

    EXPECT_EQ(state_.currentBank, 2);
    EXPECT_EQ(state_.currentSlot, 5);
    EXPECT_EQ(state_.pendingRhythm.tempo, 180);
    EXPECT_FALSE(state_.dirty);
}

TEST_F(PresetEngineTest, StartupPresetFallsBackToB1P1) {
    state_.config.startup_preset = "garbage";
    engine_->loadStartupPreset();

    EXPECT_EQ(state_.currentBank, 0);
    EXPECT_EQ(state_.currentSlot, 0);
}
