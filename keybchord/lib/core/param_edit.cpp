#include "param_edit.h"

#include "naming.h"
#include "rhythm.h"
#include "strum.h"


namespace {

const ParamId kChordParams[] = {
    ParamId::ChordOctave, ParamId::ChordMode, ParamId::ChordVoicing,
    ParamId::ChordDuration, ParamId::ChordVelocity, ParamId::ChordPan,
    ParamId::ChordRoll, ParamId::ChordMinNotes, ParamId::ChordMinInterval,
    ParamId::ChordInversion, ParamId::ChordArpMode,
};

const ParamId kStrumParams[] = {
    ParamId::StrumOctave, ParamId::StrumDuration, ParamId::StrumVelocity,
    ParamId::StrumLayout, ParamId::StrumMode, ParamId::StrumRoot,
    ParamId::StrumScale,
};

const ParamId kRhythmParams[] = {
    ParamId::RhythmTempo, ParamId::RhythmSwing, ParamId::RhythmPattern,
    ParamId::RhythmMute, ParamId::RhythmEnable, ParamId::RhythmClock,
    ParamId::RhythmLed,
};

const ParamId kBassParams[] = {
    ParamId::BassEnable, ParamId::BassOctave, ParamId::BassDuration,
    ParamId::BassVelocity, ParamId::BassChannel,
};

const ParamId kDrumParams[] = {
    ParamId::DrumKickNote, ParamId::DrumKickVel, ParamId::DrumSnareNote,
    ParamId::DrumSnareVel, ParamId::DrumHihatNote, ParamId::DrumHihatVel,
    ParamId::DrumOpenHatNote, ParamId::DrumOpenHatVel,
};

const char* const kTitles[] = {
    "", "Chord Edit", "Strum Edit", "Rhythm Edit", "Bass Edit", "Drum Edit",
};

const char* const kShortName[] = {
    "Octave", "Mode", "Voicing", "Duration", "Velocity", "Pan",
    "Roll", "Min Notes", "Min Interval", "Inversion", "Arp Mode",
    "Octave", "Duration", "Velocity", "Layout", "Mode", "Root", "Scale",
    "Tempo", "Swing", "Pattern", "Mute", "Rhythm", "Clock", "Beat LED",
    "Enable", "Octave", "Duration", "Velocity", "Channel",
    "Kick", "Kick Vel", "Snare", "Snare Vel",
    "Hi-Hat", "Hi-Hat Vel", "Open Hat", "Open Hat Vel",
};

const char* const kFullName[] = {
    "Chord Octave", "Chord Mode", "Voicing", "Chord Duration", "Chord Velocity",
    "Chord Pan", "Chord Roll", "Min Notes", "Min Interval", "Inversion", "Arp Mode",
    "Strum Octave", "Strum Duration", "Strum Velocity", "Strum Layout",
    "Strum Mode", "Strum Root", "Strum Scale",
    "Rhythm Tempo", "Rhythm Swing", "Rhythm Pattern", "Rhythm Mute",
    "Rhythm On/Off", "Clock Out", "Beat LED",
    "Bass On/Off", "Bass Octave", "Bass Duration", "Bass Velocity", "Bass Channel",
    "Kick Note", "Kick Vel", "Snare Note", "Snare Vel",
    "Hi-Hat Note", "Hi-Hat Vel", "Open Hat Note", "Open Hat Vel",
};

const char* const kPcNames[12] = {
    "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B",
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

const char* inversionShort(InversionMode m) {
    switch (m) {
        case InversionMode::First:  return "1st";
        case InversionMode::Second: return "2nd";
        case InversionMode::Third:  return "3rd";
        default:                    return "Root";
    }
}

const char* arpModeShort(ArpMode m) {
    switch (m) {
        case ArpMode::Down:        return "Down";
        case ArpMode::UpDown:      return "Up-Down";
        case ArpMode::Alternating: return "Alt";
        case ArpMode::Random:      return "Random";
        default:                   return "Up";
    }
}

const char* strumModeShort(StrumMode m) {
    switch (m) {
        case StrumMode::Scale: return "Scale";
        case StrumMode::Piano: return "Piano";
        default:               return "Chord";
    }
}

const char* pcName(uint8_t pc) {
    return kPcNames[pc % 12];
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
        case EditMenu::Bass:   return static_cast<int>(sizeof(kBassParams) / sizeof(kBassParams[0]));
        case EditMenu::Drum:   return static_cast<int>(sizeof(kDrumParams) / sizeof(kDrumParams[0]));
        default:               return 0;
    }
}

ParamId menuParamAt(EditMenu menu, int index) {
    if (index < 0 || index >= menuParamCount(menu)) return ParamId::COUNT;
    switch (menu) {
        case EditMenu::Chord:  return kChordParams[index];
        case EditMenu::Strum:  return kStrumParams[index];
        case EditMenu::Rhythm: return kRhythmParams[index];
        case EditMenu::Bass:   return kBassParams[index];
        case EditMenu::Drum:   return kDrumParams[index];
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
        case ParamId::ChordOctave:   return signedInt(state.pendingChord.octave);
        case ParamId::ChordMode:     return playModeShort(state.pendingChord.play_mode);
        case ParamId::ChordVoicing:  return voicingModeName(state.pendingChord.voicing_mode);
        case ParamId::ChordDuration: return std::to_string(state.pendingChord.note_duration_ms);
        case ParamId::ChordVelocity: return std::to_string(state.pendingChord.velocity);
        case ParamId::ChordPan:      return std::to_string(state.pendingChord.pan);
        case ParamId::ChordRoll:     return signedInt(state.pendingChord.chord_roll_ms);
        case ParamId::ChordMinNotes: return std::to_string(state.pendingChord.min_notes);
        case ParamId::ChordMinInterval:
            return state.pendingChord.min_interval == 0
                       ? "Off" : std::to_string(state.pendingChord.min_interval);
        case ParamId::ChordInversion: return inversionShort(state.pendingChord.inversion);
        case ParamId::ChordArpMode:   return arpModeShort(state.pendingChord.arp_mode);

        case ParamId::StrumOctave:   return signedInt(state.pendingStrum.octave);
        case ParamId::StrumDuration: return std::to_string(state.pendingStrum.note_duration_ms);
        case ParamId::StrumVelocity: return std::to_string(state.pendingStrum.velocity);
        case ParamId::StrumLayout:   return state.pendingStrum.limited_keys ? "Limited" : "Full";
        case ParamId::StrumMode:     return strumModeShort(state.pendingStrum.mode);
        case ParamId::StrumRoot:     return pcName(state.pendingStrum.root_pc);
        case ParamId::StrumScale:    return scaleTypeShortName(state.pendingStrum.scale_type);

        case ParamId::RhythmTempo:   return std::to_string(state.pendingRhythm.tempo);
        case ParamId::RhythmSwing:   return signedInt(state.pendingRhythm.swing);
        case ParamId::RhythmPattern: return rhythmName(state.pendingRhythm.pattern);
        case ParamId::RhythmMute:    return onOff(state.pendingRhythm.muted);
        case ParamId::RhythmEnable:  return onOff(state.pendingRhythm.enabled);
        case ParamId::RhythmClock:   return onOff(state.config.midi_clock_enabled);
        case ParamId::RhythmLed:     return onOff(state.config.bpm_indicator);

        case ParamId::BassEnable:    return onOff(state.pendingBass.enabled);
        case ParamId::BassOctave:    return signedInt(state.pendingBass.octave);
        case ParamId::BassDuration:  return std::to_string(state.pendingBass.note_duration_ms);
        case ParamId::BassVelocity:  return std::to_string(state.pendingBass.velocity);
        case ParamId::BassChannel:   return std::to_string(state.pendingBass.channel);

        case ParamId::DrumKickNote:    return std::to_string(state.pendingRhythm.drums.kick);
        case ParamId::DrumKickVel:
            return state.pendingRhythm.drums.kick_vel == 0
                       ? "Auto" : std::to_string(state.pendingRhythm.drums.kick_vel);
        case ParamId::DrumSnareNote:   return std::to_string(state.pendingRhythm.drums.snare);
        case ParamId::DrumSnareVel:
            return state.pendingRhythm.drums.snare_vel == 0
                       ? "Auto" : std::to_string(state.pendingRhythm.drums.snare_vel);
        case ParamId::DrumHihatNote:   return std::to_string(state.pendingRhythm.drums.hihat);
        case ParamId::DrumHihatVel:
            return state.pendingRhythm.drums.hihat_vel == 0
                       ? "Auto" : std::to_string(state.pendingRhythm.drums.hihat_vel);
        case ParamId::DrumOpenHatNote: return std::to_string(state.pendingRhythm.drums.open_hat);
        case ParamId::DrumOpenHatVel:
            return state.pendingRhythm.drums.open_hat_vel == 0
                       ? "Auto" : std::to_string(state.pendingRhythm.drums.open_hat_vel);

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
        case ParamId::ChordRoll:
            state.pendingChord.chord_roll_ms = static_cast<int16_t>(clampInt(
                state.pendingChord.chord_roll_ms + dir * param_bounds::CHORD_ROLL_STEP,
                param_bounds::CHORD_ROLL_MIN, param_bounds::CHORD_ROLL_MAX));
            break;
        case ParamId::ChordMinNotes:
            state.pendingChord.min_notes = static_cast<uint8_t>(clampInt(
                state.pendingChord.min_notes + dir,
                param_bounds::CHORD_MIN_NOTES_MIN, param_bounds::CHORD_MIN_NOTES_MAX));
            break;
        case ParamId::ChordMinInterval:
            state.pendingChord.min_interval = static_cast<uint8_t>(clampInt(
                state.pendingChord.min_interval + dir,
                param_bounds::CHORD_MIN_INTERVAL_MIN, param_bounds::CHORD_MIN_INTERVAL_MAX));
            break;
        case ParamId::ChordInversion:
            state.pendingChord.inversion = static_cast<InversionMode>(
                cycleEnum(static_cast<int>(state.pendingChord.inversion),
                          static_cast<int>(InversionMode::COUNT), dir));
            break;
        case ParamId::ChordArpMode:
            state.pendingChord.arp_mode = static_cast<ArpMode>(
                cycleEnum(static_cast<int>(state.pendingChord.arp_mode),
                          static_cast<int>(ArpMode::COUNT), dir));
            break;

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
        case ParamId::StrumMode:
            state.pendingStrum.mode = static_cast<StrumMode>(
                cycleEnum(static_cast<int>(state.pendingStrum.mode),
                          static_cast<int>(StrumMode::COUNT), dir));
            break;
        case ParamId::StrumRoot:
            state.pendingStrum.root_pc = static_cast<uint8_t>(cycleEnum(
                state.pendingStrum.root_pc, 12, dir));
            break;
        case ParamId::StrumScale:
            state.pendingStrum.scale_type = static_cast<ScaleType>(
                cycleEnum(static_cast<int>(state.pendingStrum.scale_type),
                          static_cast<int>(ScaleType::COUNT), dir));
            break;

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

        case ParamId::BassEnable:   state.pendingBass.enabled = delta > 0; break;
        case ParamId::BassOctave:
            state.pendingBass.octave = static_cast<int8_t>(clampInt(
                state.pendingBass.octave + dir, param_bounds::BASS_OCTAVE_MIN, param_bounds::BASS_OCTAVE_MAX));
            break;
        case ParamId::BassDuration:
            state.pendingBass.note_duration_ms = static_cast<int16_t>(clampInt(
                state.pendingBass.note_duration_ms + dir * 50,
                param_bounds::BASS_NOTE_DURATION_MIN, param_bounds::BASS_NOTE_DURATION_MAX));
            break;
        case ParamId::BassVelocity:
            state.pendingBass.velocity = static_cast<uint8_t>(clampInt(
                state.pendingBass.velocity + dir,
                param_bounds::BASS_VELOCITY_MIN, param_bounds::BASS_VELOCITY_MAX));
            break;
        case ParamId::BassChannel:
            state.pendingBass.channel = static_cast<uint8_t>(clampInt(
                state.pendingBass.channel + dir, param_bounds::BASS_CHANNEL_MIN, param_bounds::BASS_CHANNEL_MAX));
            break;

        case ParamId::DrumKickNote:
            state.pendingRhythm.drums.kick = static_cast<uint8_t>(clampInt(
                state.pendingRhythm.drums.kick + dir,
                param_bounds::DRUM_NOTE_MIN, param_bounds::DRUM_NOTE_MAX));
            break;
        case ParamId::DrumKickVel:
            state.pendingRhythm.drums.kick_vel = static_cast<uint8_t>(clampInt(
                state.pendingRhythm.drums.kick_vel + dir,
                param_bounds::DRUM_VELOCITY_MIN, param_bounds::DRUM_VELOCITY_MAX));
            break;
        case ParamId::DrumSnareNote:
            state.pendingRhythm.drums.snare = static_cast<uint8_t>(clampInt(
                state.pendingRhythm.drums.snare + dir,
                param_bounds::DRUM_NOTE_MIN, param_bounds::DRUM_NOTE_MAX));
            break;
        case ParamId::DrumSnareVel:
            state.pendingRhythm.drums.snare_vel = static_cast<uint8_t>(clampInt(
                state.pendingRhythm.drums.snare_vel + dir,
                param_bounds::DRUM_VELOCITY_MIN, param_bounds::DRUM_VELOCITY_MAX));
            break;
        case ParamId::DrumHihatNote:
            state.pendingRhythm.drums.hihat = static_cast<uint8_t>(clampInt(
                state.pendingRhythm.drums.hihat + dir,
                param_bounds::DRUM_NOTE_MIN, param_bounds::DRUM_NOTE_MAX));
            break;
        case ParamId::DrumHihatVel:
            state.pendingRhythm.drums.hihat_vel = static_cast<uint8_t>(clampInt(
                state.pendingRhythm.drums.hihat_vel + dir,
                param_bounds::DRUM_VELOCITY_MIN, param_bounds::DRUM_VELOCITY_MAX));
            break;
        case ParamId::DrumOpenHatNote:
            state.pendingRhythm.drums.open_hat = static_cast<uint8_t>(clampInt(
                state.pendingRhythm.drums.open_hat + dir,
                param_bounds::DRUM_NOTE_MIN, param_bounds::DRUM_NOTE_MAX));
            break;
        case ParamId::DrumOpenHatVel:
            state.pendingRhythm.drums.open_hat_vel = static_cast<uint8_t>(clampInt(
                state.pendingRhythm.drums.open_hat_vel + dir,
                param_bounds::DRUM_VELOCITY_MIN, param_bounds::DRUM_VELOCITY_MAX));
            break;

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
        case ParamId::ChordInversion:
            state.pendingChord.inversion = static_cast<InversionMode>(
                cycleEnum(static_cast<int>(state.pendingChord.inversion),
                          static_cast<int>(InversionMode::COUNT), 1));
            break;
        case ParamId::ChordArpMode:
            state.pendingChord.arp_mode = static_cast<ArpMode>(
                cycleEnum(static_cast<int>(state.pendingChord.arp_mode),
                          static_cast<int>(ArpMode::COUNT), 1));
            break;
        case ParamId::StrumLayout: state.pendingStrum.limited_keys = !state.pendingStrum.limited_keys; break;
        case ParamId::StrumMode:
            state.pendingStrum.mode = static_cast<StrumMode>(
                cycleEnum(static_cast<int>(state.pendingStrum.mode),
                          static_cast<int>(StrumMode::COUNT), 1));
            break;
        case ParamId::StrumScale:
            state.pendingStrum.scale_type = static_cast<ScaleType>(
                cycleEnum(static_cast<int>(state.pendingStrum.scale_type),
                          static_cast<int>(ScaleType::COUNT), 1));
            break;
        case ParamId::RhythmPattern:
            state.pendingRhythm.pattern = static_cast<uint8_t>(cycleEnum(
                state.pendingRhythm.pattern, param_bounds::RHYTHM_PATTERN_MAX + 1, 1));
            break;
        case ParamId::RhythmMute:   state.pendingRhythm.muted   = !state.pendingRhythm.muted;   break;
        case ParamId::RhythmEnable: state.pendingRhythm.enabled = !state.pendingRhythm.enabled; break;
        case ParamId::RhythmClock:  state.config.midi_clock_enabled = !state.config.midi_clock_enabled; break;
        case ParamId::RhythmLed:    state.config.bpm_indicator       = !state.config.bpm_indicator;       break;
        case ParamId::BassEnable:   state.pendingBass.enabled = !state.pendingBass.enabled; break;
        default:
            // Int params: treat a single-key "cycle" as +1.
            paramStep(state, id, 1);
            break;
    }
}

bool isAutoRepeatable(ParamId id) {
    switch (id) {
        case ParamId::ChordDuration:
        case ParamId::ChordVelocity:
        case ParamId::ChordPan:
        case ParamId::ChordRoll:
        case ParamId::StrumDuration:
        case ParamId::StrumVelocity:
        case ParamId::RhythmTempo:
        case ParamId::RhythmSwing:
        case ParamId::BassDuration:
        case ParamId::BassVelocity:
        case ParamId::DrumKickNote:
        case ParamId::DrumSnareNote:
        case ParamId::DrumHihatNote:
        case ParamId::DrumOpenHatNote:
            return true;
        default:
            return false;
    }
}
