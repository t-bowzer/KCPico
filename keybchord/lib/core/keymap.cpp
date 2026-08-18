#include "keymap.h"
#include "presets.h"


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
constexpr uint8_t HID_USAGE_F2         = 0x3B;
constexpr uint8_t HID_USAGE_F3         = 0x3C;
constexpr uint8_t HID_USAGE_F4         = 0x3D;
constexpr uint8_t HID_USAGE_F5         = 0x3E;
constexpr uint8_t HID_USAGE_F6         = 0x3F;
constexpr uint8_t HID_USAGE_F7         = 0x40;
constexpr uint8_t HID_USAGE_F8         = 0x41;
constexpr uint8_t HID_USAGE_F9         = 0x42;
constexpr uint8_t HID_USAGE_F10        = 0x43;
constexpr uint8_t HID_USAGE_F11        = 0x44;
constexpr uint8_t HID_USAGE_F12        = 0x45;
constexpr uint8_t HID_USAGE_PRTSC      = 0x46;  // Print Screen
constexpr uint8_t HID_USAGE_SCLK       = 0x47;  // Scroll Lock
constexpr uint8_t HID_USAGE_PAUSE      = 0x48;  // Pause
constexpr uint8_t HID_USAGE_ESC        = 0x29;
constexpr uint8_t HID_USAGE_LEFT       = 0x50;
constexpr uint8_t HID_USAGE_DOWN       = 0x51;
constexpr uint8_t HID_USAGE_RIGHT      = 0x4F;
constexpr uint8_t HID_USAGE_PAGE_UP    = 0x4B;
constexpr uint8_t HID_USAGE_PAGE_DOWN  = 0x4E;
constexpr uint8_t HID_USAGE_MINUS      = 0x2D;  // number-row -
constexpr uint8_t HID_USAGE_EQUALS     = 0x2E;  // number-row =
constexpr uint8_t HID_USAGE_KP_SLASH   = 0x54;
constexpr uint8_t HID_USAGE_KP_STAR    = 0x55;
constexpr uint8_t HID_USAGE_KP_MINUS   = 0x56;
constexpr uint8_t HID_USAGE_KP_PLUS    = 0x57;
constexpr uint8_t HID_USAGE_HOME       = 0x4A;
constexpr uint8_t HID_USAGE_END        = 0x4D;
constexpr uint8_t HID_USAGE_INSERT     = 0x49;
constexpr uint8_t HID_USAGE_DELETE     = 0x4C;

constexpr uint8_t NUMBER_ROW_STRUM_LO  = 0x1E;  // 1
constexpr uint8_t NUMBER_ROW_STRUM_HI  = 0x27;  // 0
constexpr uint8_t KEYPAD_STRUM_LO      = 0x59;  // Keypad 1
constexpr uint8_t KEYPAD_STRUM_HI      = 0x63;  // Keypad .

bool isNumberRowStrum(uint8_t usage) {
    return usage >= NUMBER_ROW_STRUM_LO && usage <= NUMBER_ROW_STRUM_HI;
}

bool isKeypadStrum(uint8_t usage) {
    return (usage >= KEYPAD_STRUM_LO && usage <= KEYPAD_STRUM_HI)
        || usage == HID_USAGE_KP_SLASH
        || usage == HID_USAGE_KP_STAR;
}

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

bool KeymapResolver::isSuper(uint8_t modifiers) {
    constexpr uint8_t SUPER = 0x88;  // LGui | RGui
    return (modifiers & SUPER) != 0;
}

KeyAction KeymapResolver::resolve(uint8_t hid_usage, uint8_t modifiers) const {
    KeyAction a;

    // Super is the global preset/panic modifier (spec 5.7/5.8). While Super is
    // held only preset bindings resolve; everything else is inert.
    if (isSuper(modifiers)) {
        switch (hid_usage) {
            case HID_USAGE_HOME:   a.type = ActionType::PresetBankPrev; return a;
            case HID_USAGE_END:    a.type = ActionType::PresetBankNext; return a;
            case HID_USAGE_ESC:    a.type = ActionType::Panic;          return a;
            default:
                if (isNumberRowStrum(hid_usage)) {
                    int idx = static_cast<int>(hid_usage - NUMBER_ROW_STRUM_LO); // 0..9
                    if (idx < NUM_SLOTS) {
                        a.type  = ActionType::PresetLoad;
                        a.index = static_cast<uint8_t>(idx);
                        return a;
                    }
                }
                return a;  // None
        }
    }

    GridCell cell;
    if (lookupChordKey(hid_usage, cell)) {
        a.type = ActionType::ChordKey;
        a.cell = cell;
        return a;
    }

    switch (hid_usage) {
        case HID_USAGE_BACKTICK: a.type = ActionType::Backtick;         break;
        case HID_USAGE_F1:       a.type = ActionType::PlayModeCycle;    break;
        case HID_USAGE_F2:       a.type = ActionType::VoicingToggle;    break;
        case HID_USAGE_F3:       a.type = ActionType::BassToggle;       break;
        case HID_USAGE_F4:       a.type = ActionType::RhythmLedToggle;  break;
        case HID_USAGE_F5:       a.type = ActionType::RhythmToggle;     break;
        case HID_USAGE_F6:       a.type = ActionType::RhythmClockToggle; break;
        case HID_USAGE_F7:       a.type = ActionType::RhythmPatternCycle; break;
        case HID_USAGE_F8:       a.type = ActionType::RhythmMute;       break;
        case HID_USAGE_F9:       a.type = ActionType::MenuChord;        break;
        case HID_USAGE_F10:      a.type = ActionType::MenuStrum;        break;
        case HID_USAGE_F11:      a.type = ActionType::MenuRhythm;       break;
        case HID_USAGE_F12:      a.type = ActionType::MenuBass;         break;
        case HID_USAGE_LEFT:     a.type = ActionType::Ext9;             break;
        case HID_USAGE_DOWN:     a.type = ActionType::Ext11;            break;
        case HID_USAGE_RIGHT:    a.type = ActionType::Ext13;            break;
        case HID_USAGE_PRTSC:    a.type = ActionType::Inversion1;       break;
        case HID_USAGE_SCLK:     a.type = ActionType::Inversion2;       break;
        case HID_USAGE_PAUSE:    a.type = ActionType::Inversion3;       break;
        case HID_USAGE_ESC:      a.type = ActionType::ClearEdit;        break;
        case HID_USAGE_PAGE_UP:   a.type = ActionType::TempoUp;         break;
        case HID_USAGE_PAGE_DOWN: a.type = ActionType::TempoDown;       break;
        case HID_USAGE_EQUALS:   a.type = ActionType::ChordOctaveUp;    break;
        case HID_USAGE_MINUS:    a.type = ActionType::ChordOctaveDown;  break;
        case HID_USAGE_KP_PLUS:  a.type = ActionType::StrumOctaveUp;    break;
        case HID_USAGE_KP_MINUS: a.type = ActionType::StrumOctaveDown;  break;
        case HID_USAGE_HOME:     a.type = ActionType::PresetPrev;       break;
        case HID_USAGE_END:      a.type = ActionType::PresetNext;       break;
        case HID_USAGE_INSERT:   a.type = ActionType::PresetSave;       break;
        case HID_USAGE_DELETE:   a.type = ActionType::PresetClear;      break;
        default:
            if (isNumberRowStrum(hid_usage) || isKeypadStrum(hid_usage)) {
                a.type = ActionType::StrumKey;
            } else {
                a.type = ActionType::None;
            }
            break;
    }

    return a;
}
