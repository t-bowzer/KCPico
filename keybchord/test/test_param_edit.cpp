#include <gtest/gtest.h>

#include "param_edit.h"
#include "state.h"


TEST(ParamEdit, MenuTitles) {
    EXPECT_STREQ(menuTitle(EditMenu::Chord), "Chord Edit");
    EXPECT_STREQ(menuTitle(EditMenu::Strum), "Strum Edit");
    EXPECT_STREQ(menuTitle(EditMenu::Rhythm), "Rhythm Edit");
    EXPECT_STREQ(menuTitle(EditMenu::Bass), "Bass Edit");
    EXPECT_STREQ(menuTitle(EditMenu::Drum), "Drum Edit");
    EXPECT_STREQ(menuTitle(EditMenu::None), "");
}

TEST(ParamEdit, MenuParamLists) {
    EXPECT_EQ(menuParamCount(EditMenu::Chord), 11);
    EXPECT_EQ(menuParamCount(EditMenu::Strum), 7);
    EXPECT_EQ(menuParamCount(EditMenu::Rhythm), 7);
    EXPECT_EQ(menuParamCount(EditMenu::Bass), 6);
    EXPECT_EQ(menuParamCount(EditMenu::Drum), 26);

    EXPECT_EQ(menuParamAt(EditMenu::Chord, 0), ParamId::ChordOctave);
    EXPECT_EQ(menuParamAt(EditMenu::Chord, 1), ParamId::ChordMode);
    EXPECT_EQ(menuParamAt(EditMenu::Chord, 6), ParamId::ChordRoll);
    EXPECT_EQ(menuParamAt(EditMenu::Chord, 10), ParamId::ChordArpMode);

    EXPECT_EQ(menuParamAt(EditMenu::Strum, 0), ParamId::StrumOctave);
    EXPECT_EQ(menuParamAt(EditMenu::Strum, 4), ParamId::StrumMode);

    EXPECT_EQ(menuParamAt(EditMenu::Rhythm, 0), ParamId::RhythmTempo);
    EXPECT_EQ(menuParamAt(EditMenu::Rhythm, 6), ParamId::RhythmLed);

    EXPECT_EQ(menuParamAt(EditMenu::Bass, 0), ParamId::BassEnable);

    // Out of range -> COUNT.
    EXPECT_EQ(menuParamAt(EditMenu::Strum, 7), ParamId::COUNT);
    EXPECT_EQ(menuParamAt(EditMenu::None, 0), ParamId::COUNT);
}

TEST(ParamEdit, Names) {
    EXPECT_STREQ(paramShortName(ParamId::ChordOctave), "Octave");
    EXPECT_STREQ(paramFullName(ParamId::ChordOctave), "Chord Octave");
    EXPECT_STREQ(paramFullName(ParamId::StrumOctave), "Strum Octave");
    EXPECT_STREQ(paramFullName(ParamId::RhythmTempo), "Rhythm Tempo");
    EXPECT_STREQ(paramShortName(ParamId::RhythmMute), "Mute");
    EXPECT_STREQ(paramFullName(ParamId::RhythmMute), "Rhythm Mute");
    EXPECT_STREQ(paramFullName(ParamId::StrumLayout), "Strum Layout");
    EXPECT_STREQ(paramShortName(ParamId::ChordRoll), "Roll");
    EXPECT_STREQ(paramShortName(ParamId::ChordInversion), "Inversion");
}

TEST(ParamEdit, ValueStrings) {
    StateManager s;
    EXPECT_EQ(paramValueString(s, ParamId::ChordOctave), "0");
    EXPECT_EQ(paramValueString(s, ParamId::ChordMode), "Held");
    EXPECT_EQ(paramValueString(s, ParamId::ChordVoicing), "Root");
    EXPECT_EQ(paramValueString(s, ParamId::ChordDuration), "500");
    EXPECT_EQ(paramValueString(s, ParamId::ChordVelocity), "100");
    EXPECT_EQ(paramValueString(s, ParamId::ChordPan), "64");
    EXPECT_EQ(paramValueString(s, ParamId::ChordRoll), "0");
    EXPECT_EQ(paramValueString(s, ParamId::ChordMinNotes), "3");
    EXPECT_EQ(paramValueString(s, ParamId::ChordMinInterval), "Off");
    EXPECT_EQ(paramValueString(s, ParamId::ChordInversion), "Root");
    EXPECT_EQ(paramValueString(s, ParamId::ChordArpMode), "Up");
    EXPECT_EQ(paramValueString(s, ParamId::StrumOctave), "+1");
    EXPECT_EQ(paramValueString(s, ParamId::StrumDuration), "300");
    EXPECT_EQ(paramValueString(s, ParamId::StrumLayout), "Full");
    EXPECT_EQ(paramValueString(s, ParamId::StrumMode), "Chord");
    EXPECT_EQ(paramValueString(s, ParamId::RhythmTempo), "120");
    EXPECT_EQ(paramValueString(s, ParamId::RhythmSwing), "0");
    EXPECT_EQ(paramValueString(s, ParamId::RhythmPattern), "Rock 1");
    EXPECT_EQ(paramValueString(s, ParamId::RhythmMute), "Off");
    EXPECT_EQ(paramValueString(s, ParamId::RhythmClock), "Off");
    EXPECT_EQ(paramValueString(s, ParamId::BassEnable), "Off");
    EXPECT_EQ(paramValueString(s, ParamId::BassOctave), "-1");
    EXPECT_EQ(paramValueString(s, ParamId::DrumKickVel), "Auto");
}

