#include <gtest/gtest.h>

#include "chords.h"


static std::vector<uint8_t> notes(const ResolvedChord& c, int root) {
    return chordNotes(c, root);
}

TEST(Chords, IntervalFormulasExactNotes) {
    const int root = 60;  // C4

    EXPECT_EQ(notes({0, ChordType::Major}, root),  (std::vector<uint8_t>{60, 64, 67}));
    EXPECT_EQ(notes({0, ChordType::Minor}, root),  (std::vector<uint8_t>{60, 63, 67}));
    EXPECT_EQ(notes({0, ChordType::Dom7}, root),   (std::vector<uint8_t>{60, 64, 67, 70}));
    EXPECT_EQ(notes({0, ChordType::Maj7}, root),   (std::vector<uint8_t>{60, 64, 67, 71}));
    EXPECT_EQ(notes({0, ChordType::Min7}, root),   (std::vector<uint8_t>{60, 63, 67, 70}));
    EXPECT_EQ(notes({0, ChordType::Dim}, root),    (std::vector<uint8_t>{60, 63, 66}));
    EXPECT_EQ(notes({0, ChordType::Dim7}, root),   (std::vector<uint8_t>{60, 63, 66, 69}));
    EXPECT_EQ(notes({0, ChordType::Aug}, root),    (std::vector<uint8_t>{60, 64, 68}));
    EXPECT_EQ(notes({0, ChordType::Sus4}, root),   (std::vector<uint8_t>{60, 65, 67}));
    EXPECT_EQ(notes({0, ChordType::Sus2}, root),   (std::vector<uint8_t>{60, 62, 67}));
    EXPECT_EQ(notes({0, ChordType::Min7b5}, root), (std::vector<uint8_t>{60, 63, 66, 70}));
}

TEST(Chords, AllTwelveRootsAtBaseOctave) {
    // Circle-of-fifths order -> semitone offsets (spec 5.1).
    const int expectedPc[12] = {1, 8, 3, 10, 5, 0, 7, 2, 9, 4, 11, 6};
    const char* expectedName[12] = {
        "Db", "Ab", "Eb", "Bb", "F", "C", "G", "D", "A", "E", "B", "F#",
    };

    for (int col = 0; col < 12; col++) {
        EXPECT_EQ(rootPcForColumn(col), expectedPc[col]);
        EXPECT_STREQ(rootNameForColumn(col), expectedName[col]);
        EXPECT_EQ(rootMidi(rootPcForColumn(col), 60, 0), 60 + expectedPc[col]);
    }
}

TEST(Chords, RootMidiOctaveShift) {
    EXPECT_EQ(rootMidi(0, 60, 1), 72);    // C5
    EXPECT_EQ(rootMidi(0, 60, -1), 48);   // C3
    EXPECT_EQ(rootMidi(11, 60, 1), 83);   // B5
    EXPECT_EQ(rootMidi(1, 60, 2), 85);    // Db6
}

TEST(Chords, ExtensionsAddTensions) {
    const int root = 60;
    ResolvedChord c{0, ChordType::Major, false, false, false};

    EXPECT_EQ(chordNotes(c, root), (std::vector<uint8_t>{60, 64, 67}));

    c.add9 = true;   // +14
    EXPECT_EQ(chordNotes(c, root), (std::vector<uint8_t>{60, 64, 67, 74}));

    c.add11 = true;  // +17
    EXPECT_EQ(chordNotes(c, root), (std::vector<uint8_t>{60, 64, 67, 74, 77}));

    c.add13 = true;  // +21
    EXPECT_EQ(chordNotes(c, root), (std::vector<uint8_t>{60, 64, 67, 74, 77, 81}));
}

TEST(Chords, ChordNameFlatSpelling) {
    EXPECT_EQ(chordName({1, ChordType::Major}), "Db");
    EXPECT_EQ(chordName({1, ChordType::Minor}), "Dbm");
    EXPECT_EQ(chordName({1, ChordType::Dom7}), "Db7");
    EXPECT_EQ(chordName({1, ChordType::Maj7}), "Dbmaj7");
    EXPECT_EQ(chordName({1, ChordType::Min7}), "Dbm7");
    EXPECT_EQ(chordName({1, ChordType::Dim}), "Dbdim");
    EXPECT_EQ(chordName({1, ChordType::Aug}), "Dbaug");
    EXPECT_EQ(chordName({1, ChordType::Sus4}), "Dbsus4");
    EXPECT_EQ(chordName({3, ChordType::Major, true, false, false}), "Ebadd9");
}
