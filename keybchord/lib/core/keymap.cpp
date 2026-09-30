#include "keymap.h"

#include "keymap_config.h"
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
constexpr uint8_t HID_USAGE_ESC        = 0x29;
constexpr uint8_t HID_USAGE_ENTER      = 0x28;
constexpr uint8_t HID_USAGE_BACKSPACE  = 0x2A;
constexpr uint8_t HID_USAGE_LEFT       = 0x50;
constexpr uint8_t HID_USAGE_DOWN       = 0x51;
constexpr uint8_t HID_USAGE_RIGHT      = 0x4F;
constexpr uint8_t HID_USAGE_UP         = 0x52;

constexpr uint8_t NUMBER_ROW_STRUM_LO  = 0x1E;  // 1
constexpr uint8_t NUMBER_ROW_STRUM_HI  = 0x27;  // 0
constexpr uint8_t KEYPAD_STRUM_LO      = 0x59;  // Keypad 1
constexpr uint8_t KEYPAD_STRUM_HI      = 0x63;  // Keypad .
constexpr uint8_t HID_USAGE_KP_SLASH   = 0x54;
constexpr uint8_t HID_USAGE_KP_STAR    = 0x55;
constexpr uint8_t HID_USAGE_NUM_LOCK   = 0x53;

bool isNumberRowStrum(uint8_t usage) {
    return usage >= NUMBER_ROW_STRUM_LO && usage <= NUMBER_ROW_STRUM_HI;
}

