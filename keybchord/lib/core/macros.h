#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "params.h"


// Per-preset top-row number-key behaviour. In Macro mode the top-row keys
// 1..9,0 trigger user-defined macros; the numpad remains the strum plate.
enum class NumberKeyMode : uint8_t {
    Strum = 0,
    Macro,
};

enum class MacroOp : uint8_t {
    Set = 0,
    Toggle,
    Cycle,
    Inc,
    Dec,
    DrumMute,
    COUNT,
};

// A single macro command. `valueA`/`valueB` hold the keymap-action values;
// DrumMute stores the drum-piece index in `drumPiece` and on/off in `valueA`.
struct MacroCommand {
    MacroOp op    = MacroOp::Set;
    ParamId param = ParamId::COUNT;
    int16_t valueA = 0;
    int16_t valueB = 0;
    uint8_t drumPiece = 0;

    bool operator==(const MacroCommand& other) const {
        return op == other.op && param == other.param &&
               valueA == other.valueA && valueB == other.valueB &&
               drumPiece == other.drumPiece;
    }
    bool operator!=(const MacroCommand& other) const { return !(*this == other); }
};

constexpr int MACRO_KEY_COUNT = 10;
constexpr int MAX_MACRO_COMMANDS_PER_KEY = 16;
constexpr int MAX_MACRO_COMMANDS_PER_PRESET = 48;

struct MacroConfig {
    NumberKeyMode numberKeys = NumberKeyMode::Strum;
    std::vector<MacroCommand> keys[MACRO_KEY_COUNT];

    static MacroConfig defaults();
    bool operator==(const MacroConfig& other) const;
    bool operator!=(const MacroConfig& other) const { return !(*this == other); }

    const std::vector<MacroCommand>& commandsFor(uint8_t index) const;
    std::vector<MacroCommand>& commandsFor(uint8_t index);
};

// Top-row label ("1".."9","0") for index 0..9, or nullptr when out of range.
const char* macroKeyLabel(int index);
int macroKeyIndex(const std::string& label);

// Drum-piece key names shared with the preset JSON (kick, snare, ...).
const char* macroDrumPieceKey(int index);
int macroDrumPieceIndex(const std::string& name);
ParamId macroDrumPieceVelocityParam(int index);

// Parameter lookup/value parsing for macro commands (keymap params plus the
// per-drum-piece note/velocity parameters).
bool macroLookupParamName(const std::string& name, ParamId& id);
const char* macroParamName(ParamId id);
bool macroParseParamValue(ParamId id, const std::string& value, int& out);
