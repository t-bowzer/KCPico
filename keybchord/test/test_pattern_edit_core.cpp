#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

#include "bass.h"
#include "chordpattern.h"
#include "pattern_limits.h"
#include "rhythm.h"

// --- Rhythm serializer -----------------------------------------------------

TEST(PatternSerialize, RhythmRoundTrip) {
    RhythmPattern p;
    p.name = "Test Groove";
    p.short_name = "TG";
    p.steps_per_bar = 12;
    p.swing = -25;
    p.tracks = {
        {36, "kick", {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
        {42, "hihat", {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0}},
    };

    const std::string compact = serializeRhythmPattern(p, false);
    EXPECT_EQ(compact.find('\n'), std::string::npos);

    RhythmPattern out;
    ASSERT_TRUE(parseRhythmPattern(compact, out));
    EXPECT_EQ(out.name, p.name);
    EXPECT_EQ(out.short_name, p.short_name);
    EXPECT_EQ(out.steps_per_bar, p.steps_per_bar);
    EXPECT_EQ(out.swing, p.swing);
    ASSERT_EQ(out.tracks.size(), 2u);
    EXPECT_EQ(out.tracks[0].note, 36);
    EXPECT_EQ(out.tracks[0].name, "kick");
    ASSERT_EQ(out.tracks[0].pattern.size(), 12u);
    EXPECT_EQ(out.tracks[0].pattern[0], 1);
    EXPECT_EQ(out.tracks[1].note, 42);

    const std::string pretty = serializeRhythmPattern(p, true);
    EXPECT_NE(pretty.find('\n'), std::string::npos);
    RhythmPattern prettyOut;
    ASSERT_TRUE(parseRhythmPattern(pretty, prettyOut));
    EXPECT_EQ(prettyOut.name, p.name);
    EXPECT_EQ(prettyOut.swing, p.swing);
}

TEST(PatternSerialize, RhythmOmitsEmptyShortName) {
    RhythmPattern p;
    p.name = "No Short";
    p.steps_per_bar = 4;
    p.tracks = {{36, "kick", {1, 0, 0, 0}}};

    const std::string json = serializeRhythmPattern(p, false);
    EXPECT_EQ(json.find("short_name"), std::string::npos);
    RhythmPattern out;
    ASSERT_TRUE(parseRhythmPattern(json, out));
    EXPECT_TRUE(out.short_name.empty());
}

// --- Bass serializer -------------------------------------------------------

TEST(PatternSerialize, BassRoundTrip) {
    BassPattern p;
    p.name = "Half Alt";
    p.steps_per_bar = 16;
    p.steps = {0, -1, -1, -1, -1, -1, -1, -1, 2, -1, -1, -1, -1, -1, -1, -1};
    p.sustain_steps = {8, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0};

    const std::string json = serializeBassPattern(p, false);
    EXPECT_EQ(json.find('\n'), std::string::npos);

    BassPattern out;
    ASSERT_TRUE(parseBassPattern(json, out));
    EXPECT_EQ(out.name, p.name);
    EXPECT_EQ(out.steps_per_bar, p.steps_per_bar);
    EXPECT_EQ(out.steps, p.steps);
    EXPECT_EQ(out.sustain_steps, p.sustain_steps);
    EXPECT_FALSE(out.hold);
}

TEST(PatternSerialize, BassOmitsEmptySustainAndKeepsHold) {
    BassPattern p;
    p.name = "Hold";
    p.steps_per_bar = 16;
    p.steps = {0};
    p.hold = true;

    const std::string json = serializeBassPattern(p, false);
    EXPECT_EQ(json.find("sustain_steps"), std::string::npos);
    EXPECT_NE(json.find("\"hold\":true"), std::string::npos);

    BassPattern out;
    ASSERT_TRUE(parseBassPattern(json, out));
    EXPECT_TRUE(out.hold);
    ASSERT_EQ(out.steps.size(), 1u);
    EXPECT_EQ(out.steps[0], 0);
}

// --- Chord pattern serializer ---------------------------------------------

TEST(PatternSerialize, ChordRoundTrip) {
    ChordPattern p;
    p.name = "Custom";
    p.steps_per_bar = 16;
    p.steps = {1, 0, 4, 0, 2, 0, 4, 0, 7, 0, 15, 0, 0, 0, 0, 0};
    p.sustain_steps = {16, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0};

    const std::string json = serializeChordPattern(p, false);
    EXPECT_EQ(json.find('\n'), std::string::npos);

    ChordPattern out;
    ASSERT_TRUE(parseChordPattern(json, out));
    EXPECT_EQ(out.name, p.name);
    EXPECT_EQ(out.steps_per_bar, p.steps_per_bar);
    EXPECT_EQ(out.steps, p.steps);
    EXPECT_EQ(out.sustain_steps, p.sustain_steps);
}

TEST(PatternSerialize, ChordOmitsEmptySustain) {
    ChordPattern p;
    p.name = "Alberti";
    p.steps_per_bar = 16;
    p.steps = {1, 0, 0, 0, 4, 0, 0, 0, 2, 0, 0, 0, 4, 0, 0, 0};

    const std::string json = serializeChordPattern(p, false);
    EXPECT_EQ(json.find("sustain_steps"), std::string::npos);

    ChordPattern out;
    ASSERT_TRUE(parseChordPattern(json, out));
    EXPECT_EQ(out.steps, p.steps);
}

// --- File caps -------------------------------------------------------------

TEST(PatternLimits, WorstCaseCompactRhythmFitsFileCap) {
    RhythmPattern p;
    p.name = "Worst";
    p.steps_per_bar = 16;
    for (uint8_t note = 36; note < 49; note++) {
        RhythmTrack t;
        t.note = note;
        t.name = "track";
        t.pattern.assign(8 * 16, 127);
        p.tracks.push_back(std::move(t));
    }

    const std::string json = serializeRhythmPattern(p, false);
    EXPECT_LE(json.size(), pattern_limits::RHYTHM_FILE_MAX_BYTES);
}

TEST(PatternLimits, WorstCaseCompactBassFitsFileCap) {
    BassPattern p;
    p.name = "Worst";
    p.steps_per_bar = 16;
    p.steps.assign(8 * 16, -1);

    const std::string json = serializeBassPattern(p, false);
    EXPECT_LE(json.size(), pattern_limits::BASS_FILE_MAX_BYTES);
}

TEST(PatternLimits, WorstCaseCompactChordFitsFileCap) {
    ChordPattern p;
    p.name = "Worst";
    p.steps_per_bar = 16;
    p.steps.assign(8 * 16, CHORD_MASK_SEVENTH);

    const std::string json = serializeChordPattern(p, false);
    EXPECT_LE(json.size(), pattern_limits::CHORD_FILE_MAX_BYTES);
}

TEST(PatternLimits, CountCap) {
    EXPECT_TRUE(pattern_limits::canAddPattern(0));
    EXPECT_TRUE(pattern_limits::canAddPattern(31));
    EXPECT_FALSE(pattern_limits::canAddPattern(32));
    EXPECT_FALSE(pattern_limits::canAddPattern(33));
}

TEST(PatternLimits, PerFileCaps) {
    using namespace pattern_limits;
    EXPECT_EQ(fileMaxBytes(Kind::Rhythm), 8u * 1024u);
    EXPECT_EQ(fileMaxBytes(Kind::Bass), 4u * 1024u);
    EXPECT_EQ(fileMaxBytes(Kind::Chord), 4u * 1024u);

    EXPECT_TRUE(fileSizeAllowed(Kind::Rhythm, RHYTHM_FILE_MAX_BYTES));
    EXPECT_FALSE(fileSizeAllowed(Kind::Rhythm, RHYTHM_FILE_MAX_BYTES + 1));
    EXPECT_TRUE(fileSizeAllowed(Kind::Bass, BASS_FILE_MAX_BYTES));
    EXPECT_FALSE(fileSizeAllowed(Kind::Bass, BASS_FILE_MAX_BYTES + 1));
    EXPECT_TRUE(fileSizeAllowed(Kind::Chord, CHORD_FILE_MAX_BYTES));
    EXPECT_FALSE(fileSizeAllowed(Kind::Chord, CHORD_FILE_MAX_BYTES + 1));
}

TEST(PatternLimits, BudgetCaps) {
    using namespace pattern_limits;
    EXPECT_TRUE(budgetAllowed(ALL_BUDGET_BYTES, USER_BUDGET_BYTES));
    EXPECT_FALSE(budgetAllowed(ALL_BUDGET_BYTES + 1, 0));
    EXPECT_FALSE(budgetAllowed(0, USER_BUDGET_BYTES + 1));
    EXPECT_TRUE(allBudgetAllowed(ALL_BUDGET_BYTES));
    EXPECT_FALSE(allBudgetAllowed(ALL_BUDGET_BYTES + 1));
    EXPECT_TRUE(userBudgetAllowed(USER_BUDGET_BYTES));
    EXPECT_FALSE(userBudgetAllowed(USER_BUDGET_BYTES + 1));
}
