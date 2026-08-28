#include <gtest/gtest.h>

#include "bass.h"


// Degree offsets per chord type (spec 6.8): root/3rd/5th/6th-or-7th.
TEST(Bass, DegreeOffsets) {
    EXPECT_EQ(bassDegreeOffset(ChordType::Major, 0), 0);
    EXPECT_EQ(bassDegreeOffset(ChordType::Major, 1), 4);
    EXPECT_EQ(bassDegreeOffset(ChordType::Major, 2), 7);
    EXPECT_EQ(bassDegreeOffset(ChordType::Major, 3), 9);

    EXPECT_EQ(bassDegreeOffset(ChordType::Minor, 1), 3);
    EXPECT_EQ(bassDegreeOffset(ChordType::Dom7, 3), 10);   // b7
    EXPECT_EQ(bassDegreeOffset(ChordType::Maj7, 3), 11);   // 7
    EXPECT_EQ(bassDegreeOffset(ChordType::Min7, 3), 10);
    EXPECT_EQ(bassDegreeOffset(ChordType::Dim, 2), 6);
    EXPECT_EQ(bassDegreeOffset(ChordType::Dim7, 3), 9);
    EXPECT_EQ(bassDegreeOffset(ChordType::Aug, 2), 8);
    EXPECT_EQ(bassDegreeOffset(ChordType::Sus4, 1), 5);
    EXPECT_EQ(bassDegreeOffset(ChordType::Sus2, 1), 2);
    EXPECT_EQ(bassDegreeOffset(ChordType::Min7b5, 2), 6);
    EXPECT_EQ(bassDegreeOffset(ChordType::Min7b5, 3), 10);
}

// Invalid degrees and chord types fall back cleanly.
TEST(Bass, DegreeOffsetBounds) {
    EXPECT_EQ(bassDegreeOffset(ChordType::Major, -1), -1);
    EXPECT_EQ(bassDegreeOffset(ChordType::Major, 4), -1);
    // Out-of-range chord type clamps to Major (degree 3 -> 9).
    EXPECT_EQ(bassDegreeOffset(static_cast<ChordType>(200), 3), 9);
}

TEST(Bass, ParsePattern) {
    const char* json = R"({
      "name": "Half Alt",
      "steps_per_bar": 16,
      "steps": [0,-1,-1,-1, -1,-1,-1,-1, 2,-1,-1,-1, -1,-1,-1,-1],
      "sustain_steps": [8,0,0,0, 0,0,0,0, 8,0,0,0, 0,0,0,0]
    })";

    BassPattern p;
    ASSERT_TRUE(parseBassPattern(json, p));
    EXPECT_EQ(p.name, "Half Alt");
    EXPECT_EQ(p.steps_per_bar, 16);
    EXPECT_FALSE(p.hold);
    ASSERT_EQ(p.steps.size(), 16u);
    EXPECT_EQ(p.steps[0], 0);
    EXPECT_EQ(p.steps[1], -1);
    EXPECT_EQ(p.steps[8], 2);
    ASSERT_EQ(p.sustain_steps.size(), 16u);
    EXPECT_EQ(p.sustain_steps[0], 8);
    EXPECT_EQ(p.sustain_steps[8], 8);
    EXPECT_EQ(p.beatsPerBar(), 4);
}

TEST(Bass, ParsePatternDefaultsSustainAndClampsDegrees) {
    const char* json = R"({
      "name": "Custom",
      "steps_per_bar": 4,
      "steps": [0, 5, -9, 2]
    })";
    BassPattern p;
    ASSERT_TRUE(parseBassPattern(json, p));
    // Degrees clamp to -1..3.
    EXPECT_EQ(p.steps[0], 0);
    EXPECT_EQ(p.steps[1], 3);   // 5 clamped to 3
    EXPECT_EQ(p.steps[2], -1);  // -9 clamped to -1
    EXPECT_EQ(p.steps[3], 2);
    // No sustain array -> default percussive.
    EXPECT_TRUE(p.sustain_steps.empty());
}

