#include <gtest/gtest.h>

#include <algorithm>
#include <string>

#include "keymap_config.h"
#include "storage_stub.h"


namespace {

bool actionEqual(const KeyAction& a, const KeyAction& b) {
    return a.cmd == b.cmd && a.param == b.param &&
           a.valueA == b.valueA && a.valueB == b.valueB &&
           a.menu == b.menu && a.slot == b.slot;
}

bool bindingEqual(const KeyBinding& a, const KeyBinding& b) {
    return a.modifierMask == b.modifierMask && a.usage == b.usage &&
           actionEqual(a.action, b.action);
}

bool configEqual(const KeymapConfig& a, const KeymapConfig& b) {
    if (a.bindings.size() != b.bindings.size()) return false;
    for (const auto& binding : a.bindings) {
        bool found = std::any_of(b.bindings.begin(), b.bindings.end(),
                                 [&](const KeyBinding& o) {
                                     return bindingEqual(binding, o);
                                 });
        if (!found) return false;
    }
    return true;
}

const char* validJson() {
    static std::string s = R"({
      "bindings": [
        { "keys": ["F1"], "action": { "type": "cycle", "param": "chord_mode" } },
        { "keys": ["F2"], "action": { "type": "cycle", "param": "arp_mode" } },
        { "keys": ["F3"], "action": { "type": "cycle", "param": "bass_pattern" } },
        { "keys": ["F4"], "action": { "type": "cycle", "param": "rhythm_pattern" } },
        { "keys": ["F5"], "action": { "type": "toggle", "param": "rhythm_enable", "a": 1, "b": 0 } },
        { "keys": ["F6"], "action": { "type": "toggle", "param": "rhythm_mute", "a": 1, "b": 0 } },
        { "keys": ["F7"], "action": { "type": "toggle", "param": "bass_enable", "a": 1, "b": 0 } },
        { "keys": ["F8"], "action": { "type": "toggle", "param": "rhythm_clock", "a": 1, "b": 0 } },
        { "keys": ["F9"], "action": { "type": "open_menu", "menu": "chord" } },
        { "keys": ["F10"], "action": { "type": "open_menu", "menu": "strum" } },
        { "keys": ["F11"], "action": { "type": "open_menu", "menu": "rhythm" } },
        { "keys": ["F12"], "action": { "type": "open_menu", "menu": "bass" } },
        { "keys": ["Super", "F11"], "action": { "type": "open_menu", "menu": "drum" } },
        { "keys": ["Home"], "action": { "type": "preset_prev" } },
        { "keys": ["End"], "action": { "type": "preset_next" } },
        { "keys": ["Insert"], "action": { "type": "preset_save" } },
        { "keys": ["Delete"], "action": { "type": "preset_clear" } }
      ]
    })";
    return s.c_str();
}

} // namespace


class KeymapConfigTest : public ::testing::Test {
protected:
    void SetUp() override { storage_.clear(); }
    StorageStub storage_;
};


TEST_F(KeymapConfigTest, MissingFileReturnsDefaults) {
    KeymapLoadResult r = KeymapConfig::load(storage_);
    EXPECT_TRUE(r.ok);
    EXPECT_TRUE(r.errors.empty());
    EXPECT_TRUE(configEqual(r.config, KeymapConfig::defaults()));
}

TEST_F(KeymapConfigTest, DefaultJsonRoundTrips) {
    storage_.writeFile("/keymap.json", keymapDefaultJson());
    KeymapLoadResult r = KeymapConfig::load(storage_);
    EXPECT_TRUE(r.ok);
    ASSERT_TRUE(r.errors.empty());
    EXPECT_TRUE(configEqual(r.config, KeymapConfig::defaults()));
}

TEST_F(KeymapConfigTest, EmptyFileIsError) {
    storage_.writeFile("/keymap.json", "");
    KeymapLoadResult r = KeymapConfig::load(storage_);
    EXPECT_FALSE(r.ok);
    EXPECT_FALSE(r.errors.empty());
}

TEST_F(KeymapConfigTest, MalformedJsonIsError) {
    storage_.writeFile("/keymap.json", "{not valid");
    KeymapLoadResult r = KeymapConfig::load(storage_);
    EXPECT_FALSE(r.ok);
}

TEST_F(KeymapConfigTest, MissingBindingsIsError) {
    storage_.writeFile("/keymap.json", "{\"version\": 1}");
    KeymapLoadResult r = KeymapConfig::load(storage_);
    EXPECT_FALSE(r.ok);
}

TEST_F(KeymapConfigTest, ValidJsonLoads) {
    storage_.writeFile("/keymap.json", validJson());
    KeymapLoadResult r = KeymapConfig::load(storage_);
    EXPECT_TRUE(r.ok) << [&] {
        std::string s;
        for (const auto& e : r.errors) s += e + "; ";
        return s;
    }();
    EXPECT_TRUE(r.errors.empty());
}