bool isKeypadStrum(uint8_t usage) {
    return (usage >= KEYPAD_STRUM_LO && usage <= KEYPAD_STRUM_HI)
        || usage == HID_USAGE_KP_SLASH
        || usage == HID_USAGE_KP_STAR
        || usage == HID_USAGE_NUM_LOCK;
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


uint8_t keymapComboMask(uint8_t hid_modifiers) {
    uint8_t out = 0;
    if (hid_modifiers & 0x01) out |= KMOD_CTRL;   // LCtrl
    if (hid_modifiers & 0x04) out |= KMOD_ALT;    // LAlt
    if (hid_modifiers & 0x88) out |= KMOD_SUPER;  // LGui | RGui
    return out;
}

KeymapResolver::KeymapResolver(const KeymapConfig* keymap) : keymap_(keymap) {}

bool KeymapResolver::isChordKey(uint8_t hid_usage) {
    GridCell cell;
    return lookupChordKey(hid_usage, cell);
}

bool KeymapResolver::isSuper(uint8_t modifiers) {
    constexpr uint8_t SUPER = 0x88;  // LGui | RGui
    return (modifiers & SUPER) != 0;
}

bool keymapIsReserved(uint8_t hid_usage) {
    if (KeymapResolver::isChordKey(hid_usage)) return true;
    if (isNumberRowStrum(hid_usage) || isKeypadStrum(hid_usage)) return true;
    switch (hid_usage) {
        case HID_USAGE_BACKTICK:
        case HID_USAGE_ESC:
        case HID_USAGE_ENTER:
        case HID_USAGE_BACKSPACE:
            return true;
        default:
            return false;
    }
}

char keymapCharForUsage(uint8_t hid_usage, uint8_t modifiers) {
    constexpr uint8_t SHIFT_MASK = 0x22;  // LShift 0x02 | RShift 0x20
    bool shift = (modifiers & SHIFT_MASK) != 0;

    switch (hid_usage) {
        // Letters a..z (0x04..0x1D) -> lowercase / uppercase with Shift.
        case 0x04 ... 0x1D:
            return static_cast<char>((shift ? 'A' : 'a') + (hid_usage - 0x04));
        // Number row 1..9,0 (0x1E..0x27) -> digits / ! @ # $ % ^ & * ( ).
        case 0x1E ... 0x27: {
            static const char kNumShift[10] = {
                '!', '@', '#', '$', '%', '^', '&', '*', '(', ')',
            };
            int idx = hid_usage - 0x1E;  // 0..9
            char digit = (idx == 9) ? '0' : static_cast<char>('1' + idx);
            return shift ? kNumShift[idx] : digit;
        }
        case 0x2B: return ' ';          // Tab -> space
        case 0x2C: return ' ';          // Spacebar -> space
        case 0x2D: return shift ? '_' : '-';   // -/_
        case 0x2E: return shift ? '+' : '=';   // =/+
        case 0x2F: return shift ? '{' : '[';   // [/{
        case 0x30: return shift ? '}' : ']';   // ]/}
        case 0x31: return shift ? '|' : '\\';  // \/|
        case 0x33: return shift ? ':' : ';';   // ;/:
        case 0x34: return shift ? '"' : '\'';  // '/"
        case 0x35: return shift ? '~' : '`';   // `/~
        case 0x36: return shift ? '<' : ',';   // ,/<
        case 0x37: return shift ? '>' : '.';   // ./>
        case 0x38: return shift ? '?' : '/';   // /?
        default:   return '\0';
    }
}

KeyAction KeymapResolver::resolve(uint8_t hid_usage, uint8_t modifiers) const {
    KeyAction a;
    uint8_t combo = keymapComboMask(modifiers);
    bool super = (combo & KMOD_SUPER) != 0;

    // Hardcoded safety combos on reserved keys (always win).
    if (super && hid_usage == HID_USAGE_ESC) {
        a.cmd = KeyCmd::Panic;
        return a;
    }
    if (super && isNumberRowStrum(hid_usage)) {
        int idx = static_cast<int>(hid_usage - NUMBER_ROW_STRUM_LO);  // 0..9
        if (idx < NUM_SLOTS) {
            a.cmd  = KeyCmd::PresetLoad;
            a.slot = static_cast<uint8_t>(idx);
            return a;
        }
        return a;  // Super+9/0 -> None
    }

    // Reserved play keys: chord grid, backtick, strum keys, held-extension
    // arrows, and Esc. These must resolve to their action even while Ctrl/Alt
    // is held, otherwise a chord/strum key released mid-combo would be dropped
    // and leave the chord engine latched. Super is the preset/panic modifier
    // where play keys stay inert (and the Super+Esc/Num combos above win).
    GridCell cell;
    if (lookupChordKey(hid_usage, cell)) {
        if (super) return a;                 // Super+chord -> None
        a.cmd  = KeyCmd::ChordKey;
        a.cell = cell;
        return a;
    }
    if (hid_usage == HID_USAGE_BACKTICK) {
        if (super) return a;
        a.cmd = KeyCmd::Backtick;
        return a;
    }
    // Held extension keys (Left/Down/Right) are configurable. They resolve
    // through the *no-modifier* binding so they keep working while Ctrl/Alt is
    // held (they are held modifiers — releasing one mid-combo must still
    // resolve, or the extension would latch). Super still suppresses them.
    // Defaults are add9/add11/add13.
    if (hid_usage == HID_USAGE_LEFT || hid_usage == HID_USAGE_DOWN ||
        hid_usage == HID_USAGE_RIGHT) {
        if (super) return a;
        if (keymap_) {
            const KeyAction* found = keymap_->find(0, hid_usage);
            if (found) return *found;
        }
        return a;
    }
    if (hid_usage == HID_USAGE_ESC)   { if (!super) a.cmd = KeyCmd::ClearEdit; return a; }
    if (isNumberRowStrum(hid_usage) || isKeypadStrum(hid_usage)) {
        if (super) return a;
        a.cmd = KeyCmd::StrumKey;
        return a;
    }

    // Configurable: lookup the (combo, usage) in the keymap table.
    if (keymap_) {
        const KeyAction* found = keymap_->find(combo, hid_usage);
        if (found) return *found;
    }

    return a;  // None
}