TEST(Bass, ParseHoldFlag) {
    const char* json = R"({ "name": "Hold", "steps_per_bar": 16, "hold": true, "steps": [0] })";
    BassPattern p;
    ASSERT_TRUE(parseBassPattern(json, p));
    EXPECT_TRUE(p.hold);
    EXPECT_EQ(p.steps.size(), 1u);
}

TEST(Bass, ParseRejectsMalformed) {
    BassPattern p;
    EXPECT_FALSE(parseBassPattern("not json", p));
    EXPECT_FALSE(parseBassPattern("[1,2,3]", p));  // not an object
    EXPECT_FALSE(parseBassPattern(R"({ "name": "Empty" })", p));  // no steps
}

TEST(Bass, StepOffsetResolvesDegreeAndRest) {
    BassPattern p;
    p.name = "Walking";
    p.steps_per_bar = 16;
    p.steps = {0,-1,-1,-1, 1,-1,-1,-1, 2,-1,-1,-1, 3,-1,-1,-1};

    EXPECT_EQ(bassStepOffset(p, ChordType::Major, 0), 0);   // root
    EXPECT_EQ(bassStepOffset(p, ChordType::Major, 1), -1);  // rest
    EXPECT_EQ(bassStepOffset(p, ChordType::Major, 4), 4);   // major 3rd
    EXPECT_EQ(bassStepOffset(p, ChordType::Minor, 4), 3);   // minor 3rd
    EXPECT_EQ(bassStepOffset(p, ChordType::Major, 8), 7);   // 5th
    EXPECT_EQ(bassStepOffset(p, ChordType::Major, 12), 9);  // 6th
    EXPECT_EQ(bassStepOffset(p, ChordType::Dom7, 12), 10);  // b7
    EXPECT_EQ(bassStepOffset(p, ChordType::Major, 16), -1); // out of range
}

TEST(Bass, HoldNeverFiresFromSteps) {
    BassPattern p;
    p.name = "Hold";
    p.hold = true;
    p.steps = {0};
    EXPECT_EQ(bassStepOffset(p, ChordType::Major, 0), -1);
}

TEST(Bass, StepSustain) {
    BassPattern p;
    p.steps = {0, 2};
    p.sustain_steps = {8, 0};
    EXPECT_EQ(bassStepSustain(p, 0), 8);
    EXPECT_EQ(bassStepSustain(p, 1), 0);   // percussive
    EXPECT_EQ(bassStepSustain(p, 2), 0);   // out of range
}

TEST(Bass, NameIndexRoundTrip) {
    EXPECT_EQ(bassIndex("Walking"), 0);
    EXPECT_EQ(bassIndex("No 6th"), 8);
    EXPECT_EQ(bassIndex("Nope"), -1);
    EXPECT_STREQ(bassName(0), "Walking");
    EXPECT_STREQ(bassName(8), "No 6th");
    EXPECT_STREQ(bassName(-1), "Walking");  // clamp
    EXPECT_EQ(bassCount(), BASS_COUNT);
}

TEST(Bass, FileNameMapping) {
    EXPECT_STREQ(bassFileName(0), "walking.json");
    EXPECT_STREQ(bassFileName(8), "walk_no_6th.json");
}

TEST(Bass, NameRegistryOverridesAndResets) {
    std::vector<std::string> names;
    for (int i = 0; i < BASS_COUNT; i++) names.push_back(bassName(i));
    names.push_back("My Bass");
    installBassNames(names);

    EXPECT_EQ(bassCount(), BASS_COUNT + 1);
    EXPECT_STREQ(bassName(BASS_COUNT), "My Bass");
    EXPECT_EQ(bassIndex("My Bass"), BASS_COUNT);
    EXPECT_EQ(bassIndex("Walking"), 0);

    clearBassNames();
    EXPECT_EQ(bassCount(), BASS_COUNT);
    EXPECT_EQ(bassIndex("My Bass"), -1);
    EXPECT_STREQ(bassName(BASS_COUNT), "Walking");  // clamp back to built-in
}