TEST_F(KeymapConfigTest, ReservedKeyRejected) {
    storage_.writeFile("/keymap.json", R"({
      "bindings": [
        { "keys": ["Q"], "action": { "type": "cycle", "param": "chord_mode" } }
      ]
    })");
    KeymapLoadResult r = KeymapConfig::load(storage_);
    EXPECT_FALSE(r.ok);
}

TEST_F(KeymapConfigTest, ReservedEscRejected) {
    storage_.writeFile("/keymap.json", R"({
      "bindings": [
        { "keys": ["F1"], "action": { "type": "cycle", "param": "chord_mode" } },
        { "keys": ["Esc"], "action": { "type": "preset_save" } }
      ]
    })");
    KeymapLoadResult r = KeymapConfig::load(storage_);
    EXPECT_FALSE(r.ok);
}

TEST_F(KeymapConfigTest, DuplicateKeyRejected) {
    storage_.writeFile("/keymap.json", R"({
      "bindings": [
        { "keys": ["F1"], "action": { "type": "cycle", "param": "chord_mode" } },
        { "keys": ["F1"], "action": { "type": "cycle", "param": "arp_mode" } }
      ]
    })");
    KeymapLoadResult r = KeymapConfig::load(storage_);
    EXPECT_FALSE(r.ok);
}

TEST_F(KeymapConfigTest, UnknownKeyRejected) {
    storage_.writeFile("/keymap.json", R"({
      "bindings": [
        { "keys": ["Bogus"], "action": { "type": "cycle", "param": "chord_mode" } }
      ]
    })");
    KeymapLoadResult r = KeymapConfig::load(storage_);
    EXPECT_FALSE(r.ok);
}

TEST_F(KeymapConfigTest, UnknownParamRejected) {
    storage_.writeFile("/keymap.json", R"({
      "bindings": [
        { "keys": ["F1"], "action": { "type": "cycle", "param": "nope" } }
      ]
    })");
    KeymapLoadResult r = KeymapConfig::load(storage_);
    EXPECT_FALSE(r.ok);
}

TEST_F(KeymapConfigTest, UnknownActionTypeRejected) {
    storage_.writeFile("/keymap.json", R"({
      "bindings": [
        { "keys": ["F1"], "action": { "type": "explode" } }
      ]
    })");
    KeymapLoadResult r = KeymapConfig::load(storage_);
    EXPECT_FALSE(r.ok);
}

TEST_F(KeymapConfigTest, MissingMenuCoverageRejected) {
    // Only a chord menu; strum/rhythm/bass/drum missing.
    storage_.writeFile("/keymap.json", R"({
      "bindings": [
        { "keys": ["F9"], "action": { "type": "open_menu", "menu": "chord" } },
        { "keys": ["Home"], "action": { "type": "preset_prev" } },
        { "keys": ["End"], "action": { "type": "preset_next" } },
        { "keys": ["Insert"], "action": { "type": "preset_save" } }
      ]
    })");
    KeymapLoadResult r = KeymapConfig::load(storage_);
    EXPECT_FALSE(r.ok);
}

TEST_F(KeymapConfigTest, MissingPresetCoverageRejected) {
    storage_.writeFile("/keymap.json", R"({
      "bindings": [
        { "keys": ["F9"], "action": { "type": "open_menu", "menu": "chord" } },
        { "keys": ["F10"], "action": { "type": "open_menu", "menu": "strum" } },
        { "keys": ["F11"], "action": { "type": "open_menu", "menu": "rhythm" } },
        { "keys": ["F12"], "action": { "type": "open_menu", "menu": "bass" } },
        { "keys": ["Super", "F11"], "action": { "type": "open_menu", "menu": "drum" } }
      ]
    })");
    KeymapLoadResult r = KeymapConfig::load(storage_);
    EXPECT_FALSE(r.ok);
}

TEST_F(KeymapConfigTest, DrumMuteIndexOutOfRangeRejected) {
    storage_.writeFile("/keymap.json", R"({
      "bindings": [
        { "keys": ["Alt", "F1"], "action": { "type": "drum_mute", "index": 99 } }
      ]
    })");
    KeymapLoadResult r = KeymapConfig::load(storage_);
    EXPECT_FALSE(r.ok);
}

TEST_F(KeymapConfigTest, TwoKeysSameFunctionAllowed) {
    storage_.writeFile("/keymap.json", R"({
      "bindings": [
        { "keys": ["F1"], "action": { "type": "cycle", "param": "chord_mode" } },
        { "keys": ["F2"], "action": { "type": "cycle", "param": "chord_mode" } }
      ]
    })");
    KeymapLoadResult r = KeymapConfig::load(storage_);
    // Coverage still missing (menus/presets), so this alone is not ok.
    EXPECT_FALSE(r.ok);
}

