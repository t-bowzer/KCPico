#include <gtest/gtest.h>

#include "chords.h"
#include "keymap.h"


TEST(Combinations, SameColumnResolution) {
    ChordType t;

    EXPECT_TRUE(resolveSameColumn(true, false, false, t));
    EXPECT_EQ(t, ChordType::Major);

    EXPECT_TRUE(resolveSameColumn(false, true, false, t));
    EXPECT_EQ(t, ChordType::Minor);

    EXPECT_TRUE(resolveSameColumn(false, false, true, t));
    EXPECT_EQ(t, ChordType::Dom7);

    EXPECT_TRUE(resolveSameColumn(true, false, true, t));
    EXPECT_EQ(t, ChordType::Maj7);

    EXPECT_TRUE(resolveSameColumn(false, true, true, t));
    EXPECT_EQ(t, ChordType::Min7);

    EXPECT_TRUE(resolveSameColumn(true, true, false, t));
    EXPECT_EQ(t, ChordType::Dim);

    EXPECT_TRUE(resolveSameColumn(true, true, true, t));
    EXPECT_EQ(t, ChordType::Aug);

    EXPECT_FALSE(resolveSameColumn(false, false, false, t));
}

TEST(Combinations, ResolveChordSingleColumn) {
    // Column 5 = C. 'T' = Major col 5, 'B' = Seventh col 5, 'G' = Minor col 5.
    ResolvedChord out;

    // Major only
    ASSERT_TRUE(resolveChord({{ChordQuality::Major, 5}}, false, out));
    EXPECT_EQ(out.rootPc, 0);
    EXPECT_EQ(out.type, ChordType::Major);

    // Major + 7th -> Maj7
    ASSERT_TRUE(resolveChord({{ChordQuality::Major, 5}, {ChordQuality::Seventh, 5}}, false, out));
    EXPECT_EQ(out.type, ChordType::Maj7);

    // Major + Minor + 7th -> Augmented
    ASSERT_TRUE(resolveChord({{ChordQuality::Major, 5},
                              {ChordQuality::Minor, 5},
                              {ChordQuality::Seventh, 5}}, false, out));
    EXPECT_EQ(out.type, ChordType::Aug);
}

TEST(Combinations, LeftAdjacentSus4) {
    // Major col 5 (C) + Seventh col 4 (F column) -> sus4 on C.
    ResolvedChord out;
    ASSERT_TRUE(resolveChord({{ChordQuality::Major, 5}, {ChordQuality::Seventh, 4}}, false, out));
    EXPECT_EQ(out.rootPc, 0);
    EXPECT_EQ(out.type, ChordType::Sus4);
}

TEST(Combinations, LeftAdjacentAdd9) {
    // Major col 5 (C) + Minor col 4 -> add9 on C.
    ResolvedChord out;
    ASSERT_TRUE(resolveChord({{ChordQuality::Major, 5}, {ChordQuality::Minor, 4}}, false, out));
    EXPECT_EQ(out.rootPc, 0);
    EXPECT_EQ(out.type, ChordType::Major);
    EXPECT_TRUE(out.add9);
    EXPECT_FALSE(out.add11);
    EXPECT_FALSE(out.add13);
}

TEST(Combinations, LeftmostBacktickSus4) {
    // Tab (Major col 0, Db) + ` -> sus4 on Db.
    ResolvedChord out;
    ASSERT_TRUE(resolveChord({{ChordQuality::Major, 0}}, true, out));
    EXPECT_EQ(out.rootPc, 1);  // Db
    EXPECT_EQ(out.type, ChordType::Sus4);
}

TEST(Combinations, LeftmostBacktickAdd9) {
    // Caps (Minor col 0, Db) + ` -> add9 on Db.
    ResolvedChord out;
    ASSERT_TRUE(resolveChord({{ChordQuality::Minor, 0}}, true, out));
    EXPECT_EQ(out.rootPc, 1);  // Db
    EXPECT_EQ(out.type, ChordType::Major);
    EXPECT_TRUE(out.add9);
}

TEST(Combinations, EmptyHeldResolvesNothing) {
    ResolvedChord out;
    EXPECT_FALSE(resolveChord({}, false, out));
    EXPECT_FALSE(resolveChord({}, true, out));
}

TEST(Combinations, NonAdjacentColumnsResolveNothing) {
    ResolvedChord out;
    // Major col 5 + Seventh col 2 (not adjacent) -> no match.
    EXPECT_FALSE(resolveChord({{ChordQuality::Major, 5}, {ChordQuality::Seventh, 2}}, false, out));
}
