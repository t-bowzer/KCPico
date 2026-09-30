#include "keymap_config.h"

#include <ArduinoJson.h>

#include <algorithm>
#include <cstdint>

#include "base.h"
#include "bass.h"
#include "keymap.h"
#include "rhythm.h"


static const char* KEYMAP_PATH = "/keymap.json";

namespace {

// --- HID usage constants for configurable keys ---
constexpr uint8_t HID_USAGE_ESC       = 0x29;
constexpr uint8_t HID_USAGE_BACKSPACE = 0x2A;
constexpr uint8_t HID_USAGE_ENTER     = 0x28;
constexpr uint8_t HID_USAGE_SPACE     = 0x2C;
constexpr uint8_t HID_USAGE_MINUS     = 0x2D;
constexpr uint8_t HID_USAGE_EQUALS    = 0x2E;
constexpr uint8_t HID_USAGE_F1        = 0x3A;  // F1..F12 = 0x3A..0x45
constexpr uint8_t HID_USAGE_PRTSC     = 0x46;
constexpr uint8_t HID_USAGE_SCLK      = 0x47;
constexpr uint8_t HID_USAGE_PAUSE     = 0x48;
constexpr uint8_t HID_USAGE_INSERT    = 0x49;
constexpr uint8_t HID_USAGE_HOME      = 0x4A;
constexpr uint8_t HID_USAGE_PGUP      = 0x4B;
constexpr uint8_t HID_USAGE_DELETE    = 0x4C;
constexpr uint8_t HID_USAGE_END       = 0x4D;
constexpr uint8_t HID_USAGE_PGDN      = 0x4E;
constexpr uint8_t HID_USAGE_KP_MINUS  = 0x56;
constexpr uint8_t HID_USAGE_KP_PLUS   = 0x57;
constexpr uint8_t HID_USAGE_KP_ENTER  = 0x58;
constexpr uint8_t HID_USAGE_RIGHT     = 0x4F;
constexpr uint8_t HID_USAGE_LEFT      = 0x50;
constexpr uint8_t HID_USAGE_DOWN      = 0x51;
constexpr uint8_t HID_USAGE_UP        = 0x52;

// --- Key name table (configurable keys only; modifiers handled separately) ---
struct KeyName {
    const char* name;
    uint8_t     usage;
};

const KeyName kKeyNames[] = {
    {"F1", 0x3A}, {"F2", 0x3B}, {"F3", 0x3C}, {"F4", 0x3D},
    {"F5", 0x3E}, {"F6", 0x3F}, {"F7", 0x40}, {"F8", 0x41},
    {"F9", 0x42}, {"F10", 0x43}, {"F11", 0x44}, {"F12", 0x45},
    {"PrtSc", 0x46}, {"ScLk", 0x47}, {"Pause", 0x48},
    {"Insert", 0x49}, {"Home", 0x4A}, {"PgUp", 0x4B},
    {"Delete", 0x4C}, {"End", 0x4D}, {"PgDn", 0x4E},
    {"Space", 0x2C}, {"=", 0x2E}, {"-", 0x2D},
    {"KpPlus", 0x57}, {"KpMinus", 0x56}, {"KpEnter", 0x58},
    {"Left", 0x50}, {"Down", 0x51}, {"Right", 0x4F}, {"Up", 0x52},
    {"1", 0x1E}, {"2", 0x1F}, {"3", 0x20}, {"4", 0x21},
    {"5", 0x22}, {"6", 0x23}, {"7", 0x24}, {"8", 0x25},
};

bool lookupKeyName(const std::string& name, uint8_t& usage) {
    for (const auto& k : kKeyNames) {
        if (name == k.name) { usage = k.usage; return true; }
    }
    return false;
}

bool lookupModifier(const std::string& name, uint8_t& mask) {
    if (name == "Ctrl")  { mask = KMOD_CTRL;  return true; }
    if (name == "Alt")   { mask = KMOD_ALT;   return true; }
    if (name == "Super") { mask = KMOD_SUPER; return true; }
    return false;
}

// --- Param name table (configurable parameters) ---
struct ParamName {
    const char* name;
    ParamId     id;
};

const ParamName kParamNames[] = {
    {"chord_octave", ParamId::ChordOctave},
    {"chord_mode", ParamId::ChordMode},
    {"chord_voicing", ParamId::ChordVoicing},
    {"chord_duration", ParamId::ChordDuration},
    {"chord_velocity", ParamId::ChordVelocity},
    {"chord_pan", ParamId::ChordPan},
    {"chord_roll", ParamId::ChordRoll},
    {"chord_min_notes", ParamId::ChordMinNotes},
    {"chord_min_interval", ParamId::ChordMinInterval},
    {"chord_inversion", ParamId::ChordInversion},
    {"arp_mode", ParamId::ChordArpMode},
    {"strum_octave", ParamId::StrumOctave},
    {"strum_duration", ParamId::StrumDuration},
    {"strum_velocity", ParamId::StrumVelocity},
    {"strum_layout", ParamId::StrumLayout},
    {"strum_mode", ParamId::StrumMode},
    {"strum_root", ParamId::StrumRoot},
    {"strum_scale", ParamId::StrumScale},
    {"tempo", ParamId::RhythmTempo},
    {"swing", ParamId::RhythmSwing},
    {"rhythm_pattern", ParamId::RhythmPattern},
    {"rhythm_mute", ParamId::RhythmMute},
    {"rhythm_enable", ParamId::RhythmEnable},
    {"rhythm_clock", ParamId::RhythmClock},
    {"rhythm_led", ParamId::RhythmLed},
    {"bass_enable", ParamId::BassEnable},
    {"bass_octave", ParamId::BassOctave},
    {"bass_duration", ParamId::BassDuration},
    {"bass_velocity", ParamId::BassVelocity},
    {"bass_channel", ParamId::BassChannel},
    {"bass_pattern", ParamId::BassPattern},
};

bool lookupParamName(const std::string& name, ParamId& id) {
    for (const auto& p : kParamNames) {
        if (name == p.name) { id = p.id; return true; }
    }
    return false;
}

// --- Enum / bool value name tables ---
struct ValueName {
    const char* name;
    int         value;
};

const ValueName kPlayModeNames[] = {
    {"held", 0}, {"press", 1}, {"arpeggio", 2}, {"arp_hold", 3}, {"silent", 4},
};
const ValueName kVoicingNames[] = {
    {"root", 0}, {"smart", 1}, {"down", 2}, {"up", 3},
};
const ValueName kInversionNames[] = {
    {"root", 0}, {"first", 1}, {"second", 2}, {"third", 3},
};
const ValueName kArpNames[] = {
    {"up", 0}, {"down", 1}, {"up_down", 2}, {"alternating", 3}, {"random", 4},
};
const ValueName kStrumModeNames[] = {
    {"chord", 0}, {"scale", 1}, {"piano", 2},
};
const ValueName kScaleNames[] = {
    {"ionian", 0}, {"dorian", 1}, {"phrygian", 2}, {"lydian", 3},
    {"mixolydian", 4}, {"aeolian", 5}, {"locrian", 6},
    {"harmonic_minor", 7}, {"melodic_minor", 8},
    {"major_pentatonic", 9}, {"minor_pentatonic", 10}, {"blues", 11},
};

bool lookupValue(const ValueName* table, size_t n, const std::string& s, int& out) {
    for (size_t i = 0; i < n; i++) {
        if (s == table[i].name) { out = table[i].value; return true; }
    }
    return false;
}

bool parseInteger(const std::string& s, int& out) {
    if (s.empty()) return false;
    size_t i = 0;
    bool neg = false;
    if (s[0] == '-' || s[0] == '+') { neg = (s[0] == '-'); i = 1; }
    if (i >= s.size()) return false;
    int v = 0;
    for (; i < s.size(); i++) {
        if (s[i] < '0' || s[i] > '9') return false;
        v = v * 10 + (s[i] - '0');
    }
    out = neg ? -v : v;
    return true;
}

bool isBoolParam(ParamId id) {
    switch (id) {
        case ParamId::StrumLayout:
        case ParamId::RhythmMute:
        case ParamId::RhythmEnable:
        case ParamId::RhythmClock:
        case ParamId::RhythmLed:
        case ParamId::BassEnable:
            return true;
        default:
            return false;
    }
}

// Parses a value string for `id` into an int (enum index, bool 0/1, or int).
bool parseValue(ParamId id, const std::string& s, int& out) {
    if (parseInteger(s, out)) return true;

    if (isBoolParam(id)) {
        if (s == "on")  { out = 1; return true; }
        if (s == "off") { out = 0; return true; }
        if (s == "true")  { out = 1; return true; }
        if (s == "false") { out = 0; return true; }
        return false;
    }

    switch (id) {
        case ParamId::ChordMode:
            return lookupValue(kPlayModeNames, 5, s, out);
        case ParamId::ChordVoicing:
            return lookupValue(kVoicingNames, 4, s, out);
        case ParamId::ChordInversion:
            return lookupValue(kInversionNames, 4, s, out);
        case ParamId::ChordArpMode:
            return lookupValue(kArpNames, 5, s, out);
        case ParamId::StrumMode:
            return lookupValue(kStrumModeNames, 3, s, out);
        case ParamId::StrumScale:
            return lookupValue(kScaleNames, 12, s, out);
        case ParamId::RhythmPattern: {
            int idx = rhythmIndex(s);
            if (idx < 0) return false;
            out = idx;
            return true;
        }
        case ParamId::BassPattern: {
            int idx = bassIndex(s);
            if (idx < 0) return false;
            out = idx;
            return true;
        }
        default:
            return false;
    }
}

// --- Menu name table ---
bool lookupMenuName(const std::string& s, EditMenu& menu) {
    if (s == "chord")  { menu = EditMenu::Chord;  return true; }
    if (s == "strum")  { menu = EditMenu::Strum;  return true; }
    if (s == "rhythm") { menu = EditMenu::Rhythm; return true; }
    if (s == "bass")   { menu = EditMenu::Bass;   return true; }
    if (s == "drum")   { menu = EditMenu::Drum;   return true; }
    return false;
}

// --- Validation helpers ---
bool bindingHasCmd(const std::vector<KeyBinding>& bindings, KeyCmd cmd) {
    for (const auto& b : bindings) {
        if (b.action.cmd == cmd) return true;
    }
    return false;
}

void addCoverageError(std::vector<std::string>& errors, bool present, const char* what) {
    if (!present) errors.push_back(std::string("missing required binding: ") + what);
}

} // namespace