TEST(ParamEdit, StepIntClamps) {
    StateManager s;

    paramStep(s, ParamId::ChordOctave, +1);
    EXPECT_EQ(s.pendingChord.octave, 1);
    s.pendingChord.octave = 3;
    paramStep(s, ParamId::ChordOctave, +1);
    EXPECT_EQ(s.pendingChord.octave, 3);
    paramStep(s, ParamId::ChordOctave, -1);
    EXPECT_EQ(s.pendingChord.octave, 2);

    paramStep(s, ParamId::RhythmTempo, +1);
    EXPECT_EQ(s.pendingRhythm.tempo, 121);
    s.pendingRhythm.tempo = 40;
    paramStep(s, ParamId::RhythmTempo, -1);
    EXPECT_EQ(s.pendingRhythm.tempo, 40);

    paramStep(s, ParamId::RhythmSwing, +1);
    EXPECT_EQ(s.pendingRhythm.swing, 5);
    paramStep(s, ParamId::RhythmSwing, -1);
    EXPECT_EQ(s.pendingRhythm.swing, 0);

    paramStep(s, ParamId::ChordDuration, +1);
    EXPECT_EQ(s.pendingChord.note_duration_ms, 550);

    // Chord roll steps by 10.
    paramStep(s, ParamId::ChordRoll, +1);
    EXPECT_EQ(s.pendingChord.chord_roll_ms, 10);
    paramStep(s, ParamId::ChordRoll, -1);
    EXPECT_EQ(s.pendingChord.chord_roll_ms, 0);

    // Min notes / min interval step by 1.
    paramStep(s, ParamId::ChordMinNotes, +1);
    EXPECT_EQ(s.pendingChord.min_notes, 4);
    paramStep(s, ParamId::ChordMinInterval, +1);
    EXPECT_EQ(s.pendingChord.min_interval, 1);
}

TEST(ParamEdit, StepEnumCyclesBySign) {
    StateManager s;

    paramStep(s, ParamId::ChordMode, +1);
    EXPECT_EQ(s.pendingChord.play_mode, PlayMode::PressToPlay);
    paramStep(s, ParamId::ChordMode, -1);
    EXPECT_EQ(s.pendingChord.play_mode, PlayMode::Held);

    paramStep(s, ParamId::ChordVoicing, +1);
    EXPECT_EQ(s.pendingChord.voicing_mode, VoicingMode::Smart);
    paramStep(s, ParamId::ChordVoicing, -1);
    EXPECT_EQ(s.pendingChord.voicing_mode, VoicingMode::RootPosition);

    paramStep(s, ParamId::ChordInversion, +1);
    EXPECT_EQ(s.pendingChord.inversion, InversionMode::First);
    paramStep(s, ParamId::ChordInversion, -1);
    EXPECT_EQ(s.pendingChord.inversion, InversionMode::Root);

    paramStep(s, ParamId::ChordArpMode, +1);
    EXPECT_EQ(s.pendingChord.arp_mode, ArpMode::Down);
    paramStep(s, ParamId::StrumMode, +1);
    EXPECT_EQ(s.pendingStrum.mode, StrumMode::Scale);
}

TEST(ParamEdit, StepBoolSetBySign) {
    StateManager s;
    paramStep(s, ParamId::RhythmMute, +1);
    EXPECT_TRUE(s.pendingRhythm.muted);

    paramStep(s, ParamId::RhythmClock, +1);
    EXPECT_TRUE(s.config.midi_clock_enabled);
    paramStep(s, ParamId::RhythmClock, -1);
    EXPECT_FALSE(s.config.midi_clock_enabled);

    paramStep(s, ParamId::BassEnable, +1);
    EXPECT_TRUE(s.pendingBass.enabled);
}

TEST(ParamEdit, StepPatternWraps) {
    StateManager s;
    s.pendingRhythm.pattern = 11;
    paramStep(s, ParamId::RhythmPattern, +1);
    EXPECT_EQ(s.pendingRhythm.pattern, 0);
    paramStep(s, ParamId::RhythmPattern, -1);
    EXPECT_EQ(s.pendingRhythm.pattern, 11);
}

