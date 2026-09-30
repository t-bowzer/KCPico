#include <gtest/gtest.h>

#include "keymap.h"
#include "keymap_config.h"


namespace {

KeymapResolver& defaultResolver() {
    static KeymapConfig cfg = KeymapConfig::defaults();
    static KeymapResolver r(&cfg);
    return r;
}

KeyAction resolve(uint8_t usage, uint8_t mods = 0) {
    return defaultResolver().resolve(usage, mods);
}

void expectGridCell(uint8_t usage, ChordQuality quality, int column) {
    KeyAction a = resolve(usage, 0);
    EXPECT_EQ(a.cmd, KeyCmd::ChordKey) << "usage=0x" << std::hex << (int)usage;
    if (a.cmd == KeyCmd::ChordKey) {
        EXPECT_EQ(a.cell.quality, quality);
        EXPECT_EQ(a.cell.column, column);
    }
}

constexpr uint8_t SUPER = 0x08;  // LGui
constexpr uint8_t CTRL  = 0x01;  // LCtrl
constexpr uint8_t ALT   = 0x04;  // LAlt

} // namespace


TEST(Keymap, MajorRowGrid) {
    expectGridCell(0x2B, ChordQuality::Major, 0);   // Tab
    expectGridCell(0x14, ChordQuality::Major, 1);   // Q
    expectGridCell(0x1A, ChordQuality::Major, 2);   // W
    expectGridCell(0x08, ChordQuality::Major, 3);   // E
    expectGridCell(0x15, ChordQuality::Major, 4);   // R
    expectGridCell(0x17, ChordQuality::Major, 5);   // T
    expectGridCell(0x1C, ChordQuality::Major, 6);   // Y
    expectGridCell(0x18, ChordQuality::Major, 7);   // U
    expectGridCell(0x0C, ChordQuality::Major, 8);   // I
    expectGridCell(0x12, ChordQuality::Major, 9);   // O
    expectGridCell(0x13, ChordQuality::Major, 10);  // P
    expectGridCell(0x2F, ChordQuality::Major, 11);  // [
}

TEST(Keymap, MinorRowGrid) {
    expectGridCell(0x39, ChordQuality::Minor, 0);   // Caps
    expectGridCell(0x04, ChordQuality::Minor, 1);   // A
    expectGridCell(0x16, ChordQuality::Minor, 2);   // S
    expectGridCell(0x07, ChordQuality::Minor, 3);   // D
    expectGridCell(0x09, ChordQuality::Minor, 4);   // F
    expectGridCell(0x0A, ChordQuality::Minor, 5);   // G
    expectGridCell(0x0B, ChordQuality::Minor, 6);   // H
    expectGridCell(0x0D, ChordQuality::Minor, 7);   // J
    expectGridCell(0x0E, ChordQuality::Minor, 8);   // K
    expectGridCell(0x0F, ChordQuality::Minor, 9);   // L
    expectGridCell(0x33, ChordQuality::Minor, 10);  // ;
    expectGridCell(0x34, ChordQuality::Minor, 11);  // '
}

TEST(Keymap, SeventhRowGrid) {
    expectGridCell(0xE2, ChordQuality::Seventh, 0);   // Left Shift
    expectGridCell(0x1D, ChordQuality::Seventh, 1);   // Z
    expectGridCell(0x1B, ChordQuality::Seventh, 2);   // X
    expectGridCell(0x06, ChordQuality::Seventh, 3);   // C
    expectGridCell(0x19, ChordQuality::Seventh, 4);   // V
    expectGridCell(0x05, ChordQuality::Seventh, 5);   // B
    expectGridCell(0x11, ChordQuality::Seventh, 6);   // N
    expectGridCell(0x10, ChordQuality::Seventh, 7);   // M
    expectGridCell(0x36, ChordQuality::Seventh, 8);   // ,
    expectGridCell(0x37, ChordQuality::Seventh, 9);   // .
    expectGridCell(0x38, ChordQuality::Seventh, 10);  // /
    expectGridCell(0xE6, ChordQuality::Seventh, 11);  // Right Shift
}

TEST(Keymap, ReservedChordControls) {
    EXPECT_EQ(resolve(0x35, 0).cmd, KeyCmd::Backtick);  // `
    EXPECT_EQ(resolve(0x50, 0).cmd, KeyCmd::Ext9);      // Left
    EXPECT_EQ(resolve(0x51, 0).cmd, KeyCmd::Ext11);     // Down
    EXPECT_EQ(resolve(0x4F, 0).cmd, KeyCmd::Ext13);     // Right
}