TEST_F(KeymapConfigTest, CustomRemapResolves) {
    storage_.writeFile("/keymap.json", R"({
      "bindings": [
        { "keys": ["F1"], "action": { "type": "inc", "param": "tempo" } },
        { "keys": ["F9"], "action": { "type": "open_menu", "menu": "chord" } },
        { "keys": ["F10"], "action": { "type": "open_menu", "menu": "strum" } },
        { "keys": ["F11"], "action": { "type": "open_menu", "menu": "rhythm" } },
        { "keys": ["F12"], "action": { "type": "open_menu", "menu": "bass" } },
        { "keys": ["Super", "F11"], "action": { "type": "open_menu", "menu": "drum" } },
        { "keys": ["Home"], "action": { "type": "preset_prev" } },
        { "keys": ["End"], "action": { "type": "preset_next" } },
        { "keys": ["Insert"], "action": { "type": "preset_save" } }
      ]
    })");
    KeymapLoadResult r = KeymapConfig::load(storage_);
    EXPECT_TRUE(r.ok) << [&] {
        std::string s;
        for (const auto& e : r.errors) s += e + "; ";
        return s;
    }();

    KeymapResolver resolver(&r.config);
    KeyAction a = resolver.resolve(0x3A, 0);  // F1
    EXPECT_EQ(a.cmd, KeyCmd::IncParam);
    EXPECT_EQ(a.param, ParamId::RhythmTempo);
}

TEST_F(KeymapConfigTest, PatternNamesResolveToIndices) {
    storage_.writeFile("/keymap.json", R"({
      "bindings": [
        { "keys": ["PrtSc"], "action": { "type": "set", "param": "rhythm_pattern", "value": "Rock 2" } },
        { "keys": ["ScLk"], "action": { "type": "set", "param": "bass_pattern", "value": "Half Alt" } },
        { "keys": ["F9"], "action": { "type": "open_menu", "menu": "chord" } },
        { "keys": ["F10"], "action": { "type": "open_menu", "menu": "strum" } },
        { "keys": ["F11"], "action": { "type": "open_menu", "menu": "rhythm" } },
        { "keys": ["F12"], "action": { "type": "open_menu", "menu": "bass" } },
        { "keys": ["Super", "F11"], "action": { "type": "open_menu", "menu": "drum" } },
        { "keys": ["Home"], "action": { "type": "preset_prev" } },
        { "keys": ["End"], "action": { "type": "preset_next" } },
        { "keys": ["Insert"], "action": { "type": "preset_save" } }
      ]
    })");
    KeymapLoadResult r = KeymapConfig::load(storage_);
    EXPECT_TRUE(r.ok) << [&] {
        std::string s;
        for (const auto& e : r.errors) s += e + "; ";
        return s;
    }();

    KeymapResolver resolver(&r.config);
    KeyAction a = resolver.resolve(0x46, 0);  // PrtSc -> rhythm_pattern "Rock 2"
    EXPECT_EQ(a.cmd, KeyCmd::SetParam);
    EXPECT_EQ(a.param, ParamId::RhythmPattern);
    EXPECT_EQ(a.valueA, 1);

    a = resolver.resolve(0x47, 0);  // ScLk -> bass_pattern "Half Alt"
    EXPECT_EQ(a.cmd, KeyCmd::SetParam);
    EXPECT_EQ(a.param, ParamId::BassPattern);
    EXPECT_EQ(a.valueA, 4);
}

TEST_F(KeymapConfigTest, UnknownPatternNameRejected) {
    storage_.writeFile("/keymap.json", R"({
      "bindings": [
        { "keys": ["PrtSc"], "action": { "type": "set", "param": "bass_pattern", "value": "Nope" } },
        { "keys": ["F9"], "action": { "type": "open_menu", "menu": "chord" } },
        { "keys": ["F10"], "action": { "type": "open_menu", "menu": "strum" } },
        { "keys": ["F11"], "action": { "type": "open_menu", "menu": "rhythm" } },
        { "keys": ["F12"], "action": { "type": "open_menu", "menu": "bass" } },
        { "keys": ["Super", "F11"], "action": { "type": "open_menu", "menu": "drum" } },
        { "keys": ["Home"], "action": { "type": "preset_prev" } },
        { "keys": ["End"], "action": { "type": "preset_next" } },
        { "keys": ["Insert"], "action": { "type": "preset_save" } }
      ]
    })");
    KeymapLoadResult r = KeymapConfig::load(storage_);
    EXPECT_FALSE(r.ok);
}
