#include <gtest/gtest.h>

#include "chordpattern.h"


// Degree offsets per chord type (spec 6.10): root/3rd/5th/7th, where degree 3
// (bit 3) is the 7th when present, else the root an octave up (12).
TEST(ChordPattern, OffsetsMajor) {
    // C major triad: 7th wraps to the octave root.
    EXPECT_EQ(chordPatternOffsets(ChordType::Major, CHORD_DEGREE_ROOT),
              (std::vector<int>{0}));
    EXPECT_EQ(chordPatternOffsets(ChordType::Major, CHORD_DEGREE_THIRD),
              (std::vector<int>{4}));
    EXPECT_EQ(chordPatternOffsets(ChordType::Major, CHORD_DEGREE_FIFTH),
              (std::vector<int>{7}));
    EXPECT_EQ(chordPatternOffsets(ChordType::Major, CHORD_DEGREE_SEVENTH),
              (std::vector<int>{12}));  // wrap to octave root on a triad
}

TEST(ChordPattern, OffsetsSeventhChord) {
    // Dom7 has a real 7th (b7 = 10), so bit 3 does not wrap.
    EXPECT_EQ(chordPatternOffsets(ChordType::Dom7, CHORD_DEGREE_SEVENTH),
              (std::vector<int>{10}));
    EXPECT_EQ(chordPatternOffsets(ChordType::Maj7, CHORD_DEGREE_SEVENTH),
              (std::vector<int>{11}));
    EXPECT_EQ(chordPatternOffsets(ChordType::Min7, CHORD_DEGREE_SEVENTH),
              (std::vector<int>{10}));
    EXPECT_EQ(chordPatternOffsets(ChordType::Dim7, CHORD_DEGREE_SEVENTH),
              (std::vector<int>{9}));
}

TEST(ChordPattern, OffsetsMultiNoteAscending) {
    // Root + 3rd + 5th resolves to ascending offsets.
    EXPECT_EQ(chordPatternOffsets(ChordType::Major, CHORD_MASK_TRIAD),
              (std::vector<int>{0, 4, 7}));
    // Full seventh mask on a Major triad: root, 3rd, 5th, octave root.
    EXPECT_EQ(chordPatternOffsets(ChordType::Major, CHORD_MASK_SEVENTH),
              (std::vector<int>{0, 4, 7, 12}));
}

TEST(ChordPattern, OffsetsEmptyMask) {
    EXPECT_TRUE(chordPatternOffsets(ChordType::Major, 0x00).empty());
}

TEST(ChordPattern, OffsetsInvalidTypeClampsToMajor) {
    EXPECT_EQ(chordPatternOffsets(static_cast<ChordType>(200), CHORD_DEGREE_SEVENTH),
              (std::vector<int>{12}));
}

TEST(ChordPattern, ParsePattern) {
    const char* json = R"({
      "name": "Alberti",
      "steps_per_bar": 16,
      "steps": [1,0,0,0, 4,0,0,0, 2,0,0,0, 4,0,0,0]
    })";

    ChordPattern p;
    ASSERT_TRUE(parseChordPattern(json, p));
    EXPECT_EQ(p.name, "Alberti");
    EXPECT_EQ(p.steps_per_bar, 16);
    ASSERT_EQ(p.steps.size(), 16u);
    EXPECT_EQ(p.steps[0], 1);
    EXPECT_EQ(p.steps[4], 4);
    EXPECT_EQ(p.steps[8], 2);
    EXPECT_EQ(p.steps[12], 4);
    EXPECT_TRUE(p.sustain_steps.empty());
    EXPECT_EQ(p.beatsPerBar(), 4);
}

TEST(ChordPattern, ParsePatternClampsMaskAndSustain) {
    const char* json = R"({
      "name": "Custom",
      "steps_per_bar": 4,
      "steps": [1, 99, -3, 7],
      "sustain_steps": [0, 8, 200, 1]
    })";
    ChordPattern p;
    ASSERT_TRUE(parseChordPattern(json, p));
    EXPECT_EQ(p.steps[0], 1);
    EXPECT_EQ(p.steps[1], CHORD_MASK_SEVENTH);   // 99 clamped to 0x0F
    EXPECT_EQ(p.steps[2], 0);                    // -3 clamped to 0
    EXPECT_EQ(p.steps[3], 7);
    EXPECT_EQ(p.sustain_steps[1], 8);
    EXPECT_EQ(p.sustain_steps[2], 64);           // 200 clamped to 64
}

TEST(ChordPattern, ParseRejectsMalformed) {
    ChordPattern p;
    EXPECT_FALSE(parseChordPattern("not json", p));
    EXPECT_FALSE(parseChordPattern("[1,2,3]", p));
    EXPECT_FALSE(parseChordPattern(R"({ "name": "Empty" })", p));
}

TEST(ChordPattern, StepSustain) {
    ChordPattern p;
    p.steps = {1, 4};
    p.sustain_steps = {16, 0};
    EXPECT_EQ(chordPatternStepSustain(p, 0), 16);
    EXPECT_EQ(chordPatternStepSustain(p, 1), 0);
    EXPECT_EQ(chordPatternStepSustain(p, 2), 0);
}

TEST(ChordPattern, NameIndexRoundTrip) {
    EXPECT_EQ(chordPatternIndex("Block"), 0);
    EXPECT_EQ(chordPatternIndex("Alberti"), 1);
    EXPECT_EQ(chordPatternIndex("Nope"), -1);
    EXPECT_STREQ(chordPatternName(0), "Block");
    EXPECT_STREQ(chordPatternName(1), "Alberti");
    EXPECT_STREQ(chordPatternName(-1), "Block");
    EXPECT_EQ(chordPatternCount(), CHORD_PATTERN_COUNT);
}

TEST(ChordPattern, FileNameMapping) {
    EXPECT_STREQ(chordPatternFileName(0), "block.json");
    EXPECT_STREQ(chordPatternFileName(1), "alberti.json");
}

TEST(ChordPattern, NameRegistryOverridesAndResets) {
    std::vector<std::string> names;
    for (int i = 0; i < CHORD_PATTERN_COUNT; i++) names.push_back(chordPatternName(i));
    names.push_back("My Pattern");
    installChordPatternNames(names);

    EXPECT_EQ(chordPatternCount(), CHORD_PATTERN_COUNT + 1);
    EXPECT_STREQ(chordPatternName(CHORD_PATTERN_COUNT), "My Pattern");
    EXPECT_EQ(chordPatternIndex("My Pattern"), CHORD_PATTERN_COUNT);
    EXPECT_EQ(chordPatternIndex("Block"), 0);

    clearChordPatternNames();
    EXPECT_EQ(chordPatternCount(), CHORD_PATTERN_COUNT);
    EXPECT_EQ(chordPatternIndex("My Pattern"), -1);
}