TEST(Keymap, StrumKeysResolve) {
    EXPECT_EQ(resolve(0x1E, 0).cmd, KeyCmd::StrumKey);  // 1
    EXPECT_EQ(resolve(0x27, 0).cmd, KeyCmd::StrumKey);  // 0
    EXPECT_EQ(resolve(0x62, 0).cmd, KeyCmd::StrumKey);  // Keypad 0
    EXPECT_EQ(resolve(0x63, 0).cmd, KeyCmd::StrumKey);  // Keypad .
    EXPECT_EQ(resolve(0x54, 0).cmd, KeyCmd::StrumKey);  // Keypad /
    EXPECT_EQ(resolve(0x55, 0).cmd, KeyCmd::StrumKey);  // Keypad *
    EXPECT_EQ(resolve(0x53, 0).cmd, KeyCmd::StrumKey);  // Num Lock
}

TEST(Keymap, CommonSettingsF1ToF4) {
    KeyAction a = resolve(0x3A);  // F1
    EXPECT_EQ(a.cmd, KeyCmd::CycleParam);
    EXPECT_EQ(a.param, ParamId::ChordMode);

    a = resolve(0x3B);  // F2 -> arp mode
    EXPECT_EQ(a.cmd, KeyCmd::CycleParam);
    EXPECT_EQ(a.param, ParamId::ChordArpMode);

    a = resolve(0x3C);  // F3 -> bass pattern
    EXPECT_EQ(a.cmd, KeyCmd::CycleParam);
    EXPECT_EQ(a.param, ParamId::BassPattern);

    a = resolve(0x3D);  // F4 -> rhythm pattern
    EXPECT_EQ(a.cmd, KeyCmd::CycleParam);
    EXPECT_EQ(a.param, ParamId::RhythmPattern);
}

TEST(Keymap, TogglesF5ToF8) {
    EXPECT_EQ(resolve(0x3E).cmd, KeyCmd::ToggleParam);  // F5 -> rhythm enable
    EXPECT_EQ(resolve(0x3E).param, ParamId::RhythmEnable);
    EXPECT_EQ(resolve(0x3F).param, ParamId::RhythmMute);   // F6
    EXPECT_EQ(resolve(0x40).param, ParamId::BassEnable);   // F7
    EXPECT_EQ(resolve(0x41).param, ParamId::RhythmClock);  // F8
}

TEST(Keymap, MenusF9ToF12) {
    EXPECT_EQ(resolve(0x42).menu, EditMenu::Chord);   // F9
    EXPECT_EQ(resolve(0x43).menu, EditMenu::Strum);   // F10
    EXPECT_EQ(resolve(0x44).menu, EditMenu::Rhythm);  // F11
    EXPECT_EQ(resolve(0x45).menu, EditMenu::Bass);    // F12
    EXPECT_EQ(resolve(0x42).cmd, KeyCmd::OpenMenu);
}

TEST(Keymap, SuperF11OpensDrum) {
    KeyAction a = resolve(0x44, SUPER);  // Super+F11
    EXPECT_EQ(a.cmd, KeyCmd::OpenMenu);
    EXPECT_EQ(a.menu, EditMenu::Drum);
}

TEST(Keymap, InversionKeys) {
    KeyAction a = resolve(0x46);  // PrtSc
    EXPECT_EQ(a.cmd, KeyCmd::SetParam);
    EXPECT_EQ(a.param, ParamId::ChordInversion);
    EXPECT_EQ(a.valueA, 1);

    EXPECT_EQ(resolve(0x47).valueA, 2);  // ScLk -> second
    EXPECT_EQ(resolve(0x48).valueA, 3);  // Pause -> third
}

TEST(Keymap, OctaveKeys) {
    KeyAction a = resolve(0x2E);  // =
    EXPECT_EQ(a.cmd, KeyCmd::IncParam);
    EXPECT_EQ(a.param, ParamId::ChordOctave);

    a = resolve(0x2D);  // -
    EXPECT_EQ(a.cmd, KeyCmd::DecParam);
    EXPECT_EQ(a.param, ParamId::ChordOctave);

    EXPECT_EQ(resolve(0x57).param, ParamId::StrumOctave);  // Kp+
    EXPECT_EQ(resolve(0x57).cmd, KeyCmd::IncParam);
    EXPECT_EQ(resolve(0x56).cmd, KeyCmd::DecParam);        // Kp-
}

