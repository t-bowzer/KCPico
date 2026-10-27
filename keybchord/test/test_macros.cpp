#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "adapters_null.h"
#include "display_manager.h"
#include "edit_engine.h"
#include "presets.h"
#include "state.h"
#include "storage_stub.h"


namespace {

MacroCommand command(MacroOp op, ParamId param, int16_t a = 0, int16_t b = 0) {
    MacroCommand cmd;
    cmd.op = op;
    cmd.param = param;
    cmd.valueA = a;
    cmd.valueB = b;
    return cmd;
}

} // namespace


TEST(Macros, DefaultsAreStrumAndEmpty) {
    MacroConfig cfg = MacroConfig::defaults();
    EXPECT_EQ(cfg.numberKeys, NumberKeyMode::Strum);
    for (int i = 0; i < MACRO_KEY_COUNT; i++) {
        EXPECT_TRUE(cfg.keys[i].empty());
    }
}

TEST(Macros, KeyLabelsAndDrumPieces) {
    EXPECT_STREQ(macroKeyLabel(0), "1");
    EXPECT_STREQ(macroKeyLabel(9), "0");
    EXPECT_EQ(macroKeyIndex("0"), 9);
    EXPECT_EQ(macroKeyIndex("bogus"), -1);

    EXPECT_STREQ(macroDrumPieceKey(0), "kick");
    EXPECT_EQ(macroDrumPieceIndex("kick"), 0);
    EXPECT_EQ(macroDrumPieceVelocityParam(0), ParamId::DrumKickVel);
    EXPECT_EQ(macroDrumPieceIndex("bogus"), -1);
}

TEST(Macros, PresetRoundTrip) {
    StorageStub storage;

    PresetSlot p = PresetSlot::defaults();
    p.macros.numberKeys = NumberKeyMode::Macro;
    p.macros.keys[0] = {
        command(MacroOp::Set, ParamId::RhythmMute, 0),
        command(MacroOp::Toggle, ParamId::RhythmEnable, 1, 0),
        command(MacroOp::Cycle, ParamId::ChordMode),
        command(MacroOp::Inc, ParamId::RhythmTempo),
        command(MacroOp::Dec, ParamId::RhythmTempo),
    };
    MacroCommand mute;
    mute.op = MacroOp::DrumMute;
    mute.param = ParamId::DrumKickVel;
    mute.drumPiece = 0;
    mute.valueA = 1;
    p.macros.keys[9] = {mute};

    ASSERT_TRUE(savePreset(storage, 0, 2, p));
    PresetSlot loaded = loadPreset(storage, 0, 2);

    EXPECT_EQ(loaded.macros.numberKeys, NumberKeyMode::Macro);
    ASSERT_EQ(loaded.macros.keys[0].size(), 5u);
    EXPECT_EQ(loaded.macros.keys[0][0].op, MacroOp::Set);
    EXPECT_EQ(loaded.macros.keys[0][0].param, ParamId::RhythmMute);
    EXPECT_EQ(loaded.macros.keys[0][0].valueA, 0);
    EXPECT_EQ(loaded.macros.keys[0][1].op, MacroOp::Toggle);
    EXPECT_EQ(loaded.macros.keys[0][1].valueA, 1);
    EXPECT_EQ(loaded.macros.keys[0][1].valueB, 0);
    EXPECT_EQ(loaded.macros.keys[0][2].op, MacroOp::Cycle);
    EXPECT_EQ(loaded.macros.keys[0][3].op, MacroOp::Inc);
    EXPECT_EQ(loaded.macros.keys[0][4].op, MacroOp::Dec);
    ASSERT_EQ(loaded.macros.keys[9].size(), 1u);
    EXPECT_EQ(loaded.macros.keys[9][0].op, MacroOp::DrumMute);
    EXPECT_EQ(loaded.macros.keys[9][0].drumPiece, 0);
    EXPECT_EQ(loaded.macros.keys[9][0].valueA, 1);
}

TEST(Macros, OldPresetLoadsAsStrumWithNoMacros) {
    StorageStub storage;
    storage.writeFile("/presets/bank1.json",
                      "[{\"name\":\"Old\",\"chord\":{\"velocity\":80}}]");

    PresetSlot p = loadPreset(storage, 0, 0);
    EXPECT_EQ(p.name, "Old");
    EXPECT_EQ(p.macros.numberKeys, NumberKeyMode::Strum);
    for (int i = 0; i < MACRO_KEY_COUNT; i++) {
        EXPECT_TRUE(p.macros.keys[i].empty());
    }
}

TEST(Macros, LoadEnforcesPerKeyAndTotalCaps) {
    StorageStub storage;

    std::string cmds;
    for (int i = 0; i < 20; i++) {
        if (i) cmds += ",";
        cmds += "{\"type\":\"set\",\"param\":\"tempo\",\"value\":88}";
    }

    std::string macros = "{";
    for (int i = 0; i < MACRO_KEY_COUNT; i++) {
        if (i) macros += ",";
        macros += "\"" + std::string(macroKeyLabel(i)) + "\":[" + cmds + "]";
    }
    macros += "}";

    storage.writeFile("/presets/bank1.json",
        "[{\"number_keys\":\"macro\",\"macros\":" + macros + "}]");

    PresetSlot p = loadPreset(storage, 0, 0);
    EXPECT_EQ(p.macros.numberKeys, NumberKeyMode::Macro);

    int total = 0;
    for (int i = 0; i < MACRO_KEY_COUNT; i++) {
        EXPECT_LE(static_cast<int>(p.macros.keys[i].size()),
                  MAX_MACRO_COMMANDS_PER_KEY);
        total += static_cast<int>(p.macros.keys[i].size());
    }
    EXPECT_LE(total, MAX_MACRO_COMMANDS_PER_PRESET);
    EXPECT_EQ(MAX_MACRO_COMMANDS_PER_PRESET, total);
}

