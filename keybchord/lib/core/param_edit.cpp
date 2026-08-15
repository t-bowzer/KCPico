#include "param_edit.h"

#include "naming.h"
#include "rhythm.h"


namespace {

const ParamId kChordParams[] = {
    ParamId::ChordOctave, ParamId::ChordMode, ParamId::ChordVoicing,
    ParamId::ChordDuration, ParamId::ChordVelocity, ParamId::ChordPan,
    ParamId::ChordAdd9, ParamId::ChordAdd11, ParamId::ChordAdd13,
};

const ParamId kStrumParams[] = {
    ParamId::StrumOctave, ParamId::StrumDuration, ParamId::StrumVelocity,
    ParamId::StrumLayout,
};

const ParamId kRhythmParams[] = {
    ParamId::RhythmTempo, ParamId::RhythmSwing, ParamId::RhythmPattern,
    ParamId::RhythmMute, ParamId::RhythmEnable, ParamId::RhythmClock,
    ParamId::RhythmLed,
};

const char* const kTitles[] = { "", "Chord Edit", "Strum Edit", "Rhythm Edit" };

const char* const kShortName[] = {
    "Octave", "Mode", "Voicing", "Duration", "Velocity", "Pan",
    "Add9", "Add11", "Add13",
    "Octave", "Duration", "Velocity", "Layout",
    "Tempo", "Swing", "Pattern", "Mute", "Rhythm", "Clock", "Beat LED",
};

const char* const kFullName[] = {
    "Chord Octave", "Chord Mode", "Voicing", "Chord Duration", "Chord Velocity",
    "Chord Pan", "Add9", "Add11", "Add13",
    "Strum Octave", "Strum Duration", "Strum Velocity", "Strum Layout",
    "Rhythm Tempo", "Rhythm Swing", "Rhythm Pattern", "Rhythm Mute",
    "Rhythm On/Off", "Clock Out", "Beat LED",
};

std::string signedInt(int v) {
    return (v > 0) ? ("+" + std::to_string(v)) : std::to_string(v);
}

std::string onOff(bool v) {
    return v ? "On" : "Off";
}

int clampInt(int v, int lo, int hi) {
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

int cycleEnum(int v, int count, int dir) {
    return (v + (dir > 0 ? 1 : -1) + count) % count;
}

} // namespace


const char* menuTitle(EditMenu menu) {
    int m = static_cast<int>(menu);
    if (m < 0 || m >= static_cast<int>(EditMenu::COUNT)) return "";
    return kTitles[m];
}

int menuParamCount(EditMenu menu) {
    switch (menu) {
        case EditMenu::Chord:  return static_cast<int>(sizeof(kChordParams) / sizeof(kChordParams[0]));
        case EditMenu::Strum:  return static_cast<int>(sizeof(kStrumParams) / sizeof(kStrumParams[0]));
        case EditMenu::Rhythm: return static_cast<int>(sizeof(kRhythmParams) / sizeof(kRhythmParams[0]));
        default:               return 0;
    }
}

ParamId menuParamAt(EditMenu menu, int index) {
    if (index < 0 || index >= menuParamCount(menu)) return ParamId::COUNT;
    switch (menu) {
        case EditMenu::Chord:  return kChordParams[index];
        case EditMenu::Strum:  return kStrumParams[index];
        case EditMenu::Rhythm: return kRhythmParams[index];
        default:               return ParamId::COUNT;
    }
}

const char* paramShortName(ParamId id) {
    int i = static_cast<int>(id);
    if (i < 0 || i >= static_cast<int>(ParamId::COUNT)) return "";
    return kShortName[i];
}

const char* paramFullName(ParamId id) {
    int i = static_cast<int>(id);
    if (i < 0 || i >= static_cast<int>(ParamId::COUNT)) return "";
    return kFullName[i];
}

std::string paramValueString(const StateManager& state, ParamId id) {
    switch (id) {
        case ParamId::ChordOctave:  return signedInt(state.pendingChord.octave);
        case ParamId::ChordMode:    return playModeShort(state.pendingChord.play_mode);
        case ParamId::ChordVoicing: return voicingModeName(state.pendingChord.voicing_mode);
        case ParamId::ChordDuration: return std::to_string(state.pendingChord.note_duration_ms);
        case ParamId::ChordVelocity: return std::to_string(state.pendingChord.velocity);
        case ParamId::ChordPan:      return std::to_string(state.pendingChord.pan);
        case ParamId::ChordAdd9:     return onOff(state.pendingChord.add9);
        case ParamId::ChordAdd11:    return onOff(state.pendingChord.add11);
        case ParamId::ChordAdd13:    return onOff(state.pendingChord.add13);

        case ParamId::StrumOctave:   return signedInt(state.pendingStrum.octave);
        case ParamId::StrumDuration: return std::to_string(state.pendingStrum.note_duration_ms);
        case ParamId::StrumVelocity: return std::to_string(state.pendingStrum.velocity);
        case ParamId::StrumLayout:   return state.pendingStrum.limited_keys ? "Limited" : "Full";

        case ParamId::RhythmTempo:   return std::to_string(state.pendingRhythm.tempo);
        case ParamId::RhythmSwing:   return signedInt(state.pendingRhythm.swing);
        case ParamId::RhythmPattern: return rhythmName(state.pendingRhythm.pattern);
        case ParamId::RhythmMute:    return onOff(state.pendingRhythm.muted);
        case ParamId::RhythmEnable:  return onOff(state.pendingRhythm.enabled);
        case ParamId::RhythmClock:   return onOff(state.config.midi_clock_enabled);
        case ParamId::RhythmLed:     return onOff(state.config.bpm_indicator);

        default: return "";
    }
}

void paramStep(StateManager& state, ParamId id, int delta) {
    int dir = (delta > 0) ? 1 : -1;

    switch (id) {
        case ParamId::ChordOctave:
            state.pendingChord.octave = static_cast<int8_t>(clampInt(
                state.pendingChord.octave + dir, param_bounds::CHORD_OCTAVE_MIN, param_bounds::CHORD_OCTAVE_MAX));
            break;
        case ParamId::ChordMode:
            state.pendingChord.play_mode = static_cast<PlayMode>(
                cycleEnum(static_cast<int>(state.pendingChord.play_mode),
                          static_cast<int>(PlayMode::COUNT), dir));
            break;
        case ParamId::ChordVoicing:
            state.pendingChord.voicing_mode =
                (delta > 0) ? VoicingMode::Smart : VoicingMode::RootPosition;
            break;
        case ParamId::ChordDuration:
            state.pendingChord.note_duration_ms = static_cast<int16_t>(clampInt(
                state.pendingChord.note_duration_ms + dir * 50,
                param_bounds::CHORD_NOTE_DURATION_MIN, param_bounds::CHORD_NOTE_DURATION_MAX));
            break;
        case ParamId::ChordVelocity:
            state.pendingChord.velocity = static_cast<uint8_t>(clampInt(
                state.pendingChord.velocity + dir,
                param_bounds::CHORD_VELOCITY_MIN, param_bounds::CHORD_VELOCITY_MAX));
            break;
        case ParamId::ChordPan:
            state.pendingChord.pan = static_cast<uint8_t>(clampInt(
                state.pendingChord.pan + dir,
                param_bounds::CHORD_PAN_MIN, param_bounds::CHORD_PAN_MAX));
            break;
        case ParamId::ChordAdd9:  state.pendingChord.add9  = delta > 0; break;
        case ParamId::ChordAdd11: state.pendingChord.add11 = delta > 0; break;
        case ParamId::ChordAdd13: state.pendingChord.add13 = delta > 0; break;

        case ParamId::StrumOctave:
            state.pendingStrum.octave = static_cast<int8_t>(clampInt(
                state.pendingStrum.octave + dir, param_bounds::STRUM_OCTAVE_MIN, param_bounds::STRUM_OCTAVE_MAX));
            break;
        case ParamId::StrumDuration:
            state.pendingStrum.note_duration_ms = static_cast<int16_t>(clampInt(
                state.pendingStrum.note_duration_ms + dir * 50,
                param_bounds::STRUM_NOTE_DURATION_MIN, param_bounds::STRUM_NOTE_DURATION_MAX));
            break;
        case ParamId::StrumVelocity:
            state.pendingStrum.velocity = static_cast<uint8_t>(clampInt(
                state.pendingStrum.velocity + dir,
                param_bounds::STRUM_VELOCITY_MIN, param_bounds::STRUM_VELOCITY_MAX));
            break;
        case ParamId::StrumLayout: state.pendingStrum.limited_keys = delta > 0; break;

        case ParamId::RhythmTempo:
            state.pendingRhythm.tempo = static_cast<uint16_t>(clampInt(
                state.pendingRhythm.tempo + dir, param_bounds::TEMPO_MIN, param_bounds::TEMPO_MAX));
            break;
        case ParamId::RhythmSwing:
            state.pendingRhythm.swing = static_cast<int8_t>(clampInt(
                state.pendingRhythm.swing + dir * 5, param_bounds::SWING_MIN, param_bounds::SWING_MAX));
            break;
        case ParamId::RhythmPattern:
            state.pendingRhythm.pattern = static_cast<uint8_t>(cycleEnum(
                state.pendingRhythm.pattern, param_bounds::RHYTHM_PATTERN_MAX + 1, dir));
            break;
        case ParamId::RhythmMute:   state.pendingRhythm.muted   = delta > 0; break;
        case ParamId::RhythmEnable: state.pendingRhythm.enabled = delta > 0; break;
        case ParamId::RhythmClock:  state.config.midi_clock_enabled = delta > 0; break;
        case ParamId::RhythmLed:    state.config.bpm_indicator       = delta > 0; break;

        default: break;
    }
}

void paramCycle(StateManager& state, ParamId id) {
    switch (id) {
        case ParamId::ChordMode:
            state.pendingChord.play_mode = static_cast<PlayMode>(
                cycleEnum(static_cast<int>(state.pendingChord.play_mode),
                          static_cast<int>(PlayMode::COUNT), 1));
            break;
        case ParamId::ChordVoicing:
            state.pendingChord.voicing_mode =
                (state.pendingChord.voicing_mode == VoicingMode::RootPosition)
                    ? VoicingMode::Smart : VoicingMode::RootPosition;
            break;
        case ParamId::ChordAdd9:  state.pendingChord.add9  = !state.pendingChord.add9;  break;
        case ParamId::ChordAdd11: state.pendingChord.add11 = !state.pendingChord.add11; break;
        case ParamId::ChordAdd13: state.pendingChord.add13 = !state.pendingChord.add13; break;
        case ParamId::StrumLayout: state.pendingStrum.limited_keys = !state.pendingStrum.limited_keys; break;
        case ParamId::RhythmPattern:
            state.pendingRhythm.pattern = static_cast<uint8_t>(cycleEnum(
                state.pendingRhythm.pattern, param_bounds::RHYTHM_PATTERN_MAX + 1, 1));
            break;
        case ParamId::RhythmMute:   state.pendingRhythm.muted   = !state.pendingRhythm.muted;   break;
        case ParamId::RhythmEnable: state.pendingRhythm.enabled = !state.pendingRhythm.enabled; break;
        case ParamId::RhythmClock:  state.config.midi_clock_enabled = !state.config.midi_clock_enabled; break;
        case ParamId::RhythmLed:    state.config.bpm_indicator       = !state.config.bpm_indicator;       break;
        default:
            // Int params: treat a single-key "cycle" as +1.
            paramStep(state, id, 1);
            break;
    }
}
