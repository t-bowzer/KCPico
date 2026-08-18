#include <gtest/gtest.h>

#include "bass.h"


// Blueprint offsets per spec 6.8 (4/4).
TEST(Bass, BlueprintOffsets) {
    EXPECT_EQ(bassOffsetForBeat(ChordType::Major,   0, 4), 0);
    EXPECT_EQ(bassOffsetForBeat(ChordType::Major,   1, 4), 4);
    EXPECT_EQ(bassOffsetForBeat(ChordType::Major,   2, 4), 7);
    EXPECT_EQ(bassOffsetForBeat(ChordType::Major,   3, 4), 9);

    EXPECT_EQ(bassOffsetForBeat(ChordType::Minor,   1, 4), 3);
    EXPECT_EQ(bassOffsetForBeat(ChordType::Dom7,    3, 4), 10);  // b7
    EXPECT_EQ(bassOffsetForBeat(ChordType::Maj7,    3, 4), 11);  // 7
    EXPECT_EQ(bassOffsetForBeat(ChordType::Min7,    3, 4), 10);
    EXPECT_EQ(bassOffsetForBeat(ChordType::Dim,     2, 4), 6);
    EXPECT_EQ(bassOffsetForBeat(ChordType::Dim7,    3, 4), 9);
    EXPECT_EQ(bassOffsetForBeat(ChordType::Aug,     2, 4), 8);
    EXPECT_EQ(bassOffsetForBeat(ChordType::Sus4,    1, 4), 5);
    EXPECT_EQ(bassOffsetForBeat(ChordType::Sus2,    1, 4), 2);
    EXPECT_EQ(bassOffsetForBeat(ChordType::Min7b5,  2, 4), 6);
    EXPECT_EQ(bassOffsetForBeat(ChordType::Min7b5,  3, 4), 10);
}

// 3/4 waltz cycles only the first three offsets (root-3rd-5th).
TEST(Bass, WaltzUsesThreeBeats) {
    EXPECT_EQ(bassOffsetForBeat(ChordType::Major, 0, 3), 0);
    EXPECT_EQ(bassOffsetForBeat(ChordType::Major, 1, 3), 4);
    EXPECT_EQ(bassOffsetForBeat(ChordType::Major, 2, 3), 7);
}

// bassNote = root (octave-transposed) + offset.
TEST(Bass, NoteMath) {
    // C major, root pc 0, base 60, bass octave -1 -> C3 = 48; beat 1 -> +4 = 52.
    EXPECT_EQ(bassNote(ChordType::Major, 0, 60, -1, 1, 4), 52);

    // Dom7, beat 4 -> +10.
    EXPECT_EQ(bassNote(ChordType::Dom7, 0, 60, -1, 3, 4), 58);

    // Beat clamped to the meter.
    EXPECT_EQ(bassOffsetForBeat(ChordType::Major, 5, 4), 9);
    EXPECT_EQ(bassOffsetForBeat(ChordType::Major, -1, 4), 0);
}
