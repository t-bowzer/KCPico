#include <gtest/gtest.h>

#include "strum.h"


// Helper to build a C major (rootPc 0) resolved chord for pool tests.
static ResolvedChord cMajor() {
    ResolvedChord c;
    c.rootPc = 0;
    c.type   = ChordType::Major;
    return c;
}

TEST(Strum, PoolCountAndAscending) {
    auto pool = buildNotePool(cMajor(), 60, 1, 11);
    ASSERT_EQ(pool.size(), 11u);
    // C major pitch classes {0,4,7}, anchor = 60 + 12*1 = 72.
    EXPECT_EQ(pool, (std::vector<uint8_t>{72, 76, 79, 84, 88, 91, 96, 100, 103, 108, 112}));
    for (size_t i = 1; i < pool.size(); i++) {
        EXPECT_GE(pool[i], pool[i - 1]);
    }
}

TEST(Strum, PoolOctaveShift) {
    // strumOctave 0 anchors at C4 (60).
    auto p0 = buildNotePool(cMajor(), 60, 0, 11);
    EXPECT_EQ(p0, (std::vector<uint8_t>{60, 64, 67, 72, 76, 79, 84, 88, 91, 96, 100}));

    // strumOctave -1 anchors at C3 (48).
    auto pm1 = buildNotePool(cMajor(), 60, -1, 10);
    EXPECT_EQ(pm1, (std::vector<uint8_t>{48, 52, 55, 60, 64, 67, 72, 76, 79, 84}));
}

TEST(Strum, PoolClampsToMidiRange) {
    // strumOctave +3 anchors at C7 (96); high notes must clamp at 127.
    auto pool = buildNotePool(cMajor(), 60, 3, 11);
    ASSERT_EQ(pool.size(), 11u);
    for (uint8_t n : pool) {
        EXPECT_LE(n, 127);
    }
    EXPECT_EQ(pool.back(), 127);
}

TEST(Strum, PoolIncludesExtensions) {
    ResolvedChord c = cMajor();
    c.add9 = true;  // pitch classes {0,2,4,7}
    auto pool = buildNotePool(c, 60, 1, 8);
    // anchor 72: 72(C) 74(D) 76(E) 79(G) 84(C) 86(D) 88(E) 91(G)
    EXPECT_EQ(pool, (std::vector<uint8_t>{72, 74, 76, 79, 84, 86, 88, 91}));
}

TEST(Strum, EmptyPoolForZeroCount) {
    auto pool = buildNotePool(cMajor(), 60, 1, 0);
    EXPECT_TRUE(pool.empty());
}

TEST(Strum, FullLayoutNumberRowOrdering) {
    EXPECT_EQ(strumIndexFor(0x1E, StrumLayout::Full), 0);  // 1
    EXPECT_EQ(strumIndexFor(0x1F, StrumLayout::Full), 1);  // 2
    EXPECT_EQ(strumIndexFor(0x26, StrumLayout::Full), 8);  // 9
    EXPECT_EQ(strumIndexFor(0x27, StrumLayout::Full), 9);  // 0
}

TEST(Strum, FullLayoutNumpadOrdering) {
    EXPECT_EQ(strumIndexFor(0x62, StrumLayout::Full), 0);   // Keypad 0
    EXPECT_EQ(strumIndexFor(0x63, StrumLayout::Full), 1);   // Keypad .
    EXPECT_EQ(strumIndexFor(0x59, StrumLayout::Full), 2);   // Keypad 1
    EXPECT_EQ(strumIndexFor(0x61, StrumLayout::Full), 10);  // Keypad 9
}

TEST(Strum, FullLayoutExcludesKeypadSlashStar) {
    EXPECT_EQ(strumIndexFor(0x54, StrumLayout::Full), -1);  // Keypad /
    EXPECT_EQ(strumIndexFor(0x55, StrumLayout::Full), -1);  // Keypad *
}

TEST(Strum, LimitedLayoutOrdering) {
    // 0 . 2 3 5 6 8 9 / *
    EXPECT_EQ(strumIndexFor(0x62, StrumLayout::Limited), 0);  // 0
    EXPECT_EQ(strumIndexFor(0x63, StrumLayout::Limited), 1);  // .
    EXPECT_EQ(strumIndexFor(0x5A, StrumLayout::Limited), 2);  // 2
    EXPECT_EQ(strumIndexFor(0x5B, StrumLayout::Limited), 3);  // 3
    EXPECT_EQ(strumIndexFor(0x5D, StrumLayout::Limited), 4);  // 5
    EXPECT_EQ(strumIndexFor(0x5E, StrumLayout::Limited), 5);  // 6
    EXPECT_EQ(strumIndexFor(0x60, StrumLayout::Limited), 6);  // 8
    EXPECT_EQ(strumIndexFor(0x61, StrumLayout::Limited), 7);  // 9
    EXPECT_EQ(strumIndexFor(0x54, StrumLayout::Limited), 8);  // /
    EXPECT_EQ(strumIndexFor(0x55, StrumLayout::Limited), 9);  // *
}

TEST(Strum, LimitedLayoutExcludesOthers) {
    EXPECT_EQ(strumIndexFor(0x1E, StrumLayout::Limited), -1);  // number row 1
    EXPECT_EQ(strumIndexFor(0x59, StrumLayout::Limited), -1);  // Keypad 1
    EXPECT_EQ(strumIndexFor(0x5C, StrumLayout::Limited), -1);  // Keypad 4
    EXPECT_EQ(strumIndexFor(0x5F, StrumLayout::Limited), -1);  // Keypad 7
}

TEST(Strum, NumpadPlusMinusNeverStrum) {
    EXPECT_EQ(strumIndexFor(0x57, StrumLayout::Full), -1);     // Keypad +
    EXPECT_EQ(strumIndexFor(0x56, StrumLayout::Full), -1);     // Keypad -
    EXPECT_EQ(strumIndexFor(0x57, StrumLayout::Limited), -1);  // Keypad +
    EXPECT_EQ(strumIndexFor(0x56, StrumLayout::Limited), -1);  // Keypad -
}

TEST(Strum, KeyCount) {
    EXPECT_EQ(strumKeyCount(StrumLayout::Full), 11);
    EXPECT_EQ(strumKeyCount(StrumLayout::Limited), 10);
}