TEST(Keymap, TempoKeys) {
    EXPECT_EQ(resolve(0x4B).cmd, KeyCmd::IncParam);   // PgUp
    EXPECT_EQ(resolve(0x4B).param, ParamId::RhythmTempo);
    EXPECT_EQ(resolve(0x4E).cmd, KeyCmd::DecParam);   // PgDn
    EXPECT_EQ(resolve(0x2C).cmd, KeyCmd::TapTempo);   // Space
}

TEST(Keymap, PresetNavigation) {
    EXPECT_EQ(resolve(0x4A).cmd, KeyCmd::PresetPrev);  // Home
    EXPECT_EQ(resolve(0x4D).cmd, KeyCmd::PresetNext);  // End
    EXPECT_EQ(resolve(0x49).cmd, KeyCmd::PresetSave);  // Insert
    EXPECT_EQ(resolve(0x4C).cmd, KeyCmd::PresetClear); // Delete
}

TEST(Keymap, SuperPresetBanks) {
    EXPECT_EQ(resolve(0x4A, SUPER).cmd, KeyCmd::PresetBankPrev);
    EXPECT_EQ(resolve(0x4D, SUPER).cmd, KeyCmd::PresetBankNext);
}

TEST(Keymap, SuperNumberRowLoadsPreset) {
    for (int n = 0; n < 8; n++) {
        KeyAction a = resolve(0x1E + n, SUPER);
        EXPECT_EQ(a.cmd, KeyCmd::PresetLoad);
        EXPECT_EQ(a.slot, n);
    }
    EXPECT_EQ(resolve(0x26, SUPER).cmd, KeyCmd::None);  // 9
    EXPECT_EQ(resolve(0x27, SUPER).cmd, KeyCmd::None);  // 0
}

TEST(Keymap, SuperEscIsPanic) {
    EXPECT_EQ(resolve(0x29, SUPER).cmd, KeyCmd::Panic);
    EXPECT_EQ(resolve(0x29, 0).cmd, KeyCmd::ClearEdit);
    EXPECT_EQ(resolve(0x29, 0x80).cmd, KeyCmd::Panic);  // RGui
}

TEST(Keymap, SuperChordKeyIsInert) {
    EXPECT_EQ(resolve(0x14, SUPER).cmd, KeyCmd::None);  // Q
    EXPECT_EQ(resolve(0x1E, 0).cmd, KeyCmd::StrumKey);  // 1 without Super
}

TEST(Keymap, PlayKeysResolveWhileCtrlAltHeld) {
    // A chord/strum key released while Ctrl/Alt is still held must still resolve
    // to its play action, or the chord engine would miss the release and latch.
    EXPECT_EQ(resolve(0x17, CTRL).cmd, KeyCmd::ChordKey);  // T with Ctrl
    EXPECT_EQ(resolve(0x17, ALT).cmd, KeyCmd::ChordKey);   // T with Alt
    EXPECT_EQ(resolve(0x1E, CTRL).cmd, KeyCmd::StrumKey);  // 1 with Ctrl
    EXPECT_EQ(resolve(0x1E, ALT).cmd, KeyCmd::StrumKey);   // 1 with Alt
    EXPECT_EQ(resolve(0x35, CTRL).cmd, KeyCmd::Backtick);  // ` with Ctrl
    EXPECT_EQ(resolve(0x50, CTRL).cmd, KeyCmd::Ext9);      // Left with Ctrl
}

TEST(Keymap, CtrlCombos) {
    KeyAction a = resolve(0x3A, CTRL);  // Ctrl+F1 -> cycle voicing
    EXPECT_EQ(a.cmd, KeyCmd::CycleParam);
    EXPECT_EQ(a.param, ParamId::ChordVoicing);

    EXPECT_EQ(resolve(0x3B, CTRL).param, ParamId::ChordInversion);  // Ctrl+F2
    EXPECT_EQ(resolve(0x3C, CTRL).param, ParamId::ChordArpMode);    // Ctrl+F3

    a = resolve(0x3D, CTRL);  // Ctrl+F4 -> toggle roll 0/50
    EXPECT_EQ(a.cmd, KeyCmd::ToggleParam);
    EXPECT_EQ(a.param, ParamId::ChordRoll);
    EXPECT_EQ(a.valueA, 0);
    EXPECT_EQ(a.valueB, 50);

    EXPECT_EQ(resolve(0x3E, CTRL).param, ParamId::RhythmLed);   // Ctrl+F5
    EXPECT_EQ(resolve(0x3F, CTRL).cmd, KeyCmd::None);           // Ctrl+F6 removed

    a = resolve(0x2E, CTRL);  // Ctrl+= -> inc chord velocity
    EXPECT_EQ(a.cmd, KeyCmd::IncParam);
    EXPECT_EQ(a.param, ParamId::ChordVelocity);
    EXPECT_EQ(resolve(0x2D, CTRL).cmd, KeyCmd::DecParam);       // Ctrl+- -> dec

    EXPECT_EQ(resolve(0x58, CTRL).param, ParamId::StrumLayout); // Ctrl+KpEnter
    EXPECT_EQ(resolve(0x57, CTRL).param, ParamId::StrumScale);  // Ctrl+Kp+
    EXPECT_EQ(resolve(0x56, CTRL).cmd, KeyCmd::DecParam);       // Ctrl+Kp-
}