const KeyAction* KeymapConfig::find(uint8_t modifierMask, uint8_t usage) const {
    for (const auto& b : bindings) {
        if (b.modifierMask == modifierMask && b.usage == usage) return &b.action;
    }
    return nullptr;
}

KeymapConfig KeymapConfig::defaults() {
    KeymapConfig cfg;
    std::vector<KeyBinding>& b = cfg.bindings;

    auto bind = [&](uint8_t mods, uint8_t usage, KeyAction a) {
        KeyBinding kb;
        kb.modifierMask = mods;
        kb.usage = usage;
        kb.action = a;
        b.push_back(kb);
    };

    auto cycle = [](ParamId p) { KeyAction a; a.cmd = KeyCmd::CycleParam; a.param = p; return a; };
    auto toggle = [](ParamId p, int va, int vb) {
        KeyAction a; a.cmd = KeyCmd::ToggleParam; a.param = p; a.valueA = va; a.valueB = vb;
        return a;
    };
    auto setp = [](ParamId p, int v) { KeyAction a; a.cmd = KeyCmd::SetParam; a.param = p; a.valueA = v; return a; };
    auto incp = [](ParamId p) { KeyAction a; a.cmd = KeyCmd::IncParam; a.param = p; return a; };
    auto decp = [](ParamId p) { KeyAction a; a.cmd = KeyCmd::DecParam; a.param = p; return a; };
    auto openMenu = [](EditMenu m) { KeyAction a; a.cmd = KeyCmd::OpenMenu; a.menu = m; return a; };
    auto preset = [](KeyCmd c) { KeyAction a; a.cmd = c; return a; };
    auto ext = [](KeyCmd c) { KeyAction a; a.cmd = c; return a; };
    auto drumMute = [](uint8_t idx) { KeyAction a; a.cmd = KeyCmd::DrumMute; a.slot = idx; return a; };

    // F1-F4: most common settings.
    bind(0, HID_USAGE_F1, cycle(ParamId::ChordMode));
    bind(0, HID_USAGE_F1 + 1, cycle(ParamId::ChordArpMode));       // F2
    bind(0, HID_USAGE_F1 + 2, cycle(ParamId::BassPattern));        // F3
    bind(0, HID_USAGE_F1 + 3, cycle(ParamId::RhythmPattern));      // F4

    // F5-F8: on/off toggles.
    bind(0, HID_USAGE_F1 + 4, toggle(ParamId::RhythmEnable, 1, 0)); // F5
    bind(0, HID_USAGE_F1 + 5, toggle(ParamId::RhythmMute, 1, 0));   // F6
    bind(0, HID_USAGE_F1 + 6, toggle(ParamId::BassEnable, 1, 0));   // F7
    bind(0, HID_USAGE_F1 + 7, toggle(ParamId::RhythmClock, 1, 0));  // F8

    // F9-F12: menus.
    bind(0, HID_USAGE_F1 + 8, openMenu(EditMenu::Chord));   // F9
    bind(0, HID_USAGE_F1 + 9, openMenu(EditMenu::Strum));   // F10
    bind(0, HID_USAGE_F1 + 10, openMenu(EditMenu::Rhythm)); // F11
    bind(0, HID_USAGE_F1 + 11, openMenu(EditMenu::Bass));   // F12

    // Inversions.
    bind(0, HID_USAGE_PRTSC, setp(ParamId::ChordInversion, 1));  // First
    bind(0, HID_USAGE_SCLK,  setp(ParamId::ChordInversion, 2));  // Second
    bind(0, HID_USAGE_PAUSE, setp(ParamId::ChordInversion, 3));  // Third

    // Octaves.
    bind(0, HID_USAGE_EQUALS, incp(ParamId::ChordOctave));
    bind(0, HID_USAGE_MINUS,  decp(ParamId::ChordOctave));
    bind(0, HID_USAGE_KP_PLUS,  incp(ParamId::StrumOctave));
    bind(0, HID_USAGE_KP_MINUS, decp(ParamId::StrumOctave));

    // Held extensions (arrow keys): add9/add11/add13.
    bind(0, HID_USAGE_LEFT,  ext(KeyCmd::Ext9));
    bind(0, HID_USAGE_DOWN,  ext(KeyCmd::Ext11));
    bind(0, HID_USAGE_RIGHT, ext(KeyCmd::Ext13));

    // Tempo.
    bind(0, HID_USAGE_PGUP, incp(ParamId::RhythmTempo));
    bind(0, HID_USAGE_PGDN, decp(ParamId::RhythmTempo));
    bind(0, HID_USAGE_SPACE, preset(KeyCmd::TapTempo));

    // Presets.
    bind(0, HID_USAGE_HOME,   preset(KeyCmd::PresetPrev));
    bind(0, HID_USAGE_END,    preset(KeyCmd::PresetNext));
    bind(0, HID_USAGE_INSERT, preset(KeyCmd::PresetSave));
    bind(0, HID_USAGE_DELETE, preset(KeyCmd::PresetClear));

    // Super combos.
    bind(KMOD_SUPER, HID_USAGE_HOME, preset(KeyCmd::PresetBankPrev));
    bind(KMOD_SUPER, HID_USAGE_END,  preset(KeyCmd::PresetBankNext));
    bind(KMOD_SUPER, HID_USAGE_F1 + 10, openMenu(EditMenu::Drum));  // Super+F11

    // Ctrl combos.
    bind(KMOD_CTRL, HID_USAGE_F1,     cycle(ParamId::ChordVoicing));
    bind(KMOD_CTRL, HID_USAGE_F1 + 1, cycle(ParamId::ChordInversion));
    bind(KMOD_CTRL, HID_USAGE_F1 + 2, cycle(ParamId::ChordArpMode));   // Ctrl+F3
    bind(KMOD_CTRL, HID_USAGE_F1 + 3, toggle(ParamId::ChordRoll, 0, 50)); // Ctrl+F4
    bind(KMOD_CTRL, HID_USAGE_F1 + 4, toggle(ParamId::RhythmLed, 1, 0));  // Ctrl+F5
    bind(KMOD_CTRL, HID_USAGE_EQUALS, incp(ParamId::ChordVelocity));
    bind(KMOD_CTRL, HID_USAGE_MINUS,  decp(ParamId::ChordVelocity));
    bind(KMOD_CTRL, HID_USAGE_KP_ENTER, cycle(ParamId::StrumLayout));
    bind(KMOD_CTRL, HID_USAGE_KP_PLUS,  incp(ParamId::StrumScale));
    bind(KMOD_CTRL, HID_USAGE_KP_MINUS, decp(ParamId::StrumScale));

    // Alt combos.
    bind(KMOD_ALT, HID_USAGE_KP_ENTER, cycle(ParamId::StrumMode));
    bind(KMOD_ALT, HID_USAGE_KP_PLUS,  incp(ParamId::StrumRoot));
    bind(KMOD_ALT, HID_USAGE_KP_MINUS, decp(ParamId::StrumRoot));
    bind(KMOD_ALT, HID_USAGE_EQUALS,   incp(ParamId::BassOctave));
    bind(KMOD_ALT, HID_USAGE_MINUS,    decp(ParamId::BassOctave));
    for (int i = 0; i < 12; i++) {
        bind(KMOD_ALT, HID_USAGE_F1 + i, drumMute(static_cast<uint8_t>(i)));
    }

    return cfg;
}

