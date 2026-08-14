#include <gtest/gtest.h>

#include "chords.h"
#include "keymap.h"


TEST(Extensions, IndependentToggles) {
    const int root = 60;
    ResolvedChord c{0, ChordType::Major, false, false, false};

    EXPECT_EQ(chordNotes(c, root), (std::vector<uint8_t>{60, 64, 67}));

    c.add9 = true;
    EXPECT_EQ(chordNotes(c, root), (std::vector<uint8_t>{60, 64, 67, 74}));

    c = {0, ChordType::Major, false, true, false};
    EXPECT_EQ(chordNotes(c, root), (std::vector<uint8_t>{60, 64, 67, 77}));

    c = {0, ChordType::Major, false, false, true};
    EXPECT_EQ(chordNotes(c, root), (std::vector<uint8_t>{60, 64, 67, 81}));
}

TEST(Extensions, AllEightFlagCombinations) {
    const int root = 60;
    for (int mask = 0; mask < 8; mask++) {
        ResolvedChord c;
        c.rootPc = 0;
        c.type = ChordType::Major;
        c.add9 = mask & 1;
        c.add11 = mask & 2;
        c.add13 = mask & 4;

        auto v = chordNotes(c, root);
        EXPECT_EQ(v.size(), size_t(3 + (c.add9 ? 1 : 0) + (c.add11 ? 1 : 0) + (c.add13 ? 1 : 0)));

        if (c.add9)  EXPECT_EQ(v[3], 74);
        if (c.add11) EXPECT_EQ(v[3 + (c.add9 ? 1 : 0)], 77);
    }
}

TEST(Extensions, LeftAdjacentAdd9SetsFlag) {
    ResolvedChord out;
    ASSERT_TRUE(resolveChord({{ChordQuality::Major, 5}, {ChordQuality::Minor, 4}}, false, out));
    EXPECT_TRUE(out.add9);
    EXPECT_FALSE(out.add11);
    EXPECT_FALSE(out.add13);
}

TEST(Extensions, ExtensionsApplyToNonMajorChords) {
    ResolvedChord c{0, ChordType::Min7, true, false, false};
    auto v = chordNotes(c, 60);  // min7 = {60,63,67,70}, +add9 = 74
    EXPECT_EQ(v, (std::vector<uint8_t>{60, 63, 67, 70, 74}));
}
