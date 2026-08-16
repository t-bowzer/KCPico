#include <gtest/gtest.h>

#include "keymap.h"


static void expectGridCell(const KeymapResolver& r, uint8_t usage,
                           ChordQuality quality, int column) {
    KeyAction a = r.resolve(usage, 0);
    EXPECT_EQ(a.type, ActionType::ChordKey) << "usage=0x" << std::hex << (int)usage;
    if (a.type == ActionType::ChordKey) {
        EXPECT_EQ(a.cell.quality, quality);
        EXPECT_EQ(a.cell.column, column);
    }
}

TEST(Keymap, MajorRowGrid) {
    KeymapResolver r;
    expectGridCell(r, 0x2B, ChordQuality::Major, 0);   // Tab
    expectGridCell(r, 0x14, ChordQuality::Major, 1);   // Q
    expectGridCell(r, 0x1A, ChordQuality::Major, 2);   // W
    expectGridCell(r, 0x08, ChordQuality::Major, 3);   // E
    expectGridCell(r, 0x15, ChordQuality::Major, 4);   // R
    expectGridCell(r, 0x17, ChordQuality::Major, 5);   // T
    expectGridCell(r, 0x1C, ChordQuality::Major, 6);   // Y
    expectGridCell(r, 0x18, ChordQuality::Major, 7);   // U
    expectGridCell(r, 0x0C, ChordQuality::Major, 8);   // I
    expectGridCell(r, 0x12, ChordQuality::Major, 9);   // O
    expectGridCell(r, 0x13, ChordQuality::Major, 10);  // P
    expectGridCell(r, 0x2F, ChordQuality::Major, 11);  // [
}

TEST(Keymap, MinorRowGrid) {
    KeymapResolver r;
    expectGridCell(r, 0x39, ChordQuality::Minor, 0);   // Caps
    expectGridCell(r, 0x04, ChordQuality::Minor, 1);   // A
    expectGridCell(r, 0x16, ChordQuality::Minor, 2);   // S
    expectGridCell(r, 0x07, ChordQuality::Minor, 3);   // D
    expectGridCell(r, 0x09, ChordQuality::Minor, 4);   // F
    expectGridCell(r, 0x0A, ChordQuality::Minor, 5);   // G
    expectGridCell(r, 0x0B, ChordQuality::Minor, 6);   // H
    expectGridCell(r, 0x0D, ChordQuality::Minor, 7);   // J
    expectGridCell(r, 0x0E, ChordQuality::Minor, 8);   // K
    expectGridCell(r, 0x0F, ChordQuality::Minor, 9);   // L
    expectGridCell(r, 0x33, ChordQuality::Minor, 10);  // ;
    expectGridCell(r, 0x34, ChordQuality::Minor, 11);  // '
}

TEST(Keymap, SeventhRowGrid) {
    KeymapResolver r;
    expectGridCell(r, 0xE2, ChordQuality::Seventh, 0);   // Left Shift
    expectGridCell(r, 0x1D, ChordQuality::Seventh, 1);   // Z
    expectGridCell(r, 0x1B, ChordQuality::Seventh, 2);   // X
    expectGridCell(r, 0x06, ChordQuality::Seventh, 3);   // C
    expectGridCell(r, 0x19, ChordQuality::Seventh, 4);   // V
    expectGridCell(r, 0x05, ChordQuality::Seventh, 5);   // B
    expectGridCell(r, 0x11, ChordQuality::Seventh, 6);   // N
    expectGridCell(r, 0x10, ChordQuality::Seventh, 7);   // M
    expectGridCell(r, 0x36, ChordQuality::Seventh, 8);   // ,
    expectGridCell(r, 0x37, ChordQuality::Seventh, 9);   // .
    expectGridCell(r, 0x38, ChordQuality::Seventh, 10);  // /
    expectGridCell(r, 0xE6, ChordQuality::Seventh, 11);  // Right Shift
}

TEST(Keymap, ChordControls) {
    KeymapResolver r;
    EXPECT_EQ(r.resolve(0x35, 0).type, ActionType::Backtick);        // `
    EXPECT_EQ(r.resolve(0x3A, 0).type, ActionType::PlayModeCycle);   // F1
    EXPECT_EQ(r.resolve(0x3E, 0).type, ActionType::VoicingToggle);   // F5
    EXPECT_EQ(r.resolve(0x50, 0).type, ActionType::ExtToggle9);      // Left
    EXPECT_EQ(r.resolve(0x51, 0).type, ActionType::ExtToggle11);     // Down
    EXPECT_EQ(r.resolve(0x4F, 0).type, ActionType::ExtToggle13);     // Right
    EXPECT_EQ(r.resolve(0x2E, 0).type, ActionType::ChordOctaveUp);   // =
    EXPECT_EQ(r.resolve(0x2D, 0).type, ActionType::ChordOctaveDown); // -
}

TEST(Keymap, StrumOctaveKeys) {
    KeymapResolver r;
    EXPECT_EQ(r.resolve(0x57, 0).type, ActionType::StrumOctaveUp);   // Keypad +
    EXPECT_EQ(r.resolve(0x56, 0).type, ActionType::StrumOctaveDown); // Keypad -
}