TEST(ParamEdit, DrumVelocityStepsThroughAutoOff) {
    StateManager s;
    // Auto (0) -> Off (128) when stepping down.
    paramStep(s, ParamId::DrumKickVel, -1);
    EXPECT_EQ(s.pendingRhythm.drums.kick_vel, 128);
    paramStep(s, ParamId::DrumKickVel, -1);
    EXPECT_EQ(s.pendingRhythm.drums.kick_vel, 127);

    // Off (128) -> Auto (0) when stepping up.
    s.pendingRhythm.drums.kick_vel = 128;
    paramStep(s, ParamId::DrumKickVel, +1);
    EXPECT_EQ(s.pendingRhythm.drums.kick_vel, 0);

    // Auto (0) -> 1 when stepping up.
    paramStep(s, ParamId::DrumKickVel, +1);
    EXPECT_EQ(s.pendingRhythm.drums.kick_vel, 1);
}

TEST(ParamEdit, DrumVelocityStrings) {
    StateManager s;
    EXPECT_EQ(paramValueString(s, ParamId::DrumKickVel), "Auto");
    s.pendingRhythm.drums.kick_vel = 128;
    EXPECT_EQ(paramValueString(s, ParamId::DrumKickVel), "Off");
    s.pendingRhythm.drums.kick_vel = 100;
    EXPECT_EQ(paramValueString(s, ParamId::DrumKickVel), "100");
}

TEST(ParamEdit, CycleToggles) {
    StateManager s;

    paramCycle(s, ParamId::ChordMode);
    EXPECT_EQ(s.pendingChord.play_mode, PlayMode::PressToPlay);

    paramCycle(s, ParamId::ChordVoicing);
    EXPECT_EQ(s.pendingChord.voicing_mode, VoicingMode::Smart);
    paramCycle(s, ParamId::ChordVoicing);
    EXPECT_EQ(s.pendingChord.voicing_mode, VoicingMode::RootPosition);

    paramCycle(s, ParamId::StrumLayout);
    EXPECT_TRUE(s.pendingStrum.limited_keys);

    paramCycle(s, ParamId::BassEnable);
    EXPECT_TRUE(s.pendingBass.enabled);

    paramCycle(s, ParamId::RhythmEnable);
    EXPECT_TRUE(s.pendingRhythm.enabled);
    paramCycle(s, ParamId::RhythmEnable);
    EXPECT_FALSE(s.pendingRhythm.enabled);

    paramCycle(s, ParamId::RhythmPattern);
    EXPECT_EQ(s.pendingRhythm.pattern, 1);

    paramCycle(s, ParamId::RhythmLed);
    EXPECT_FALSE(s.config.bpm_indicator);
}

TEST(ParamEdit, AutoRepeatableParams) {
    EXPECT_TRUE(isAutoRepeatable(ParamId::ChordDuration));
    EXPECT_TRUE(isAutoRepeatable(ParamId::ChordVelocity));
    EXPECT_TRUE(isAutoRepeatable(ParamId::ChordPan));
    EXPECT_TRUE(isAutoRepeatable(ParamId::ChordRoll));
    EXPECT_TRUE(isAutoRepeatable(ParamId::StrumDuration));
    EXPECT_TRUE(isAutoRepeatable(ParamId::StrumVelocity));
    EXPECT_TRUE(isAutoRepeatable(ParamId::RhythmTempo));
    EXPECT_TRUE(isAutoRepeatable(ParamId::RhythmSwing));
    EXPECT_TRUE(isAutoRepeatable(ParamId::BassDuration));
    EXPECT_TRUE(isAutoRepeatable(ParamId::DrumKickVel));
    EXPECT_TRUE(isAutoRepeatable(ParamId::DrumSnareVel));
    EXPECT_TRUE(isAutoRepeatable(ParamId::DrumShakerVel));

    EXPECT_FALSE(isAutoRepeatable(ParamId::ChordOctave));
    EXPECT_FALSE(isAutoRepeatable(ParamId::ChordMinNotes));
    EXPECT_FALSE(isAutoRepeatable(ParamId::ChordMinInterval));
    EXPECT_FALSE(isAutoRepeatable(ParamId::StrumOctave));
    EXPECT_FALSE(isAutoRepeatable(ParamId::ChordMode));
    EXPECT_FALSE(isAutoRepeatable(ParamId::ChordVoicing));
    EXPECT_FALSE(isAutoRepeatable(ParamId::StrumLayout));
    EXPECT_FALSE(isAutoRepeatable(ParamId::RhythmPattern));
    EXPECT_FALSE(isAutoRepeatable(ParamId::RhythmMute));
    EXPECT_FALSE(isAutoRepeatable(ParamId::RhythmEnable));
    EXPECT_FALSE(isAutoRepeatable(ParamId::RhythmClock));
    EXPECT_FALSE(isAutoRepeatable(ParamId::RhythmLed));
}