TEST(Keymap, AltCombos) {
    EXPECT_EQ(resolve(0x58, ALT).param, ParamId::StrumMode);    // Alt+KpEnter
    EXPECT_EQ(resolve(0x57, ALT).param, ParamId::StrumRoot);    // Alt+Kp+
    EXPECT_EQ(resolve(0x56, ALT).cmd, KeyCmd::DecParam);        // Alt+Kp-

    KeyAction a = resolve(0x2E, ALT);  // Alt+= -> inc bass octave
    EXPECT_EQ(a.cmd, KeyCmd::IncParam);
    EXPECT_EQ(a.param, ParamId::BassOctave);
    EXPECT_EQ(resolve(0x2D, ALT).cmd, KeyCmd::DecParam);        // Alt+- -> dec
    EXPECT_EQ(resolve(0x2D, ALT).param, ParamId::BassOctave);

    a = resolve(0x3A, ALT);  // Alt+F1 -> drum mute 0
    EXPECT_EQ(a.cmd, KeyCmd::DrumMute);
    EXPECT_EQ(a.slot, 0);
    EXPECT_EQ(resolve(0x45, ALT).slot, 11);  // Alt+F12 -> drum mute 11
}

TEST(Keymap, UnmappedKeysAreNone) {
    EXPECT_EQ(resolve(0x28, 0).cmd, KeyCmd::None);  // Enter
    EXPECT_EQ(resolve(0x65, 0).cmd, KeyCmd::None);  // Menu key
    EXPECT_EQ(resolve(0xE1, 0).cmd, KeyCmd::None);  // LCtrl press
    EXPECT_EQ(resolve(0xE3, 0).cmd, KeyCmd::None);  // LAlt press
}

TEST(Keymap, NullResolverReturnsNoneForConfigurable) {
    KeymapResolver r;  // null keymap
    EXPECT_EQ(r.resolve(0x3A, 0).cmd, KeyCmd::None);        // F1
    EXPECT_EQ(r.resolve(0x14, 0).cmd, KeyCmd::ChordKey);    // Q still reserved
    EXPECT_EQ(r.resolve(0x29, 0).cmd, KeyCmd::ClearEdit);   // Esc still reserved
}

TEST(Keymap, IsChordKey) {
    EXPECT_TRUE(KeymapResolver::isChordKey(0x14));   // Q
    EXPECT_TRUE(KeymapResolver::isChordKey(0x2B));   // Tab
    EXPECT_TRUE(KeymapResolver::isChordKey(0xE2));   // LShift
    EXPECT_TRUE(KeymapResolver::isChordKey(0x2F));   // [
    EXPECT_FALSE(KeymapResolver::isChordKey(0x35));  // `
    EXPECT_FALSE(KeymapResolver::isChordKey(0x1E));  // 1
    EXPECT_FALSE(KeymapResolver::isChordKey(0x3A));  // F1
}

TEST(Keymap, SuperModifierDetection) {
    EXPECT_TRUE(KeymapResolver::isSuper(0x08));    // LGui
    EXPECT_TRUE(KeymapResolver::isSuper(0x80));    // RGui
    EXPECT_FALSE(KeymapResolver::isSuper(0x01));   // LCtrl is not Super
    EXPECT_FALSE(KeymapResolver::isSuper(0x04));   // LAlt is not Super
}

TEST(Keymap, ComboMask) {
    EXPECT_EQ(keymapComboMask(0x08), KMOD_SUPER);
    EXPECT_EQ(keymapComboMask(0x80), KMOD_SUPER);
    EXPECT_EQ(keymapComboMask(0x01), KMOD_CTRL);
    EXPECT_EQ(keymapComboMask(0x04), KMOD_ALT);
    EXPECT_EQ(keymapComboMask(0x00), 0);
    // Shift is not a combo modifier.
    EXPECT_EQ(keymapComboMask(0x02), 0);
    EXPECT_EQ(keymapComboMask(0x20), 0);
    // RCtrl/RAlt are ignored (only left modifiers participate).
    EXPECT_EQ(keymapComboMask(0x10), 0);
    EXPECT_EQ(keymapComboMask(0x40), 0);
}