KeymapLoadResult KeymapConfig::load(StorageAdapter& storage) {
    KeymapLoadResult result;

    if (!storage.exists(KEYMAP_PATH)) {
        result.ok = true;
        result.config = KeymapConfig::defaults();
        return result;
    }

    std::string raw = storage.readFile(KEYMAP_PATH);
    if (raw.empty()) {
        result.ok = false;
        result.config = KeymapConfig::defaults();
        result.errors.push_back("keymap.json is empty");
        return result;
    }

    JsonDocument doc;
    auto error = deserializeJson(doc, raw);
    if (error) {
        result.ok = false;
        result.config = KeymapConfig::defaults();
        result.errors.push_back(std::string("invalid JSON: ") + error.c_str());
        return result;
    }
    if (!doc.is<JsonObject>() || !doc.containsKey("bindings") ||
        !doc["bindings"].is<JsonArray>()) {
        result.ok = false;
        result.config = KeymapConfig::defaults();
        result.errors.push_back("top-level 'bindings' array is required");
        return result;
    }

    KeymapConfig cfg;
    int bindIdx = 0;
    for (JsonVariant bv : doc["bindings"].as<JsonArray>()) {
        std::string where = "bindings[" + std::to_string(bindIdx) + "]";
        bindIdx++;

        if (!bv.is<JsonObject>()) {
            result.errors.push_back(where + ": not an object");
            result.ok = false;
            continue;
        }

        auto obj = bv.as<JsonObject>();

        // Parse the key list: exactly one non-modifier key + at most one modifier.
        if (!obj.containsKey("keys") || !obj["keys"].is<JsonArray>() ||
            obj["keys"].size() == 0 || obj["keys"].size() > 2) {
            result.errors.push_back(where + ": 'keys' must be 1 or 2 entries");
            result.ok = false;
            continue;
        }

        uint8_t mods = 0;
        uint8_t usage = 0;
        bool haveKey = false;
        bool keyErr = false;
        for (JsonVariant kv : obj["keys"].as<JsonArray>()) {
            if (!kv.is<const char*>()) { keyErr = true; break; }
            std::string name = kv.as<std::string>();
            uint8_t m;
            if (lookupModifier(name, m)) {
                if (mods != 0) { keyErr = true; break; }  // two modifiers
                mods = m;
            } else if (lookupKeyName(name, usage)) {
                if (haveKey) { keyErr = true; break; }    // two keys
                haveKey = true;
            } else {
                result.errors.push_back(where + ": unknown key '" + name + "'");
                result.ok = false;
                keyErr = true;
                break;
            }
        }
        if (keyErr) {
            result.ok = false;
            continue;
        }
        if (!haveKey) {
            result.errors.push_back(where + ": no non-modifier key");
            result.ok = false;
            continue;
        }

        // Reserved keys cannot be rebound.
        if (keymapIsReserved(usage)) {
            result.errors.push_back(where + ": key is reserved");
            result.ok = false;
            continue;
        }

        // Duplicate (modifier, usage) check.
        bool dup = false;
        for (const auto& existing : cfg.bindings) {
            if (existing.modifierMask == mods && existing.usage == usage) {
                dup = true;
                break;
            }
        }
        if (dup) {
            result.errors.push_back(where + ": key already bound");
            result.ok = false;
            continue;
        }

        // Parse the action.
        if (!obj.containsKey("action") || !obj["action"].is<JsonObject>()) {
            result.errors.push_back(where + ": 'action' object is required");
            result.ok = false;
            continue;
        }
        auto act = obj["action"].as<JsonObject>();
        if (!act.containsKey("type") || !act["type"].is<const char*>()) {
            result.errors.push_back(where + ": action 'type' is required");
            result.ok = false;
            continue;
        }
        std::string type = act["type"].as<std::string>();

        KeyAction a;
        bool actErr = false;
        auto needParam = [&](ParamId& out) {
            if (!act.containsKey("param") || !act["param"].is<const char*>()) {
                result.errors.push_back(where + ": action 'param' is required");
                result.ok = false;
                actErr = true;
                return;
            }
            if (!lookupParamName(act["param"].as<std::string>(), out)) {
                result.errors.push_back(where + ": unknown param '" +
                                        act["param"].as<std::string>() + "'");
                result.ok = false;
                actErr = true;
            }
        };
        auto needInt = [&](const char* field, int& out) {
            if (!act.containsKey(field) || !act[field].is<int>()) {
                result.errors.push_back(where + std::string(": action '") + field +
                                        "' (integer) is required");
                result.ok = false;
                actErr = true;
                return;
            }
            out = act[field].as<int>();
        };
        auto needValue = [&](const char* field, ParamId id, int& out) {
            if (!act.containsKey(field)) {
                result.errors.push_back(where + std::string(": action '") + field +
                                        "' is required");
                result.ok = false;
                actErr = true;
                return;
            }
            std::string s;
            if (act[field].is<const char*>()) {
                s = act[field].as<std::string>();
            } else if (act[field].is<int>()) {
                s = std::to_string(act[field].as<int>());
            } else {
                result.errors.push_back(where + std::string(": action '") + field +
                                        "' must be a string or integer");
                result.ok = false;
                actErr = true;
                return;
            }
            if (!parseValue(id, s, out)) {
                result.errors.push_back(where + std::string(": invalid value '") + s +
                                        "' for '" + field + "'");
                result.ok = false;
                actErr = true;
            }
        };

        if (type == "open_menu") {
            if (!act.containsKey("menu") || !act["menu"].is<const char*>() ||
                !lookupMenuName(act["menu"].as<std::string>(), a.menu)) {
                result.errors.push_back(where + ": invalid 'menu'");
                result.ok = false;
                actErr = true;
            } else {
                a.cmd = KeyCmd::OpenMenu;
            }
        } else if (type == "cycle") {
            needParam(a.param);
            a.cmd = KeyCmd::CycleParam;
        } else if (type == "set") {
            needParam(a.param);
            if (!actErr) needValue("value", a.param, a.valueA);
            a.cmd = KeyCmd::SetParam;
        } else if (type == "inc") {
            needParam(a.param);
            a.cmd = KeyCmd::IncParam;
        } else if (type == "dec") {
            needParam(a.param);
            a.cmd = KeyCmd::DecParam;
        } else if (type == "toggle") {
            needParam(a.param);
            if (!actErr) needValue("a", a.param, a.valueA);
            if (!actErr) needValue("b", a.param, a.valueB);
            a.cmd = KeyCmd::ToggleParam;
        } else if (type == "ext") {
            int v = 0;
            needInt("value", v);
            if (!actErr) {
                if (v == 9)       a.cmd = KeyCmd::Ext9;
                else if (v == 11) a.cmd = KeyCmd::Ext11;
                else if (v == 13) a.cmd = KeyCmd::Ext13;
                else {
                    result.errors.push_back(where + ": ext 'value' must be 9, 11, or 13");
                    result.ok = false;
                    actErr = true;
                }
            }
        } else if (type == "drum_mute") {
            int idx;
            if (!act.containsKey("index") || !act["index"].is<int>()) {
                result.errors.push_back(where + ": drum_mute 'index' is required");
                result.ok = false;
                actErr = true;
            } else {
                idx = act["index"].as<int>();
                if (idx < 0 || idx > 11) {
                    result.errors.push_back(where + ": drum_mute 'index' out of range");
                    result.ok = false;
                    actErr = true;
                } else {
                    a.cmd = KeyCmd::DrumMute;
                    a.slot = static_cast<uint8_t>(idx);
                }
            }
        } else if (type == "preset_prev")      { a.cmd = KeyCmd::PresetPrev; }
        else if (type == "preset_next")        { a.cmd = KeyCmd::PresetNext; }
        else if (type == "preset_bank_prev")   { a.cmd = KeyCmd::PresetBankPrev; }
        else if (type == "preset_bank_next")   { a.cmd = KeyCmd::PresetBankNext; }
        else if (type == "preset_load") {
            int slot = 0;
            needInt("slot", slot);
            if (!actErr && slot > 7) {
                result.errors.push_back(where + ": preset_load 'slot' out of range");
                result.ok = false;
                actErr = true;
            }
            a.slot = static_cast<uint8_t>(slot);
            a.cmd = KeyCmd::PresetLoad;
        }
        else if (type == "preset_save")        { a.cmd = KeyCmd::PresetSave; }
        else if (type == "preset_clear")       { a.cmd = KeyCmd::PresetClear; }
        else if (type == "tap_tempo")          { a.cmd = KeyCmd::TapTempo; }
        else {
            result.errors.push_back(where + ": unknown action type '" + type + "'");
            result.ok = false;
            actErr = true;
        }

        if (actErr) continue;

        KeyBinding kb;
        kb.modifierMask = mods;
        kb.usage = usage;
        kb.action = a;
        cfg.bindings.push_back(kb);
    }

    if (result.ok) {
        // Required coverage (FR: every menu reachable + preset nav/save; preset
        // load is hardcoded on Super+1..8 so it is always available).
        bool chord  = false, strum = false, rhythm = false, bass = false, drum = false;
        for (const auto& b : cfg.bindings) {
            if (b.action.cmd == KeyCmd::OpenMenu) {
                switch (b.action.menu) {
                    case EditMenu::Chord:  chord  = true; break;
                    case EditMenu::Strum:  strum  = true; break;
                    case EditMenu::Rhythm: rhythm = true; break;
                    case EditMenu::Bass:   bass   = true; break;
                    case EditMenu::Drum:   drum   = true; break;
                    default: break;
                }
            }
        }
        addCoverageError(result.errors, chord,  "menu chord");
        addCoverageError(result.errors, strum,  "menu strum");
        addCoverageError(result.errors, rhythm, "menu rhythm");
        addCoverageError(result.errors, bass,   "menu bass");
        addCoverageError(result.errors, drum,   "menu drum");
        addCoverageError(result.errors, bindingHasCmd(cfg.bindings, KeyCmd::PresetPrev),  "preset_prev");
        addCoverageError(result.errors, bindingHasCmd(cfg.bindings, KeyCmd::PresetNext),  "preset_next");
        addCoverageError(result.errors, bindingHasCmd(cfg.bindings, KeyCmd::PresetSave),  "preset_save");
        if (!result.errors.empty()) result.ok = false;
    }

    if (result.ok) {
        result.config = std::move(cfg);
    } else {
        result.config = KeymapConfig::defaults();
    }

    return result;
}

