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
    EXPECT_EQ(r.resolve(0x35, 0).type, ActionType::Backtick);       // `
    EXPECT_EQ(r.resolve(0x3A, 0).type, ActionType::PlayModeCycle);  // F1
    EXPECT_EQ(r.resolve(0x3E, 0).type, ActionType::VoicingToggle);  // F5
    EXPECT_EQ(r.resolve(0x50, 0).type, ActionType::ExtToggle9);     // Left
    EXPECT_EQ(r.resolve(0x51, 0).type, ActionType::ExtToggle11);    // Down
    EXPECT_EQ(r.resolve(0x4F, 0).type, ActionType::ExtToggle13);    // Right
    EXPECT_EQ(r.resolve(0x2E, 0).type, ActionType::OctaveUp);       // =
    EXPECT_EQ(r.resolve(0x57, 0).type, ActionType::OctaveUp);       // Keypad+
    EXPECT_EQ(r.resolve(0x2D, 0).type, ActionType::OctaveDown);     // -
    EXPECT_EQ(r.resolve(0x56, 0).type, ActionType::OctaveDown);     // Keypad-
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

TEST(Keymap, AltFunctionKeys) {
    KeymapResolver r;
    const uint8_t ALT = 0x04;  // LAlt modifier bit

    // Alt+F2/F3/F4 arm strum parameter edits; Alt+F5 toggles limited keys.
    EXPECT_EQ(r.resolve(0x3B, ALT).type, ActionType::StrumOctave);   // Alt+F2
    EXPECT_EQ(r.resolve(0x3C, ALT).type, ActionType::StrumDuration); // Alt+F3
    EXPECT_EQ(r.resolve(0x3D, ALT).type, ActionType::StrumVelocity); // Alt+F4
    EXPECT_EQ(r.resolve(0x3E, ALT).type, ActionType::LimitedToggle); // Alt+F5

    // Without Alt, F2/F3/F4 are not yet mapped (chord param editing deferred);
    // F5 remains the voicing toggle.
    EXPECT_EQ(r.resolve(0x3B, 0).type, ActionType::None);
    EXPECT_EQ(r.resolve(0x3C, 0).type, ActionType::None);
    EXPECT_EQ(r.resolve(0x3D, 0).type, ActionType::None);
    EXPECT_EQ(r.resolve(0x3E, 0).type, ActionType::VoicingToggle);
}

TEST(Keymap, EscIsClearEdit) {
    KeymapResolver r;
    EXPECT_EQ(r.resolve(0x29, 0).type, ActionType::ClearEdit);  // Esc
}

TEST(Keymap, RhythmControls) {
    KeymapResolver r;
    const uint8_t CTRL  = 0x01;  // LCtrl modifier bit
    const uint8_t SUPER = 0x08;  // LGui modifier bit

    EXPECT_EQ(r.resolve(0x40, 0).type, ActionType::RhythmToggle);       // F7
    EXPECT_EQ(r.resolve(0x41, 0).type, ActionType::RhythmPatternCycle); // F8
    EXPECT_EQ(r.resolve(0x42, 0).type, ActionType::RhythmMute);         // F9

    EXPECT_EQ(r.resolve(0x4B, 0).type, ActionType::TempoUp);            // Page Up
    EXPECT_EQ(r.resolve(0x4E, 0).type, ActionType::TempoDown);          // Page Down
    EXPECT_EQ(r.resolve(0x4B, CTRL).type, ActionType::SwingUp);         // Ctrl+Page Up
    EXPECT_EQ(r.resolve(0x4E, CTRL).type, ActionType::SwingDown);       // Ctrl+Page Down

    EXPECT_EQ(r.resolve(0x40, SUPER).type, ActionType::ClockToggle);    // Super+F7
    EXPECT_EQ(r.resolve(0x41, SUPER).type, ActionType::LedToggle);      // Super+F8
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

TEST(Keymap, ShiftIsNotFunctionModifier) {
    // Shift (bits 1/5 = 0x02/0x20) is a chord root, never a function modifier.
    EXPECT_FALSE(KeymapResolver::isFunctionModifier(0x02));   // LShift
    EXPECT_FALSE(KeymapResolver::isFunctionModifier(0x20));   // RShift
    EXPECT_FALSE(KeymapResolver::isFunctionModifier(0x22));   // both shifts
    EXPECT_TRUE(KeymapResolver::isFunctionModifier(0x01));    // LCtrl
    EXPECT_TRUE(KeymapResolver::isFunctionModifier(0x04));    // LAlt
    EXPECT_TRUE(KeymapResolver::isFunctionModifier(0x08));    // LGui/Super
}

TEST(Keymap, ModifierDetection) {
    EXPECT_TRUE(KeymapResolver::isCtrl(0x01));     // LCtrl
    EXPECT_TRUE(KeymapResolver::isCtrl(0x10));     // RCtrl
    EXPECT_FALSE(KeymapResolver::isCtrl(0x04));    // Alt is not Ctrl
    EXPECT_TRUE(KeymapResolver::isSuper(0x08));    // LGui
    EXPECT_TRUE(KeymapResolver::isSuper(0x80));    // RGui
    EXPECT_FALSE(KeymapResolver::isSuper(0x01));   // Ctrl is not Super
}
