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
    VoicingToggle,  // F2
    BassToggle,     // F3
    RhythmLedToggle,   // F4
    RhythmToggle,      // F5
    RhythmClockToggle, // F6
    RhythmPatternCycle,// F7
    RhythmMute,        // F8
    MenuChord,      // F9  -> Chord Edit menu
    MenuStrum,      // F10 -> Strum Edit menu
    MenuRhythm,     // F11 -> Rhythm Edit menu
    MenuBass,       // F12 -> Bass Edit menu
    Ext9,           // Left arrow  (held add9 modifier, FR-C7)
    Ext11,          // Down arrow  (held add11 modifier, FR-C7)
    Ext13,          // Right arrow (held add13 modifier, FR-C7)
    Inversion1,     // PrtSc (first inversion, VR-8)
    Inversion2,     // ScLk  (second inversion, VR-8)
    Inversion3,     // Pause (third inversion, VR-8)
    ChordOctaveUp,   // = (number row)
    ChordOctaveDown, // - (number row)
    StrumOctaveUp,   // Keypad +
    StrumOctaveDown, // Keypad -
    StrumKey,       // number-row / keypad strum key (layout applied in engine)
    ClearEdit,      // Esc (exits the edit menu / cancels)
    TempoUp,        // Page Up
    TempoDown,      // Page Down
    PresetPrev,     // Home -> cursor previous preset (FR-P3)
    PresetNext,     // End -> cursor next preset (FR-P3)
    PresetBankPrev, // Super+Home -> cursor previous bank (FR-P4)
    PresetBankNext, // Super+End -> cursor next bank (FR-P4)
    PresetLoad,     // Super+1..8 -> load slot (action.index = 0..7) (FR-P5)
    PresetSave,     // Insert -> save prompt (FR-P6)
    PresetClear,    // Delete -> clear prompt (FR-P7)
    Panic,          // Super+Esc -> all-sound/notes-off all channels (FR-C11)
    TapTempo,       // Space -> tap tempo (sets rhythm tempo)
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
    // Shift is a chord root and is NOT treated as a function modifier; Super
    // is the global preset/panic modifier. Ctrl/Alt/Menu have no runtime
    // function (Ctrl is the power-on boot key only).
    KeyAction resolve(uint8_t hid_usage, uint8_t modifiers) const;

    // True if the usage is one of the 36 chord-grid keys (incl. Tab/Caps/Shift/[ /').
    static bool isChordKey(uint8_t hid_usage);

    // True if Super/Gui (either) is present in the HID modifier byte. Super is
    // the global modifier (presets/panic).
    static bool isSuper(uint8_t modifiers);
};
