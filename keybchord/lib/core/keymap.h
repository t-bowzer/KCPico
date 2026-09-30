#pragma once

#include <cstdint>

#include "params.h"


enum class ChordQuality : uint8_t {
    Major = 0,
    Minor,
    Seventh,
    COUNT
};

struct GridCell {
    ChordQuality quality = ChordQuality::Major;
    int          column  = 0;  // 0..11 (circle-of-fifths order)
};

// Combo-modifier bits (normalized from the HID modifier byte). Shift is a chord
// key and is NOT a combo modifier; only Super (either Gui) and the *left*
// Ctrl/Alt participate in combos (per the keymap spec).
constexpr uint8_t KMOD_CTRL  = 0x01;  // LCtrl
constexpr uint8_t KMOD_ALT   = 0x02;  // LAlt
constexpr uint8_t KMOD_SUPER = 0x04;  // LGui | RGui

// Extracts the combo-modifier bits (KMOD_*) from a raw HID modifier byte.
uint8_t keymapComboMask(uint8_t hid_modifiers);

// True if the HID usage is a reserved key that may not be rebound in the
// keymap config (chord-grid, strum, backtick, Esc, Enter/Backspace).
bool keymapIsReserved(uint8_t hid_usage);

// Character produced by a keyboard key for preset name text-entry, or '\0' if
// the usage is not a printable text-entry key. `modifiers` is the raw HID
// modifier byte; holding Shift (left 0x02 / right 0x20) yields the shifted
// character (uppercase letters, number-row symbols). Tab and Space map to ' '.
char keymapCharForUsage(uint8_t hid_usage, uint8_t modifiers = 0);

// A resolved key command. "Reserved" commands (ChordKey/Backtick/StrumKey/
// ClearEdit/Panic) are hardcoded; everything else is data-driven from the
// loaded keymap config (including the held extensions Ext9/Ext11/Ext13, which
// map to the "ext" keymap action).
enum class KeyCmd : uint8_t {
    None = 0,
    // Reserved (hardcoded, non-configurable):
    ChordKey,       // one of the 36 chord-grid keys (GridCell in action)
    Backtick,       // ` key (leftmost-column modifier source)
    StrumKey,       // number-row / keypad strum key (layout applied in engine)
    ClearEdit,      // Esc (exits the edit menu / cancels)
    Panic,          // Super+Esc -> all-sound/notes-off all channels (FR-C11)
    // Configurable:
    Ext9,           // held add9 modifier (default: Left arrow, FR-C7)
    Ext11,          // held add11 modifier (default: Down arrow, FR-C7)
    Ext13,          // held add13 modifier (default: Right arrow, FR-C7)
    OpenMenu,       // menu = EditMenu
    CycleParam,     // param
    SetParam,       // param + valueA
    IncParam,       // param
    DecParam,       // param
    ToggleParam,    // param + valueA + valueB
    DrumMute,       // slot = drum index (0-based track position)
    PresetPrev,     // cursor previous preset (FR-P3)
    PresetNext,     // cursor next preset (FR-P3)
    PresetBankPrev, // cursor previous bank (FR-P4)
    PresetBankNext, // cursor next bank (FR-P4)
    PresetLoad,     // slot = 0..7 -> load slot (FR-P5)
    PresetSave,     // save prompt (FR-P6)
    PresetClear,    // clear prompt (FR-P7)
    TapTempo,       // tap tempo (sets rhythm tempo)
    COUNT
};

struct KeyAction {
    KeyCmd  cmd    = KeyCmd::None;
    ParamId param  = ParamId::COUNT;
    int     valueA = 0;
    int     valueB = 0;
    EditMenu menu  = EditMenu::None;  // valid when cmd == OpenMenu
    GridCell cell;                     // valid when cmd == ChordKey
    uint8_t  slot  = 0;                // valid when cmd == PresetLoad / DrumMute
};

class KeymapConfig;


class KeymapResolver {
public:
    // `keymap` may be null, in which case resolve() returns None for every
    // non-reserved key (no configurable bindings).
    explicit KeymapResolver(const KeymapConfig* keymap = nullptr);

    // Maps a raw HID usage (plus modifier byte) to a semantic action.
    KeyAction resolve(uint8_t hid_usage, uint8_t modifiers) const;

    // True if the usage is one of the 36 chord-grid keys (incl. Tab/Caps/Shift/[ /').
    static bool isChordKey(uint8_t hid_usage);

    // True if Super/Gui (either) is present in the HID modifier byte.
    static bool isSuper(uint8_t modifiers);

private:
    const KeymapConfig* keymap_;
};
