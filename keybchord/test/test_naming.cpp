#include <gtest/gtest.h>

#include "naming.h"


TEST(Naming, PlayModeShortNames) {
    EXPECT_STREQ(playModeShort(PlayMode::Held), "Held");
    EXPECT_STREQ(playModeShort(PlayMode::PressToPlay), "Press");
    EXPECT_STREQ(playModeShort(PlayMode::Arpeggio), "Arp");
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
