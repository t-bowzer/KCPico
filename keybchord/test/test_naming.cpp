#include <gtest/gtest.h>

#include "naming.h"


TEST(Naming, PlayModeShortNames) {
    EXPECT_STREQ(playModeShort(PlayMode::Held), "Held");
    EXPECT_STREQ(playModeShort(PlayMode::PressToPlay), "Press");
    EXPECT_STREQ(playModeShort(PlayMode::Arpeggio), "Arp");
    EXPECT_STREQ(playModeShort(PlayMode::Rhythm), "Rhythm");
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