TEST(Keymap, ReservedFlag) {
    EXPECT_TRUE(keymapIsReserved(0x14));   // Q
    EXPECT_TRUE(keymapIsReserved(0x1E));   // 1
    EXPECT_TRUE(keymapIsReserved(0x35));   // backtick
    EXPECT_TRUE(keymapIsReserved(0x29));   // Esc
    EXPECT_TRUE(keymapIsReserved(0x28));   // Enter
    EXPECT_FALSE(keymapIsReserved(0x50));  // Left (now bindable)
    EXPECT_FALSE(keymapIsReserved(0x51));  // Down (now bindable)
    EXPECT_FALSE(keymapIsReserved(0x4F));  // Right (now bindable)
    EXPECT_FALSE(keymapIsReserved(0x52));  // Up (now bindable)
    EXPECT_FALSE(keymapIsReserved(0x3A));  // F1
    EXPECT_FALSE(keymapIsReserved(0x4A));  // Home
}

TEST(Keymap, CharForUsage) {
    EXPECT_EQ(keymapCharForUsage(0x14), 'q');
    EXPECT_EQ(keymapCharForUsage(0x1A), 'w');
    EXPECT_EQ(keymapCharForUsage(0x04), 'a');
    EXPECT_EQ(keymapCharForUsage(0x1D), 'z');
    EXPECT_EQ(keymapCharForUsage(0x2F), '[');
    EXPECT_EQ(keymapCharForUsage(0x33), ';');
    EXPECT_EQ(keymapCharForUsage(0x38), '/');
    EXPECT_EQ(keymapCharForUsage(0x2B), ' ');   // Tab -> space
    EXPECT_EQ(keymapCharForUsage(0x2C), ' ');   // Spacebar -> space
    EXPECT_EQ(keymapCharForUsage(0x29), '\0');  // Esc
    EXPECT_EQ(keymapCharForUsage(0x3A), '\0');  // F1
    EXPECT_EQ(keymapCharForUsage(0x00), '\0');
}

TEST(Keymap, CharForUsageNumbers) {
    EXPECT_EQ(keymapCharForUsage(0x1E), '1');
    EXPECT_EQ(keymapCharForUsage(0x1F), '2');
    EXPECT_EQ(keymapCharForUsage(0x27), '0');
}

TEST(Keymap, CharForUsageShifted) {
    constexpr uint8_t SHIFT = 0x02;  // LShift

    // Capital letters.
    EXPECT_EQ(keymapCharForUsage(0x04, SHIFT), 'A');
    EXPECT_EQ(keymapCharForUsage(0x1D, SHIFT), 'Z');

    // Number-row symbols.
    EXPECT_EQ(keymapCharForUsage(0x1E, SHIFT), '!');
    EXPECT_EQ(keymapCharForUsage(0x27, SHIFT), ')');

    // Punctuation shifts.
    EXPECT_EQ(keymapCharForUsage(0x2D, SHIFT), '_');   // underscore
    EXPECT_EQ(keymapCharForUsage(0x2E, SHIFT), '+');
    EXPECT_EQ(keymapCharForUsage(0x2F, SHIFT), '{');
    EXPECT_EQ(keymapCharForUsage(0x30, SHIFT), '}');
    EXPECT_EQ(keymapCharForUsage(0x31, SHIFT), '|');
    EXPECT_EQ(keymapCharForUsage(0x33, SHIFT), ':');
    EXPECT_EQ(keymapCharForUsage(0x34, SHIFT), '"');
    EXPECT_EQ(keymapCharForUsage(0x35, SHIFT), '~');
    EXPECT_EQ(keymapCharForUsage(0x36, SHIFT), '<');
    EXPECT_EQ(keymapCharForUsage(0x37, SHIFT), '>');
    EXPECT_EQ(keymapCharForUsage(0x38, SHIFT), '?');

    // Right shift also shifts.
    EXPECT_EQ(keymapCharForUsage(0x04, 0x20), 'A');

    // Ctrl/Alt/Super are not shift.
    EXPECT_EQ(keymapCharForUsage(0x04, 0x01), 'a');
    EXPECT_EQ(keymapCharForUsage(0x04, 0x04), 'a');
    EXPECT_EQ(keymapCharForUsage(0x04, 0x08), 'a');
}
