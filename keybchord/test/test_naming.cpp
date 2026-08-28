#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "naming.h"
#include "rhythm.h"


TEST(Naming, PlayModeShortNames) {
    EXPECT_STREQ(playModeShort(PlayMode::Held), "Held");
    EXPECT_STREQ(playModeShort(PlayMode::PressToPlay), "Press");
    EXPECT_STREQ(playModeShort(PlayMode::Arpeggio), "Arp");
    EXPECT_STREQ(playModeShort(PlayMode::ArpHold), "ArpHld");
    EXPECT_STREQ(playModeShort(PlayMode::Silent), "Silent");
}

TEST(Naming, VoicingModeNames) {
    EXPECT_STREQ(voicingModeName(VoicingMode::RootPosition), "Root");
    EXPECT_STREQ(voicingModeName(VoicingMode::Smart), "Smart");
}

TEST(Naming, RhythmShortCodes) {
    const char* expected[12] = {
        "Rk", "R2", "Wz", "Sw", "SR", "BN",
        "Rb", "Tg", "Mr", "Sb", "Ds", "Fx",
    };
    for (int i = 0; i < 12; i++) {
        EXPECT_STREQ(rhythmShortCode(i), expected[i]);
    }
}

TEST(Naming, RhythmShortCodeOutOfRangeFallsBackToFirst) {
    EXPECT_STREQ(rhythmShortCode(-1), "Rk");
    EXPECT_STREQ(rhythmShortCode(12), "Rk");
    EXPECT_STREQ(rhythmShortCode(999), "Rk");
}

TEST(Naming, RhythmShortCodeDerivesFromUserName) {
    std::vector<std::string> names;
    for (int i = 0; i < RHYTHM_COUNT; i++) names.push_back(rhythmName(i));
    names.push_back("Test Groove");
    names.push_back("funk");
    installRhythmNames(names);

    EXPECT_STREQ(rhythmShortCode(RHYTHM_COUNT), "TE");
    EXPECT_STREQ(rhythmShortCode(RHYTHM_COUNT + 1), "FU");

    clearRhythmNames();
    EXPECT_STREQ(rhythmShortCode(RHYTHM_COUNT), "Rk");  // back to built-in fallback
}

TEST(Naming, RhythmShortCodeUsesAuthoredShortName) {
    std::vector<std::string> names;
    std::vector<std::string> shorts;
    for (int i = 0; i < RHYTHM_COUNT; i++) {
        names.push_back(rhythmName(i));
        shorts.push_back("");
    }
    shorts[0] = "R1";               // overrides the built-in "Rk"
    names.push_back("My Funky Groove");
    shorts.push_back("FG");
    names.push_back("Long Name Here");
    shorts.push_back("TooLong");    // truncated to "To"
    installRhythmNames(names);
    installRhythmShortNames(shorts);

    EXPECT_STREQ(rhythmShortCode(0), "R1");
    EXPECT_STREQ(rhythmShortCode(RHYTHM_COUNT), "FG");
    EXPECT_STREQ(rhythmShortCode(RHYTHM_COUNT + 1), "To");

    clearRhythmNames();
    clearRhythmShortNames();
    EXPECT_STREQ(rhythmShortCode(0), "Rk");  // built-in code restored
}

TEST(Naming, NoteNameFlatSpellingAndOctave) {
    EXPECT_EQ(noteName(60), "C4");
    EXPECT_EQ(noteName(61), "Db4");
    EXPECT_EQ(noteName(66), "F#4");
    EXPECT_EQ(noteName(56), "Ab3");
    EXPECT_EQ(noteName(0), "C-1");
    EXPECT_EQ(noteName(127), "G9");
}

TEST(Naming, CcNameKnownAndFallback) {
    EXPECT_EQ(ccName(7), "Volume");
    EXPECT_EQ(ccName(120), "AllSoundOff");
    EXPECT_EQ(ccName(123), "AllNotesOff");
    EXPECT_EQ(ccName(64), "Sustain");
    EXPECT_EQ(ccName(3), "CC3");
}

TEST(Naming, MessageTypeName) {
    EXPECT_STREQ(messageTypeName(0x90), "NoteOn");
    EXPECT_STREQ(messageTypeName(0x80), "NoteOff");
    EXPECT_STREQ(messageTypeName(0xB0), "CC");
    EXPECT_STREQ(messageTypeName(0xC0), "ProgramChange");
    EXPECT_STREQ(messageTypeName(0xF8), "Clock");
    EXPECT_STREQ(messageTypeName(0xFA), "Start");
    EXPECT_STREQ(messageTypeName(0xFC), "Stop");
}
