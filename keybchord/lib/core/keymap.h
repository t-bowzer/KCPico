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
    OctaveUp,       // = / Keypad+
    OctaveDown,     // - / Keypad-
    COUNT
};

struct KeyAction {
    ActionType type = ActionType::None;
    GridCell   cell;  // valid when type == ChordKey
};


class KeymapResolver {
public:
    // Maps a raw HID usage (plus modifier byte) to a semantic action.
    // Shift is a chord root and is NOT treated as a function modifier;
    // only Ctrl/Alt/Super act as function modifiers (deferred to later milestones).
    KeyAction resolve(uint8_t hid_usage, uint8_t modifiers) const;

    // True if the usage is one of the 36 chord-grid keys (incl. Tab/Caps/Shift/[ /').
    static bool isChordKey(uint8_t hid_usage);

    // True if Ctrl, Alt, or Super is present in the HID modifier byte.
    static bool isFunctionModifier(uint8_t modifiers);
};