TEST(Keymap, StrumKeysResolve) {
    KeymapResolver r;
    // Number row 1..0 -> StrumKey.
    EXPECT_EQ(r.resolve(0x1E, 0).type, ActionType::StrumKey);  // 1
    EXPECT_EQ(r.resolve(0x27, 0).type, ActionType::StrumKey);  // 0
    // Keypad digits/decimal -> StrumKey (Num-Lock independent).
    EXPECT_EQ(r.resolve(0x62, 0).type, ActionType::StrumKey);  // Keypad 0
    EXPECT_EQ(r.resolve(0x63, 0).type, ActionType::StrumKey);  // Keypad .
    EXPECT_EQ(r.resolve(0x59, 0).type, ActionType::StrumKey);  // Keypad 1
    EXPECT_EQ(r.resolve(0x61, 0).type, ActionType::StrumKey);  // Keypad 9
    // Keypad / and * -> StrumKey (limited layout only, filtered in engine).
    EXPECT_EQ(r.resolve(0x54, 0).type, ActionType::StrumKey);  // Keypad /
    EXPECT_EQ(r.resolve(0x55, 0).type, ActionType::StrumKey);  // Keypad *
}

TEST(Keymap, MenuToggleKeys) {
    KeymapResolver r;
    EXPECT_EQ(r.resolve(0x65, 0).type, ActionType::MenuChord);  // Menu key
    EXPECT_EQ(r.resolve(0xE1, 0).type, ActionType::MenuRhythm); // LCtrl
    EXPECT_EQ(r.resolve(0xE5, 0).type, ActionType::MenuRhythm); // RCtrl
    EXPECT_EQ(r.resolve(0xE3, 0).type, ActionType::MenuStrum);  // LAlt
    EXPECT_EQ(r.resolve(0xE7, 0).type, ActionType::MenuStrum);  // RAlt
}

TEST(Keymap, EscIsClearEdit) {
    KeymapResolver r;
    EXPECT_EQ(r.resolve(0x29, 0).type, ActionType::ClearEdit);  // Esc
}

TEST(Keymap, RhythmControls) {
    KeymapResolver r;
    EXPECT_EQ(r.resolve(0x40, 0).type, ActionType::RhythmToggle);       // F7
    EXPECT_EQ(r.resolve(0x41, 0).type, ActionType::RhythmPatternCycle); // F8
    EXPECT_EQ(r.resolve(0x42, 0).type, ActionType::RhythmMute);         // F9
    EXPECT_EQ(r.resolve(0x4B, 0).type, ActionType::TempoUp);            // Page Up
    EXPECT_EQ(r.resolve(0x4E, 0).type, ActionType::TempoDown);          // Page Down
}

TEST(Keymap, UnmappedKeysAreNone) {
    KeymapResolver r;
    EXPECT_EQ(r.resolve(0x28, 0).type, ActionType::None);  // Enter
    EXPECT_EQ(r.resolve(0x53, 0).type, ActionType::None);  // Num Lock
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

TEST(Keymap, PresetNavigationKeys) {
    KeymapResolver r;
    EXPECT_EQ(r.resolve(0x4A, 0).type, ActionType::PresetPrev);  // Home
    EXPECT_EQ(r.resolve(0x4D, 0).type, ActionType::PresetNext);  // End
}

TEST(Keymap, SuperPresetKeys) {
    KeymapResolver r;
    constexpr uint8_t SUPER = 0x08;

    EXPECT_EQ(r.resolve(0x4A, SUPER).type, ActionType::PresetBankPrev);  // Super+Home
    EXPECT_EQ(r.resolve(0x4D, SUPER).type, ActionType::PresetBankNext);  // Super+End
}

TEST(Keymap, InsertDeleteAreSaveClear) {
    KeymapResolver r;
    EXPECT_EQ(r.resolve(0x49, 0).type, ActionType::PresetSave);   // Insert
    EXPECT_EQ(r.resolve(0x4C, 0).type, ActionType::PresetClear);  // Delete

    // Super no longer binds Insert/Delete.
    EXPECT_EQ(r.resolve(0x49, 0x08).type, ActionType::None);
    EXPECT_EQ(r.resolve(0x4C, 0x08).type, ActionType::None);
}

TEST(Keymap, F6TogglesClockOut) {
    KeymapResolver r;
    EXPECT_EQ(r.resolve(0x3F, 0).type, ActionType::RhythmClockToggle);  // F6
}

TEST(Keymap, F10TogglesRhythmLed) {
    KeymapResolver r;
    EXPECT_EQ(r.resolve(0x43, 0).type, ActionType::RhythmLedToggle);  // F10
}

TEST(Keymap, SuperNumberRowLoadsPreset) {
    KeymapResolver r;
    constexpr uint8_t SUPER = 0x08;

    // Super+1..8 -> PresetLoad with index 0..7.
    for (int n = 0; n < 8; n++) {
        KeyAction a = r.resolve(0x1E + n, SUPER);
        EXPECT_EQ(a.type, ActionType::PresetLoad);
        EXPECT_EQ(a.index, n);
    }

    // Super+9 and Super+0 are not valid preset slots -> None.
    EXPECT_EQ(r.resolve(0x26, SUPER).type, ActionType::None);  // 9
    EXPECT_EQ(r.resolve(0x27, SUPER).type, ActionType::None);  // 0
}

TEST(Keymap, SuperPressIsInert) {
    KeymapResolver r;
    constexpr uint8_t SUPER = 0x08;

    EXPECT_EQ(r.resolve(0xE4, SUPER).type, ActionType::None);  // LGui press
    EXPECT_EQ(r.resolve(0xE8, SUPER).type, ActionType::None);  // RGui press
}

TEST(Keymap, SuperChordKeyIsInert) {
    KeymapResolver r;
    constexpr uint8_t SUPER = 0x08;

    // Chord keys are not chord keys while Super is held.
    EXPECT_EQ(r.resolve(0x14, SUPER).type, ActionType::None);  // Q
    EXPECT_EQ(r.resolve(0x1E, 0).type, ActionType::StrumKey);  // 1 without Super
}
