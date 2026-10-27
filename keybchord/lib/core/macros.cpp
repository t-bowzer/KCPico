#include "macros.h"

#include "keymap_config.h"


namespace {

constexpr const char* kMacroKeyLabels[MACRO_KEY_COUNT] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
};

struct MacroParamName {
    const char* name;
    ParamId     id;
};

const MacroParamName kMacroExtraParams[] = {
    {"drum_kick",        ParamId::DrumKickNote},
    {"drum_kick_vel",    ParamId::DrumKickVel},
    {"drum_snare",       ParamId::DrumSnareNote},
    {"drum_snare_vel",   ParamId::DrumSnareVel},
    {"drum_hihat",       ParamId::DrumHihatNote},
    {"drum_hihat_vel",   ParamId::DrumHihatVel},
    {"drum_open_hat",    ParamId::DrumOpenHatNote},
    {"drum_open_hat_vel", ParamId::DrumOpenHatVel},
    {"drum_rimshot",     ParamId::DrumRimshotNote},
    {"drum_rimshot_vel", ParamId::DrumRimshotVel},
    {"drum_clap",        ParamId::DrumClapNote},
    {"drum_clap_vel",    ParamId::DrumClapVel},
    {"drum_crash",       ParamId::DrumCrashNote},
    {"drum_crash_vel",   ParamId::DrumCrashVel},
    {"drum_ride",        ParamId::DrumRideNote},
    {"drum_ride_vel",    ParamId::DrumRideVel},
    {"drum_bongo",       ParamId::DrumBongoNote},
    {"drum_bongo_vel",   ParamId::DrumBongoVel},
    {"drum_conga_lo",    ParamId::DrumCongaLoNote},
    {"drum_conga_lo_vel", ParamId::DrumCongaLoVel},
    {"drum_conga_hi",    ParamId::DrumCongaHiNote},
    {"drum_conga_hi_vel", ParamId::DrumCongaHiVel},
    {"drum_clave",       ParamId::DrumClaveNote},
    {"drum_clave_vel",   ParamId::DrumClaveVel},
    {"drum_shaker",      ParamId::DrumShakerNote},
    {"drum_shaker_vel",  ParamId::DrumShakerVel},
};

const char* const kDrumPieceKeys[] = {
    "kick", "snare", "hihat", "open_hat", "rimshot", "clap", "crash",
    "ride", "bongo", "conga_lo", "conga_hi", "clave", "shaker",
};

const ParamId kDrumPieceVelocityParams[] = {
    ParamId::DrumKickVel,    ParamId::DrumSnareVel,
    ParamId::DrumHihatVel,   ParamId::DrumOpenHatVel,
    ParamId::DrumRimshotVel, ParamId::DrumClapVel,
    ParamId::DrumCrashVel,   ParamId::DrumRideVel,
    ParamId::DrumBongoVel,   ParamId::DrumCongaLoVel,
    ParamId::DrumCongaHiVel, ParamId::DrumClaveVel,
    ParamId::DrumShakerVel,
};

constexpr int DRUM_PIECE_COUNT =
    static_cast<int>(sizeof(kDrumPieceKeys) / sizeof(kDrumPieceKeys[0]));

bool isDrumVelocityParam(ParamId id) {
    for (ParamId v : kDrumPieceVelocityParams) {
        if (v == id) return true;
    }
    return false;
}

} // namespace


MacroConfig MacroConfig::defaults() {
    return MacroConfig{};
}

bool MacroConfig::operator==(const MacroConfig& other) const {
    if (numberKeys != other.numberKeys) return false;
    for (int i = 0; i < MACRO_KEY_COUNT; i++) {
        if (names[i] != other.names[i]) return false;
        if (keys[i] != other.keys[i]) return false;
    }
    return true;
}

std::string MacroConfig::nameFor(uint8_t index) const {
    if (index >= MACRO_KEY_COUNT) return "";
    if (!names[index].empty()) return names[index];
    const char* label = macroKeyLabel(index);
    return label ? (std::string("M") + label) : "";
}

const std::vector<MacroCommand>& MacroConfig::commandsFor(uint8_t index) const {
    static const std::vector<MacroCommand> empty;
    if (index >= MACRO_KEY_COUNT) return empty;
    return keys[index];
}

std::vector<MacroCommand>& MacroConfig::commandsFor(uint8_t index) {
    static std::vector<MacroCommand> empty;
    if (index >= MACRO_KEY_COUNT) return empty;
    return keys[index];
}

const char* macroKeyLabel(int index) {
    if (index < 0 || index >= MACRO_KEY_COUNT) return nullptr;
    return kMacroKeyLabels[index];
}

int macroKeyIndex(const std::string& label) {
    for (int i = 0; i < MACRO_KEY_COUNT; i++) {
        if (label == kMacroKeyLabels[i]) return i;
    }
    return -1;
}

const char* macroDrumPieceKey(int index) {
    if (index < 0 || index >= DRUM_PIECE_COUNT) return nullptr;
    return kDrumPieceKeys[index];
}

int macroDrumPieceIndex(const std::string& name) {
    for (int i = 0; i < DRUM_PIECE_COUNT; i++) {
        if (name == kDrumPieceKeys[i]) return i;
    }
    return -1;
}

ParamId macroDrumPieceVelocityParam(int index) {
    if (index < 0 || index >= DRUM_PIECE_COUNT) return ParamId::COUNT;
    return kDrumPieceVelocityParams[index];
}

bool macroLookupParamName(const std::string& name, ParamId& id) {
    if (keymapLookupParamName(name, id)) return true;
    for (const auto& p : kMacroExtraParams) {
        if (name == p.name) { id = p.id; return true; }
    }
    return false;
}

const char* macroParamName(ParamId id) {
    const char* name = keymapParamName(id);
    if (name) return name;
    for (const auto& p : kMacroExtraParams) {
        if (id == p.id) return p.name;
    }
    return nullptr;
}

bool macroParseParamValue(ParamId id, const std::string& value, int& out) {
    if (isDrumVelocityParam(id)) {
        if (value == "off")  { out = param_bounds::DRUM_VELOCITY_OFF; return true; }
        if (value == "auto") { out = 0; return true; }
    }
    return keymapParseParamValue(id, value, out);
}
