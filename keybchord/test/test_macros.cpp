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


class MacroRecordingLcd : public LcdAdapter {
public:
    struct Frame {
        std::string l1;
        std::string l2;
    };

    bool begin() override { return true; }
    void write(const std::string& l1, const std::string& l2) override {
        frames.push_back({l1, l2});
    }
    void clear() override {}

    std::vector<Frame> frames;
};


TEST(Macros, DefaultsAreStrumAndEmpty) {
    MacroConfig cfg = MacroConfig::defaults();
    EXPECT_EQ(cfg.numberKeys, NumberKeyMode::Strum);
    for (int i = 0; i < MACRO_KEY_COUNT; i++) {
        EXPECT_TRUE(cfg.keys[i].empty());
    }
    EXPECT_EQ(cfg.nameFor(0), "M1");
    EXPECT_EQ(cfg.nameFor(8), "M9");
    EXPECT_EQ(cfg.nameFor(9), "M0");
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
    p.macros.names[0] = "Verse";
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
    EXPECT_EQ(loaded.macros.nameFor(0), "Verse");
    EXPECT_EQ(loaded.macros.nameFor(9), "M0");
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

TEST(Macros, LegacyArrayMacroGetsDefaultName) {
    StorageStub storage;
    storage.writeFile("/presets/bank1.json",
        "[{\"number_keys\":\"macro\",\"macros\":{\"1\":["
        "{\"type\":\"set\",\"param\":\"tempo\",\"value\":88}]}}]");

    PresetSlot p = loadPreset(storage, 0, 0);
    EXPECT_EQ(p.macros.nameFor(0), "M1");
    ASSERT_EQ(p.macros.keys[0].size(), 1u);
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

TEST(Macros, MidiCommandRoundTrip) {
    StorageStub storage;

    PresetSlot p = PresetSlot::defaults();
    MacroCommand midi;
    midi.op = MacroOp::Midi;
    midi.midiBytes = {0xF0, 0x41, 0x10, 0x42, 0x12, 0x18, 0x00, 0x00,
                      0x00, 0x01, 0x02, 0x03, 0x04, 0xF7};
    midi.delayMs = 250;
    p.macros.keys[3] = {midi};

    ASSERT_TRUE(savePreset(storage, 0, 4, p));
    PresetSlot loaded = loadPreset(storage, 0, 4);

    ASSERT_EQ(loaded.macros.keys[3].size(), 1u);
    EXPECT_EQ(loaded.macros.keys[3][0].op, MacroOp::Midi);
    EXPECT_EQ(loaded.macros.keys[3][0].midiBytes, midi.midiBytes);
    EXPECT_EQ(loaded.macros.keys[3][0].delayMs, 250);
}

TEST(Macros, LoadSkipsMalformedMidiCommands) {
    StorageStub storage;
    storage.writeFile("/presets/bank1.json",
        "[{\"number_keys\":\"macro\",\"macros\":{\"1\":["
        "{\"type\":\"midi\",\"bytes\":[]},"
        "{\"type\":\"midi\",\"bytes\":[1,2,300]},"
        "{\"type\":\"midi\",\"bytes\":[1,2,3],\"delay_ms\":-1},"
        "{\"type\":\"midi\",\"bytes\":[144,60,100]},"
        "{\"type\":\"midi\",\"bytes\":[176,7,42],\"delay_ms\":75}"
        "]}}]");

    PresetSlot p = loadPreset(storage, 0, 0);
    ASSERT_EQ(p.macros.keys[0].size(), 2u);
    EXPECT_EQ(p.macros.keys[0][0].midiBytes,
              (std::vector<uint8_t>{144, 60, 100}));
    EXPECT_EQ(p.macros.keys[0][1].midiBytes,
              (std::vector<uint8_t>{176, 7, 42}));
    EXPECT_EQ(p.macros.keys[0][1].delayMs, 75);
}

TEST(Macros, LoadKeepsMidiSysExBytes) {
    StorageStub storage;
    storage.writeFile("/presets/bank1.json",
        "[{\"number_keys\":\"macro\",\"macros\":{\"1\":["
        "{\"type\":\"midi\",\"bytes\":[240,65,16,0,0,0,14,18,2,0,0,42,1,83,247]},"
        "{\"type\":\"midi\",\"bytes\":[176,0,85]},"
        "{\"type\":\"midi\",\"bytes\":[176,32,0],\"delay_ms\":25},"
        "{\"type\":\"midi\",\"bytes\":[192,0],\"delay_ms\":25}"
        "]}}]");

    PresetSlot p = loadPreset(storage, 0, 0);
    ASSERT_EQ(p.macros.keys[0].size(), 4u);
    EXPECT_EQ(p.macros.keys[0][0].midiBytes,
              (std::vector<uint8_t>{240, 65, 16, 0, 0, 0, 14, 18,
                                    2, 0, 0, 42, 1, 83, 247}));
    EXPECT_EQ(p.macros.keys[0][2].delayMs, 25);
    EXPECT_EQ(p.macros.keys[0][3].delayMs, 25);
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
        lcd_.frames.clear();
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
    MacroRecordingLcd lcd_;
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

TEST_F(MacroTriggerTest, ShowsNamedMacroLoaded) {
    state_.pendingMacros.names[0] = "Verse";
    MacroCommand cmd;
    cmd.op = MacroOp::Set;
    cmd.param = ParamId::RhythmTempo;
    cmd.valueA = 88;
    state_.pendingMacros.keys[0].push_back(cmd);

    edit_->triggerMacro(0, 0);
    display_->update(0);

    ASSERT_FALSE(lcd_.frames.empty());
    EXPECT_EQ(lcd_.frames.back().l1.rfind("Macro Verse", 0), 0u);
    EXPECT_EQ(lcd_.frames.back().l2.rfind("loaded", 0), 0u);
}

TEST_F(MacroTriggerTest, ShowsDefaultMacroNameLoaded) {
    MacroCommand cmd;
    cmd.op = MacroOp::Set;
    cmd.param = ParamId::RhythmTempo;
    cmd.valueA = 90;
    state_.pendingMacros.keys[1].push_back(cmd);

    edit_->triggerMacro(1, 0);
    display_->update(0);

    ASSERT_FALSE(lcd_.frames.empty());
    EXPECT_EQ(lcd_.frames.back().l1.rfind("Macro M2 loaded", 0), 0u);
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

TEST_F(MacroTriggerTest, SendsMidiCommandsWithCumulativeDelay) {
    std::vector<std::vector<uint8_t>> sent;
    edit_->setMidiSendCallback(
        [&sent](const std::vector<uint8_t>& bytes) { sent.push_back(bytes); });

    MacroConfig cfg;
    MacroCommand first;
    first.op = MacroOp::Midi;
    first.midiBytes = {176, 7, 100};
    MacroCommand second = first;
    second.midiBytes = {144, 60, 100};
    second.delayMs = 100;
    MacroCommand third = first;
    third.midiBytes = {128, 60, 0};
    third.delayMs = 100;
    cfg.keys[0] = {first, second, third};
    state_.pendingMacros = cfg;

    edit_->triggerMacro(0, 1000);
    ASSERT_EQ(sent.size(), 1u);
    EXPECT_EQ(sent[0], (std::vector<uint8_t>{176, 7, 100}));

    edit_->update(101000 - 1);
    EXPECT_EQ(sent.size(), 1u);
    edit_->update(101000);
    ASSERT_EQ(sent.size(), 2u);
    EXPECT_EQ(sent[1], (std::vector<uint8_t>{144, 60, 100}));

    edit_->update(201000);
    ASSERT_EQ(sent.size(), 3u);
    EXPECT_EQ(sent[2], (std::vector<uint8_t>{128, 60, 0}));
}

TEST_F(MacroTriggerTest, EmptyOrOutOfRangeMacroIsHarmless) {
    state_.pendingRhythm.tempo = 123;

    edit_->triggerMacro(0, 0);
    edit_->triggerMacro(99, 0);

    EXPECT_EQ(state_.pendingRhythm.tempo, 123);
}
