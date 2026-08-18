#include <gtest/gtest.h>

#include "voicing.h"


TEST(Voicing, RootPositionCMajor) {
    ResolvedChord c{0, ChordType::Major};
    auto v = voiceRootPosition(c, 60, 0, 48, 84);
    EXPECT_EQ(v, (std::vector<uint8_t>{60, 64, 67}));
}

TEST(Voicing, RootPositionFullRangeHigh) {
    ResolvedChord c{0, ChordType::Major};
    auto v = voiceRootPosition(c, 60, 3, 48, 84);  // +3 octaves -> C7
    EXPECT_EQ(v, (std::vector<uint8_t>{96, 100, 103}));
}

TEST(Voicing, RootPositionFullRangeLow) {
    ResolvedChord c{0, ChordType::Major};
    auto v = voiceRootPosition(c, 60, -3, 48, 84);  // -3 octaves -> C1
    EXPECT_EQ(v, (std::vector<uint8_t>{24, 28, 31}));
}

TEST(Voicing, RootPositionClampsToMidi) {
    ResolvedChord c{11, ChordType::Major};  // B major
    auto v = voiceRootPosition(c, 60, 3, 0, 127);
    for (uint8_t n : v) {
        EXPECT_GE(n, 0);
        EXPECT_LE(n, 127);
    }
}

TEST(Voicing, RootPositionOctaveClampsAtEdges) {
    ResolvedChord c{0, ChordType::Major};
    // Beyond the param range the root stays within [C1, C7], preserving pitch.
    auto high = voiceRootPosition(c, 60, 10, 0, 127);
    auto low  = voiceRootPosition(c, 60, -10, 0, 127);
    EXPECT_EQ(high, (std::vector<uint8_t>{96, 100, 103}));  // C7, not C14
    EXPECT_EQ(low,  (std::vector<uint8_t>{24, 28, 31}));    // C1, not C-6
}

TEST(Voicing, SmartFallsBackToRootPosition) {
    ResolvedChord c{0, ChordType::Major};
    auto v = voiceSmart(c, 60, 0, 48, 84, {});
    EXPECT_EQ(v, (std::vector<uint8_t>{60, 64, 67}));
}

TEST(Voicing, SmartVoiceLeadingMinimizesMovement) {
    ResolvedChord cmajor{0, ChordType::Major};  // C
    ResolvedChord fmajor{5, ChordType::Major};  // F

    std::vector<uint8_t> prev = {60, 64, 67};   // C4, E4, G4
    auto v = voiceSmart(fmajor, 60, 0, 48, 84, prev);

    // Nearest inversion of F major to C major root position is {60, 65, 69}.
    EXPECT_EQ(v, (std::vector<uint8_t>{60, 65, 69}));
}

TEST(Voicing, SmartDoesNotWalkDown) {
    ResolvedChord c{0, ChordType::Major};
    std::vector<uint8_t> prev = {60, 64, 67};
    // Many transitions must not cumulatively drift the voicing downward.
    int lowest = 60;
    for (int i = 0; i < 24; i++) {
        ResolvedChord next{static_cast<int>(i % 12), ChordType::Major};
        auto v = voiceSmart(next, 60, 0, 48, 84, prev);
        EXPECT_GE(v.front(), 48);  // stays within range
        EXPECT_LE(v.back(), 84);
        lowest = std::min(lowest, static_cast<int>(v.front()));
        prev = v;
    }
    EXPECT_GE(lowest, 48);  // never walked out the bottom of the range
}

TEST(Voicing, SmartProducesValidInversion) {
    ResolvedChord c{5, ChordType::Major};  // F major (F A C)
    std::vector<uint8_t> prev = {60, 64, 67};
    auto v = voiceSmart(c, 60, 0, 48, 84, prev);

    // All notes must be F, A, or C pitch classes.
    for (uint8_t n : v) {
        int pc = n % 12;
        EXPECT_TRUE(pc == 5 || pc == 9 || pc == 0);
    }
    for (size_t i = 1; i < v.size(); i++) {
        EXPECT_GT(v[i], v[i - 1]);  // ascending
    }
}

// VR-6: minimum note count pads with ascending octave notes.
TEST(Voicing, MinNotesPadsWithOctaveNotes) {
    ResolvedChord c{0, ChordType::Major};  // C major
    VoicingConfig cfg;
    cfg.min_notes = 4;
    auto v = voiceChord(c, 60, 0, 48, 84, cfg, {});
    EXPECT_EQ(v, (std::vector<uint8_t>{60, 64, 67, 72}));  // C E G C

    cfg.min_notes = 5;
    v = voiceChord(c, 60, 0, 48, 84, cfg, {});
    EXPECT_EQ(v, (std::vector<uint8_t>{60, 64, 67, 72, 76}));  // C E G C E
}

TEST(Voicing, MinNotesOnMaj7) {
    ResolvedChord c{0, ChordType::Maj7};  // C E G B
    VoicingConfig cfg;
    cfg.min_notes = 5;
    auto v = voiceChord(c, 60, 0, 48, 84, cfg, {});
    EXPECT_EQ(v, (std::vector<uint8_t>{60, 64, 67, 71, 72}));  // C E G B C
}

// VR-7: minimum interval spread — tightest ascending configuration.
TEST(Voicing, MinIntervalSpread) {
    ResolvedChord c{0, ChordType::Major};  // C major
    VoicingConfig cfg;
    cfg.min_interval = 5;
    auto v = voiceChord(c, 60, 0, 48, 84, cfg, {});
    EXPECT_EQ(v, (std::vector<uint8_t>{60, 67, 76}));  // C G E (tightest)

    cfg.min_interval = 4;
    v = voiceChord(c, 60, 0, 48, 84, cfg, {});
    EXPECT_EQ(v, (std::vector<uint8_t>{60, 67, 76}));  // C G E (not C E G)
}

// VR-8: manual inversions rotate the bass.
TEST(Voicing, ManualInversions) {
    ResolvedChord c{0, ChordType::Major};  // C major root {60,64,67}
    VoicingConfig cfg;

    cfg.inversion = InversionMode::First;
    EXPECT_EQ(voiceChord(c, 60, 0, 48, 84, cfg, {}),
              (std::vector<uint8_t>{64, 67, 72}));  // E G C

    cfg.inversion = InversionMode::Second;
    EXPECT_EQ(voiceChord(c, 60, 0, 48, 84, cfg, {}),
              (std::vector<uint8_t>{67, 72, 76}));  // G C E
}

