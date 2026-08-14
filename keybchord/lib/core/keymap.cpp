#include "keymap.h"


namespace {

struct ChordKeyEntry {
    uint8_t      usage;
    ChordQuality quality;
    int          column;
};

// HID usages for the authentic Omnichord 3x12 grid (spec section 5.1).
// Row = quality; column = root (circle of fifths, left->right):
//   Db Ab Eb Bb F C G D A E B F#
constexpr ChordKeyEntry kMajorKeys[12] = {
    {0x2B, ChordQuality::Major, 0},   // Tab
    {0x14, ChordQuality::Major, 1},   // Q
    {0x1A, ChordQuality::Major, 2},   // W
    {0x08, ChordQuality::Major, 3},   // E
    {0x15, ChordQuality::Major, 4},   // R
    {0x17, ChordQuality::Major, 5},   // T
    {0x1C, ChordQuality::Major, 6},   // Y
    {0x18, ChordQuality::Major, 7},   // U
    {0x0C, ChordQuality::Major, 8},   // I
    {0x12, ChordQuality::Major, 9},   // O
    {0x13, ChordQuality::Major, 10},  // P
    {0x2F, ChordQuality::Major, 11},  // [
};

constexpr ChordKeyEntry kMinorKeys[12] = {
    {0x39, ChordQuality::Minor, 0},   // Caps Lock
    {0x04, ChordQuality::Minor, 1},   // A
    {0x16, ChordQuality::Minor, 2},   // S
    {0x07, ChordQuality::Minor, 3},   // D
    {0x09, ChordQuality::Minor, 4},   // F
    {0x0A, ChordQuality::Minor, 5},   // G
    {0x0B, ChordQuality::Minor, 6},   // H
    {0x0D, ChordQuality::Minor, 7},   // J
    {0x0E, ChordQuality::Minor, 8},   // K
    {0x0F, ChordQuality::Minor, 9},   // L
    {0x33, ChordQuality::Minor, 10},  // ;
    {0x34, ChordQuality::Minor, 11},  // '
};

constexpr ChordKeyEntry kSeventhKeys[12] = {
    {0xE2, ChordQuality::Seventh, 0},   // Left Shift
    {0x1D, ChordQuality::Seventh, 1},   // Z
    {0x1B, ChordQuality::Seventh, 2},   // X
    {0x06, ChordQuality::Seventh, 3},   // C
    {0x19, ChordQuality::Seventh, 4},   // V
    {0x05, ChordQuality::Seventh, 5},   // B
    {0x11, ChordQuality::Seventh, 6},   // N
    {0x10, ChordQuality::Seventh, 7},   // M
    {0x36, ChordQuality::Seventh, 8},   // ,
    {0x37, ChordQuality::Seventh, 9},   // .
    {0x38, ChordQuality::Seventh, 10},  // /
    {0xE6, ChordQuality::Seventh, 11},  // Right Shift
};

constexpr uint8_t HID_USAGE_BACKTICK   = 0x35;
constexpr uint8_t HID_USAGE_F1         = 0x3A;
constexpr uint8_t HID_USAGE_F5         = 0x3E;
constexpr uint8_t HID_USAGE_LEFT       = 0x50;
constexpr uint8_t HID_USAGE_DOWN       = 0x51;
constexpr uint8_t HID_USAGE_RIGHT      = 0x4F;
constexpr uint8_t HID_USAGE_MINUS      = 0x2D;  // number-row -
constexpr uint8_t HID_USAGE_EQUALS     = 0x2E;  // number-row =
constexpr uint8_t HID_USAGE_KP_MINUS   = 0x56;
constexpr uint8_t HID_USAGE_KP_PLUS    = 0x57;

bool lookupChordKey(uint8_t usage, GridCell& out) {
    for (const auto& e : kMajorKeys) {
        if (e.usage == usage) { out = {e.quality, e.column}; return true; }
    }
    for (const auto& e : kMinorKeys) {
        if (e.usage == usage) { out = {e.quality, e.column}; return true; }
    }
    for (const auto& e : kSeventhKeys) {
        if (e.usage == usage) { out = {e.quality, e.column}; return true; }
    }
    return false;
}

} // namespace


bool KeymapResolver::isChordKey(uint8_t hid_usage) {
    GridCell cell;
    return lookupChordKey(hid_usage, cell);
}

bool KeymapResolver::isFunctionModifier(uint8_t modifiers) {
    // Ctrl (bits 0/4), Alt (bits 2/6), Super/Gui (bits 3/7).
    // Shift (bits 1/5) is a chord root, never a function modifier.
    constexpr uint8_t CTRL  = 0x11;  // LCtrl | RCtrl
    constexpr uint8_t ALT   = 0x44;  // LAlt  | RAlt
    constexpr uint8_t SUPER = 0x88;  // LGui  | RGui
    return (modifiers & (CTRL | ALT | SUPER)) != 0;
}

KeyAction KeymapResolver::resolve(uint8_t hid_usage, uint8_t modifiers) const {
    (void)modifiers;  // modifier byte reserved for Ctrl/Alt/Super combos (later milestones)

    KeyAction a;

    GridCell cell;
    if (lookupChordKey(hid_usage, cell)) {
        a.type = ActionType::ChordKey;
        a.cell = cell;
        return a;
    }

    switch (hid_usage) {
        case HID_USAGE_BACKTICK: a.type = ActionType::Backtick;  break;
        case HID_USAGE_F1:       a.type = ActionType::PlayModeCycle; break;
        case HID_USAGE_F5:       a.type = ActionType::VoicingToggle; break;
        case HID_USAGE_LEFT:     a.type = ActionType::ExtToggle9;    break;
        case HID_USAGE_DOWN:     a.type = ActionType::ExtToggle11;   break;
        case HID_USAGE_RIGHT:    a.type = ActionType::ExtToggle13;   break;
        case HID_USAGE_EQUALS:
        case HID_USAGE_KP_PLUS:  a.type = ActionType::OctaveUp;      break;
        case HID_USAGE_MINUS:
        case HID_USAGE_KP_MINUS: a.type = ActionType::OctaveDown;    break;
        default:                 a.type = ActionType::None;          break;
    }

    return a;
}

