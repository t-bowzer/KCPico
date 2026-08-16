#pragma once

#include <cstdint>


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

enum class ActionType : uint8_t {
    None = 0,
    ChordKey,       // one of the 36 chord-grid keys (GridCell in action)
    Backtick,       // ` key (leftmost-column modifier source)
    PlayModeCycle,  // F1
    VoicingToggle,  // F5
    ExtToggle9,     // Left arrow
    ExtToggle11,    // Down arrow
    ExtToggle13,    // Right arrow
    ChordOctaveUp,   // = (number row)
    ChordOctaveDown, // - (number row)
    StrumOctaveUp,   // Keypad +
    StrumOctaveDown, // Keypad -
    StrumKey,       // number-row / keypad strum key (layout applied in engine)
    ClearEdit,      // Esc (exits the edit menu)
    RhythmToggle,   // F7
    RhythmPatternCycle, // F8
    RhythmMute,     // F9
    RhythmClockToggle, // F6 (main-menu clock-out toggle, FR-R7)
    RhythmLedToggle,   // F10 (main-menu beat-LED on/off, FR-R8)
    TempoUp,        // Page Up
    TempoDown,      // Page Down
    MenuChord,      // Menu key (0x65) -> Chord Edit menu
    MenuStrum,      // Alt -> Strum Edit menu
    MenuRhythm,     // Ctrl -> Rhythm Edit menu
    PresetPrev,     // Home -> cursor previous preset (FR-P3)
    PresetNext,     // End -> cursor next preset (FR-P3)
    PresetBankPrev, // Super+Home -> cursor previous bank (FR-P4)
    PresetBankNext, // Super+End -> cursor next bank (FR-P4)
    PresetLoad,     // Super+1..8 -> load slot (action.index = 0..7) (FR-P5)
    PresetSave,     // Super+Insert -> save prompt (FR-P6)
    PresetClear,    // Super+Delete -> clear prompt (FR-P7)
    COUNT
};

struct KeyAction {
    ActionType type = ActionType::None;
    GridCell   cell;  // valid when type == ChordKey
    uint8_t    index = 0;  // valid when type == PresetLoad (0-based slot)
};


class KeymapResolver {
public:
    // Maps a raw HID usage (plus modifier byte) to a semantic action.
    // Shift is a chord root and is NOT treated as a function modifier;
    // only Ctrl/Alt/Super act as function modifiers. Alt is the strum modifier
    // (Alt+F2..F5), Ctrl the chord/preset modifier, Super the global modifier.
    KeyAction resolve(uint8_t hid_usage, uint8_t modifiers) const;

    // True if the usage is one of the 36 chord-grid keys (incl. Tab/Caps/Shift/[ /').
    static bool isChordKey(uint8_t hid_usage);

    // True if Super/Gui (either) is present in the HID modifier byte. Super is
    // the global modifier (presets/panic); Ctrl/Alt are menu toggles, not
    // function modifiers.
    static bool isSuper(uint8_t modifiers);
};