std::string keymapDefaultJson() {
    return std::string(
R"({
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
    { "keys": ["PrtSc"], "action": { "type": "set", "param": "chord_inversion", "value": "first" } },
    { "keys": ["ScLk"], "action": { "type": "set", "param": "chord_inversion", "value": "second" } },
    { "keys": ["Pause"], "action": { "type": "set", "param": "chord_inversion", "value": "third" } },
    { "keys": ["="], "action": { "type": "inc", "param": "chord_octave" } },
    { "keys": ["-"], "action": { "type": "dec", "param": "chord_octave" } },
    { "keys": ["KpPlus"], "action": { "type": "inc", "param": "strum_octave" } },
    { "keys": ["KpMinus"], "action": { "type": "dec", "param": "strum_octave" } },
    { "keys": ["Left"], "action": { "type": "ext", "value": 9 } },
    { "keys": ["Down"], "action": { "type": "ext", "value": 11 } },
    { "keys": ["Right"], "action": { "type": "ext", "value": 13 } },
    { "keys": ["PgUp"], "action": { "type": "inc", "param": "tempo" } },
    { "keys": ["PgDn"], "action": { "type": "dec", "param": "tempo" } },
    { "keys": ["Space"], "action": { "type": "tap_tempo" } },
    { "keys": ["Home"], "action": { "type": "preset_prev" } },
    { "keys": ["End"], "action": { "type": "preset_next" } },
    { "keys": ["Insert"], "action": { "type": "preset_save" } },
    { "keys": ["Delete"], "action": { "type": "preset_clear" } },
    { "keys": ["Super", "Home"], "action": { "type": "preset_bank_prev" } },
    { "keys": ["Super", "End"], "action": { "type": "preset_bank_next" } },
    { "keys": ["Super", "F11"], "action": { "type": "open_menu", "menu": "drum" } },
    { "keys": ["Ctrl", "F1"], "action": { "type": "cycle", "param": "chord_voicing" } },
    { "keys": ["Ctrl", "F2"], "action": { "type": "cycle", "param": "chord_inversion" } },
    { "keys": ["Ctrl", "F3"], "action": { "type": "cycle", "param": "arp_mode" } },
    { "keys": ["Ctrl", "F4"], "action": { "type": "toggle", "param": "chord_roll", "a": 0, "b": 50 } },
    { "keys": ["Ctrl", "F5"], "action": { "type": "toggle", "param": "rhythm_led", "a": 1, "b": 0 } },
    { "keys": ["Ctrl", "="], "action": { "type": "inc", "param": "chord_velocity" } },
    { "keys": ["Ctrl", "-"], "action": { "type": "dec", "param": "chord_velocity" } },
    { "keys": ["Ctrl", "KpEnter"], "action": { "type": "cycle", "param": "strum_layout" } },
    { "keys": ["Ctrl", "KpPlus"], "action": { "type": "inc", "param": "strum_scale" } },
    { "keys": ["Ctrl", "KpMinus"], "action": { "type": "dec", "param": "strum_scale" } },
    { "keys": ["Alt", "KpEnter"], "action": { "type": "cycle", "param": "strum_mode" } },
    { "keys": ["Alt", "KpPlus"], "action": { "type": "inc", "param": "strum_root" } },
    { "keys": ["Alt", "KpMinus"], "action": { "type": "dec", "param": "strum_root" } },
    { "keys": ["Alt", "="], "action": { "type": "inc", "param": "bass_octave" } },
    { "keys": ["Alt", "-"], "action": { "type": "dec", "param": "bass_octave" } },
    { "keys": ["Alt", "F1"], "action": { "type": "drum_mute", "index": 0 } },
    { "keys": ["Alt", "F2"], "action": { "type": "drum_mute", "index": 1 } },
    { "keys": ["Alt", "F3"], "action": { "type": "drum_mute", "index": 2 } },
    { "keys": ["Alt", "F4"], "action": { "type": "drum_mute", "index": 3 } },
    { "keys": ["Alt", "F5"], "action": { "type": "drum_mute", "index": 4 } },
    { "keys": ["Alt", "F6"], "action": { "type": "drum_mute", "index": 5 } },
    { "keys": ["Alt", "F7"], "action": { "type": "drum_mute", "index": 6 } },
    { "keys": ["Alt", "F8"], "action": { "type": "drum_mute", "index": 7 } },
    { "keys": ["Alt", "F9"], "action": { "type": "drum_mute", "index": 8 } },
    { "keys": ["Alt", "F10"], "action": { "type": "drum_mute", "index": 9 } },
    { "keys": ["Alt", "F11"], "action": { "type": "drum_mute", "index": 10 } },
    { "keys": ["Alt", "F12"], "action": { "type": "drum_mute", "index": 11 } }
  ]
}
)");
}