TEST(Macros, LoadSkipsMalformedCommands) {
    StorageStub storage;
    storage.writeFile("/presets/bank1.json",
        "[{\"number_keys\":\"macro\",\"macros\":{\"1\":["
        "{\"type\":\"set\",\"param\":\"nope\",\"value\":1},"
        "{\"type\":\"set\",\"param\":\"tempo\",\"value\":88},"
        "{\"type\":\"drum_mute\",\"piece\":\"kick\",\"value\":\"on\"},"
        "{\"type\":\"drum_mute\",\"piece\":\"nope\",\"value\":\"on\"}"
        "]}}]");

    PresetSlot p = loadPreset(storage, 0, 0);
    ASSERT_EQ(p.macros.keys[0].size(), 2u);
    EXPECT_EQ(p.macros.keys[0][0].param, ParamId::RhythmTempo);
    EXPECT_EQ(p.macros.keys[0][1].op, MacroOp::DrumMute);
    EXPECT_EQ(p.macros.keys[0][1].param, ParamId::DrumKickVel);
}


class MacroTriggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        state_.loadDefaults();
        display_ = new DisplayManager(state_, lcd_);
        edit_ = new EditEngine(state_, *display_);
        modeChanged_ = 0;
        arpChanged_ = 0;
        patternChanged_ = 0;
        edit_->setModeChangedCallback([this]() { modeChanged_++; });
        edit_->setArpModeChangedCallback([this]() { arpChanged_++; });
        edit_->setPatternChangedCallback([this]() { patternChanged_++; });
    }

    void TearDown() override {
        delete edit_;
        delete display_;
    }

    StateManager state_;
    NullLcdAdapter lcd_;
    DisplayManager* display_ = nullptr;
    EditEngine* edit_ = nullptr;
    int modeChanged_ = 0;
    int arpChanged_ = 0;
    int patternChanged_ = 0;
};


TEST_F(MacroTriggerTest, AppliesSetToggleAndDrumMuteInOrder) {
    state_.pendingRhythm.enabled = true;
    state_.pendingRhythm.muted = true;
    state_.pendingRhythm.tempo = 120;

    MacroConfig cfg;
    cfg.numberKeys = NumberKeyMode::Macro;
    cfg.keys[0] = {
        command(MacroOp::Set, ParamId::RhythmMute, 0),
        command(MacroOp::Set, ParamId::RhythmTempo, 88),
        command(MacroOp::Toggle, ParamId::RhythmEnable, 1, 0),
    };
    MacroCommand mute;
    mute.op = MacroOp::DrumMute;
    mute.param = ParamId::DrumKickVel;
    mute.drumPiece = 0;
    mute.valueA = 1;
    cfg.keys[0].push_back(mute);
    state_.pendingMacros = cfg;

    edit_->triggerMacro(0, 0);

    EXPECT_FALSE(state_.pendingRhythm.muted);
    EXPECT_EQ(state_.pendingRhythm.tempo, 88);
    EXPECT_FALSE(state_.pendingRhythm.enabled);
    EXPECT_EQ(state_.pendingRhythm.drums.kick_vel,
              param_bounds::DRUM_VELOCITY_OFF);
}

TEST_F(MacroTriggerTest, AppliesCycleIncAndDec) {
    state_.pendingChord.play_mode = PlayMode::Held;
    state_.pendingRhythm.tempo = 100;

    MacroConfig cfg;
    cfg.keys[1] = {
        command(MacroOp::Cycle, ParamId::ChordMode),
        command(MacroOp::Inc, ParamId::RhythmTempo),
        command(MacroOp::Dec, ParamId::RhythmTempo),
        command(MacroOp::Inc, ParamId::RhythmTempo),
    };
    state_.pendingMacros = cfg;

    edit_->triggerMacro(1, 0);

    EXPECT_EQ(state_.pendingChord.play_mode, PlayMode::PressToPlay);
    EXPECT_EQ(state_.pendingRhythm.tempo, 101);
    EXPECT_EQ(patternChanged_, 0);
    EXPECT_EQ(arpChanged_, 0);
}

TEST_F(MacroTriggerTest, FiresCallbacksAfterApplyingAllCommands) {
    state_.pendingChord.play_mode = PlayMode::Held;

    MacroConfig cfg;
    cfg.keys[2] = {
        command(MacroOp::Set, ParamId::ChordMode,
                static_cast<int16_t>(PlayMode::Arpeggio)),
        command(MacroOp::Set, ParamId::ChordArpMode,
                static_cast<int16_t>(ArpMode::Down)),
    };
    state_.pendingMacros = cfg;

    edit_->triggerMacro(2, 0);

    EXPECT_EQ(state_.pendingChord.play_mode, PlayMode::Arpeggio);
    EXPECT_EQ(state_.pendingChord.arp_mode, ArpMode::Down);
    EXPECT_EQ(modeChanged_, 1);
    EXPECT_EQ(arpChanged_, 1);
}

TEST_F(MacroTriggerTest, EmptyOrOutOfRangeMacroIsHarmless) {
    state_.pendingRhythm.tempo = 123;

    edit_->triggerMacro(0, 0);
    edit_->triggerMacro(99, 0);

    EXPECT_EQ(state_.pendingRhythm.tempo, 123);
}
