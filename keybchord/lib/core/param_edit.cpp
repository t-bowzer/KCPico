#include "param_edit.h"

#include "bass.h"
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
    ParamId::BassVelocity, ParamId::BassChannel, ParamId::BassPattern,
};

const ParamId kDrumParams[] = {
    ParamId::DrumKickNote, ParamId::DrumKickVel, ParamId::DrumSnareNote,
    ParamId::DrumSnareVel, ParamId::DrumHihatNote, ParamId::DrumHihatVel,
    ParamId::DrumOpenHatNote, ParamId::DrumOpenHatVel,
    ParamId::DrumRimshotNote, ParamId::DrumRimshotVel,
    ParamId::DrumClapNote, ParamId::DrumClapVel,
    ParamId::DrumCrashNote, ParamId::DrumCrashVel,
    ParamId::DrumRideNote, ParamId::DrumRideVel,
    ParamId::DrumBongoNote, ParamId::DrumBongoVel,
    ParamId::DrumCongaLoNote, ParamId::DrumCongaLoVel,
    ParamId::DrumCongaHiNote, ParamId::DrumCongaHiVel,
    ParamId::DrumClaveNote, ParamId::DrumClaveVel,
    ParamId::DrumShakerNote, ParamId::DrumShakerVel,
};

const char* const kTitles[] = {
    "", "Chord Edit", "Strum Edit", "Rhythm Edit", "Bass Edit", "Drum Edit",
};

const char* const kShortName[] = {
    "Octave", "Mode", "Voicing", "Duration", "Velocity", "Pan",
    "Roll", "Min Notes", "Min Interval", "Inversion", "Arp Mode",
    "Octave", "Duration", "Velocity", "Layout", "Mode", "Root", "Scale",
    "Tempo", "Swing", "Pattern", "Mute", "Rhythm", "Clock", "Beat LED", "USB MIDI",
    "Enable", "Octave", "Duration", "Velocity", "Channel", "Pattern",
    "Kick", "Kick Vel", "Snare", "Snare Vel",
    "Hi-Hat", "Hi-Hat Vel", "Open Hat", "Open Hat Vel",
    "Rimshot", "Rimshot Vel", "Clap", "Clap Vel",
    "Crash", "Crash Vel", "Ride", "Ride Vel",
    "Bongo", "Bongo Vel", "Conga Lo", "Conga Lo Vel",
    "Conga Hi", "Conga Hi Vel", "Clave", "Clave Vel",
    "Shaker", "Shaker Vel",
};

const char* const kFullName[] = {
    "Chord Octave", "Chord Mode", "Voicing", "Chord Duration", "Chord Velocity",
    "Chord Pan", "Chord Roll", "Min Notes", "Min Interval", "Inversion", "Arp Mode",
    "Strum Octave", "Strum Duration", "Strum Velocity", "Strum Layout",
    "Strum Mode", "Strum Root", "Strum Scale",
    "Rhythm Tempo", "Rhythm Swing", "Rhythm Pattern", "Rhythm Mute",
    "Rhythm On/Off", "Clock Out", "Beat LED", "USB MIDI",
    "Bass On/Off", "Bass Octave", "Bass Duration", "Bass Velocity", "Bass Channel",
    "Bass Pattern",
    "Kick Note", "Kick Vel", "Snare Note", "Snare Vel",
    "Hi-Hat Note", "Hi-Hat Vel", "Open Hat Note", "Open Hat Vel",
    "Rimshot Note", "Rimshot Vel", "Clap Note", "Clap Vel",
    "Crash Note", "Crash Vel", "Ride Note", "Ride Vel",
    "Bongo Note", "Bongo Vel", "Conga Lo Note", "Conga Lo Vel",
    "Conga Hi Note", "Conga Hi Vel", "Clave Note", "Clave Vel",
    "Shaker Note", "Shaker Vel",
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

std::string drumVelString(uint8_t v) {
    if (v == param_bounds::DRUM_VELOCITY_OFF) return "Off";
    return (v == 0) ? "Auto" : std::to_string(v);
}

// Drum velocity cycles 0 (Auto) -> 1..127 -> 128 (Off) -> 0. The position is
// simply the byte value, so stepping is a modular increment over 129 positions.
uint8_t stepDrumVel(uint8_t v, int dir) {
    constexpr int POSITIONS = 129;
    return static_cast<uint8_t>(
        (static_cast<int>(v) + dir + POSITIONS) % POSITIONS);
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
        case ParamId::UsbMidi:       return onOff(state.config.usb_midi_enabled);

        case ParamId::BassEnable:    return onOff(state.pendingBass.enabled);
        case ParamId::BassOctave:    return signedInt(state.pendingBass.octave);
        case ParamId::BassDuration:  return std::to_string(state.pendingBass.note_duration_ms);
        case ParamId::BassVelocity:  return std::to_string(state.pendingBass.velocity);
        case ParamId::BassChannel:   return std::to_string(state.pendingBass.channel);
        case ParamId::BassPattern:   return bassName(state.pendingBass.pattern);

        case ParamId::DrumKickNote:    return std::to_string(state.pendingRhythm.drums.kick);
        case ParamId::DrumKickVel:     return drumVelString(state.pendingRhythm.drums.kick_vel);
        case ParamId::DrumSnareNote:   return std::to_string(state.pendingRhythm.drums.snare);
        case ParamId::DrumSnareVel:    return drumVelString(state.pendingRhythm.drums.snare_vel);
        case ParamId::DrumHihatNote:   return std::to_string(state.pendingRhythm.drums.hihat);
        case ParamId::DrumHihatVel:    return drumVelString(state.pendingRhythm.drums.hihat_vel);
        case ParamId::DrumOpenHatNote: return std::to_string(state.pendingRhythm.drums.open_hat);
        case ParamId::DrumOpenHatVel:  return drumVelString(state.pendingRhythm.drums.open_hat_vel);
        case ParamId::DrumRimshotNote: return std::to_string(state.pendingRhythm.drums.rimshot);
        case ParamId::DrumRimshotVel:  return drumVelString(state.pendingRhythm.drums.rimshot_vel);
        case ParamId::DrumClapNote:    return std::to_string(state.pendingRhythm.drums.clap);
        case ParamId::DrumClapVel:     return drumVelString(state.pendingRhythm.drums.clap_vel);
        case ParamId::DrumCrashNote:   return std::to_string(state.pendingRhythm.drums.crash);
        case ParamId::DrumCrashVel:    return drumVelString(state.pendingRhythm.drums.crash_vel);
        case ParamId::DrumRideNote:    return std::to_string(state.pendingRhythm.drums.ride);
        case ParamId::DrumRideVel:     return drumVelString(state.pendingRhythm.drums.ride_vel);
        case ParamId::DrumBongoNote:   return std::to_string(state.pendingRhythm.drums.bongo);
        case ParamId::DrumBongoVel:    return drumVelString(state.pendingRhythm.drums.bongo_vel);
        case ParamId::DrumCongaLoNote: return std::to_string(state.pendingRhythm.drums.conga_lo);
        case ParamId::DrumCongaLoVel:  return drumVelString(state.pendingRhythm.drums.conga_lo_vel);
        case ParamId::DrumCongaHiNote: return std::to_string(state.pendingRhythm.drums.conga_hi);
        case ParamId::DrumCongaHiVel:  return drumVelString(state.pendingRhythm.drums.conga_hi_vel);
        case ParamId::DrumClaveNote:   return std::to_string(state.pendingRhythm.drums.clave);
        case ParamId::DrumClaveVel:    return drumVelString(state.pendingRhythm.drums.clave_vel);
        case ParamId::DrumShakerNote:  return std::to_string(state.pendingRhythm.drums.shaker);
        case ParamId::DrumShakerVel:   return drumVelString(state.pendingRhythm.drums.shaker_vel);

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
            state.pendingChord.voicing_mode = static_cast<VoicingMode>(
                cycleEnum(static_cast<int>(state.pendingChord.voicing_mode),
                          static_cast<int>(VoicingMode::COUNT), dir));
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
                state.pendingRhythm.pattern, rhythmCount(), dir));
            break;
        case ParamId::RhythmMute:   state.pendingRhythm.muted   = delta > 0; break;
        case ParamId::RhythmEnable: state.pendingRhythm.enabled = delta > 0; break;
        case ParamId::RhythmClock:  state.config.midi_clock_enabled = delta > 0; break;
        case ParamId::RhythmLed:    state.config.bpm_indicator       = delta > 0; break;
        case ParamId::UsbMidi:      state.config.usb_midi_enabled    = delta > 0; break;

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
        case ParamId::BassPattern:
            state.pendingBass.pattern = static_cast<uint8_t>(cycleEnum(
                state.pendingBass.pattern, bassCount(), dir));
            break;

        case ParamId::DrumKickNote:
            state.pendingRhythm.drums.kick = static_cast<uint8_t>(clampInt(
                state.pendingRhythm.drums.kick + dir,
                param_bounds::DRUM_NOTE_MIN, param_bounds::DRUM_NOTE_MAX));
            break;
        case ParamId::DrumKickVel:
            state.pendingRhythm.drums.kick_vel = stepDrumVel(state.pendingRhythm.drums.kick_vel, dir);
            break;
        case ParamId::DrumSnareNote:
            state.pendingRhythm.drums.snare = static_cast<uint8_t>(clampInt(
                state.pendingRhythm.drums.snare + dir,
                param_bounds::DRUM_NOTE_MIN, param_bounds::DRUM_NOTE_MAX));
            break;
        case ParamId::DrumSnareVel:
            state.pendingRhythm.drums.snare_vel = stepDrumVel(state.pendingRhythm.drums.snare_vel, dir);
            break;
        case ParamId::DrumHihatNote:
            state.pendingRhythm.drums.hihat = static_cast<uint8_t>(clampInt(
                state.pendingRhythm.drums.hihat + dir,
                param_bounds::DRUM_NOTE_MIN, param_bounds::DRUM_NOTE_MAX));
            break;
        case ParamId::DrumHihatVel:
            state.pendingRhythm.drums.hihat_vel = stepDrumVel(state.pendingRhythm.drums.hihat_vel, dir);
            break;
        case ParamId::DrumOpenHatNote:
            state.pendingRhythm.drums.open_hat = static_cast<uint8_t>(clampInt(
                state.pendingRhythm.drums.open_hat + dir,
                param_bounds::DRUM_NOTE_MIN, param_bounds::DRUM_NOTE_MAX));
            break;
        case ParamId::DrumOpenHatVel:
            state.pendingRhythm.drums.open_hat_vel = stepDrumVel(state.pendingRhythm.drums.open_hat_vel, dir);
            break;
        case ParamId::DrumRimshotNote:
            state.pendingRhythm.drums.rimshot = static_cast<uint8_t>(clampInt(
                state.pendingRhythm.drums.rimshot + dir,
                param_bounds::DRUM_NOTE_MIN, param_bounds::DRUM_NOTE_MAX));
            break;
        case ParamId::DrumRimshotVel:
            state.pendingRhythm.drums.rimshot_vel = stepDrumVel(state.pendingRhythm.drums.rimshot_vel, dir);
            break;
        case ParamId::DrumClapNote:
            state.pendingRhythm.drums.clap = static_cast<uint8_t>(clampInt(
                state.pendingRhythm.drums.clap + dir,
                param_bounds::DRUM_NOTE_MIN, param_bounds::DRUM_NOTE_MAX));
            break;
        case ParamId::DrumClapVel:
            state.pendingRhythm.drums.clap_vel = stepDrumVel(state.pendingRhythm.drums.clap_vel, dir);
            break;
        case ParamId::DrumCrashNote:
            state.pendingRhythm.drums.crash = static_cast<uint8_t>(clampInt(
                state.pendingRhythm.drums.crash + dir,
                param_bounds::DRUM_NOTE_MIN, param_bounds::DRUM_NOTE_MAX));
            break;
        case ParamId::DrumCrashVel:
            state.pendingRhythm.drums.crash_vel = stepDrumVel(state.pendingRhythm.drums.crash_vel, dir);
            break;
        case ParamId::DrumRideNote:
            state.pendingRhythm.drums.ride = static_cast<uint8_t>(clampInt(
                state.pendingRhythm.drums.ride + dir,
                param_bounds::DRUM_NOTE_MIN, param_bounds::DRUM_NOTE_MAX));
            break;
        case ParamId::DrumRideVel:
            state.pendingRhythm.drums.ride_vel = stepDrumVel(state.pendingRhythm.drums.ride_vel, dir);
            break;
        case ParamId::DrumBongoNote:
            state.pendingRhythm.drums.bongo = static_cast<uint8_t>(clampInt(
                state.pendingRhythm.drums.bongo + dir,
                param_bounds::DRUM_NOTE_MIN, param_bounds::DRUM_NOTE_MAX));
            break;
        case ParamId::DrumBongoVel:
            state.pendingRhythm.drums.bongo_vel = stepDrumVel(state.pendingRhythm.drums.bongo_vel, dir);
            break;
        case ParamId::DrumCongaLoNote:
            state.pendingRhythm.drums.conga_lo = static_cast<uint8_t>(clampInt(
                state.pendingRhythm.drums.conga_lo + dir,
                param_bounds::DRUM_NOTE_MIN, param_bounds::DRUM_NOTE_MAX));
            break;
        case ParamId::DrumCongaLoVel:
            state.pendingRhythm.drums.conga_lo_vel = stepDrumVel(state.pendingRhythm.drums.conga_lo_vel, dir);
            break;
        case ParamId::DrumCongaHiNote:
            state.pendingRhythm.drums.conga_hi = static_cast<uint8_t>(clampInt(
                state.pendingRhythm.drums.conga_hi + dir,
                param_bounds::DRUM_NOTE_MIN, param_bounds::DRUM_NOTE_MAX));
            break;
        case ParamId::DrumCongaHiVel:
            state.pendingRhythm.drums.conga_hi_vel = stepDrumVel(state.pendingRhythm.drums.conga_hi_vel, dir);
            break;
        case ParamId::DrumClaveNote:
            state.pendingRhythm.drums.clave = static_cast<uint8_t>(clampInt(
                state.pendingRhythm.drums.clave + dir,
                param_bounds::DRUM_NOTE_MIN, param_bounds::DRUM_NOTE_MAX));
            break;
        case ParamId::DrumClaveVel:
            state.pendingRhythm.drums.clave_vel = stepDrumVel(state.pendingRhythm.drums.clave_vel, dir);
            break;
        case ParamId::DrumShakerNote:
            state.pendingRhythm.drums.shaker = static_cast<uint8_t>(clampInt(
                state.pendingRhythm.drums.shaker + dir,
                param_bounds::DRUM_NOTE_MIN, param_bounds::DRUM_NOTE_MAX));
            break;
        case ParamId::DrumShakerVel:
            state.pendingRhythm.drums.shaker_vel = stepDrumVel(state.pendingRhythm.drums.shaker_vel, dir);
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
            state.pendingChord.voicing_mode = static_cast<VoicingMode>(
                cycleEnum(static_cast<int>(state.pendingChord.voicing_mode),
                          static_cast<int>(VoicingMode::COUNT), 1));
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
                state.pendingRhythm.pattern, rhythmCount(), 1));
            break;
        case ParamId::RhythmMute:   state.pendingRhythm.muted   = !state.pendingRhythm.muted;   break;
        case ParamId::RhythmEnable: state.pendingRhythm.enabled = !state.pendingRhythm.enabled; break;
        case ParamId::RhythmClock:  state.config.midi_clock_enabled = !state.config.midi_clock_enabled; break;
        case ParamId::RhythmLed:    state.config.bpm_indicator       = !state.config.bpm_indicator;       break;
        case ParamId::UsbMidi:      state.config.usb_midi_enabled    = !state.config.usb_midi_enabled;    break;
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
        case ParamId::DrumRimshotNote:
        case ParamId::DrumClapNote:
        case ParamId::DrumCrashNote:
        case ParamId::DrumRideNote:
        case ParamId::DrumBongoNote:
        case ParamId::DrumCongaLoNote:
        case ParamId::DrumCongaHiNote:
        case ParamId::DrumClaveNote:
        case ParamId::DrumShakerNote:
        case ParamId::DrumKickVel:
        case ParamId::DrumSnareVel:
        case ParamId::DrumHihatVel:
        case ParamId::DrumOpenHatVel:
        case ParamId::DrumRimshotVel:
        case ParamId::DrumClapVel:
        case ParamId::DrumCrashVel:
        case ParamId::DrumRideVel:
        case ParamId::DrumBongoVel:
        case ParamId::DrumCongaLoVel:
        case ParamId::DrumCongaHiVel:
        case ParamId::DrumClaveVel:
        case ParamId::DrumShakerVel:
            return true;
        default:
            return false;
    }
}

namespace {

// Current value of a non-drum parameter as an int (enum index, bool 0/1, int).
int paramIntValue(const StateManager& s, ParamId id) {
    switch (id) {
        case ParamId::ChordOctave:      return s.pendingChord.octave;
        case ParamId::ChordMode:        return static_cast<int>(s.pendingChord.play_mode);
        case ParamId::ChordVoicing:     return static_cast<int>(s.pendingChord.voicing_mode);
        case ParamId::ChordDuration:    return s.pendingChord.note_duration_ms;
        case ParamId::ChordVelocity:    return s.pendingChord.velocity;
        case ParamId::ChordPan:         return s.pendingChord.pan;
        case ParamId::ChordRoll:        return s.pendingChord.chord_roll_ms;
        case ParamId::ChordMinNotes:    return s.pendingChord.min_notes;
        case ParamId::ChordMinInterval: return s.pendingChord.min_interval;
        case ParamId::ChordInversion:   return static_cast<int>(s.pendingChord.inversion);
        case ParamId::ChordArpMode:     return static_cast<int>(s.pendingChord.arp_mode);
        case ParamId::StrumOctave:      return s.pendingStrum.octave;
        case ParamId::StrumDuration:    return s.pendingStrum.note_duration_ms;
        case ParamId::StrumVelocity:    return s.pendingStrum.velocity;
        case ParamId::StrumLayout:      return s.pendingStrum.limited_keys ? 1 : 0;
        case ParamId::StrumMode:        return static_cast<int>(s.pendingStrum.mode);
        case ParamId::StrumRoot:        return s.pendingStrum.root_pc;
        case ParamId::StrumScale:       return static_cast<int>(s.pendingStrum.scale_type);
        case ParamId::RhythmTempo:      return s.pendingRhythm.tempo;
        case ParamId::RhythmSwing:      return s.pendingRhythm.swing;
        case ParamId::RhythmPattern:    return s.pendingRhythm.pattern;
        case ParamId::RhythmMute:       return s.pendingRhythm.muted ? 1 : 0;
        case ParamId::RhythmEnable:     return s.pendingRhythm.enabled ? 1 : 0;
        case ParamId::RhythmClock:      return s.config.midi_clock_enabled ? 1 : 0;
        case ParamId::RhythmLed:        return s.config.bpm_indicator ? 1 : 0;
        case ParamId::UsbMidi:          return s.config.usb_midi_enabled ? 1 : 0;
        case ParamId::BassEnable:       return s.pendingBass.enabled ? 1 : 0;
        case ParamId::BassOctave:       return s.pendingBass.octave;
        case ParamId::BassDuration:     return s.pendingBass.note_duration_ms;
        case ParamId::BassVelocity:     return s.pendingBass.velocity;
        case ParamId::BassChannel:      return s.pendingBass.channel;
        case ParamId::BassPattern:      return static_cast<int>(s.pendingBass.pattern);
        default:                        return 0;
    }
}

} // namespace

void paramSet(StateManager& state, ParamId id, int value) {
    switch (id) {
        case ParamId::ChordOctave:
            state.pendingChord.octave = static_cast<int8_t>(clamp<int>(
                value, param_bounds::CHORD_OCTAVE_MIN, param_bounds::CHORD_OCTAVE_MAX));
            break;
        case ParamId::ChordMode:
            state.pendingChord.play_mode = static_cast<PlayMode>(clamp<int>(
                value, 0, static_cast<int>(PlayMode::COUNT) - 1));
            break;
        case ParamId::ChordVoicing:
            state.pendingChord.voicing_mode = static_cast<VoicingMode>(clamp<int>(
                value, 0, static_cast<int>(VoicingMode::COUNT) - 1));
            break;
        case ParamId::ChordDuration:
            state.pendingChord.note_duration_ms = static_cast<int16_t>(clamp<int>(
                value, param_bounds::CHORD_NOTE_DURATION_MIN, param_bounds::CHORD_NOTE_DURATION_MAX));
            break;
        case ParamId::ChordVelocity:
            state.pendingChord.velocity = static_cast<uint8_t>(clamp<int>(
                value, param_bounds::CHORD_VELOCITY_MIN, param_bounds::CHORD_VELOCITY_MAX));
            break;
        case ParamId::ChordPan:
            state.pendingChord.pan = static_cast<uint8_t>(clamp<int>(
                value, param_bounds::CHORD_PAN_MIN, param_bounds::CHORD_PAN_MAX));
            break;
        case ParamId::ChordRoll:
            state.pendingChord.chord_roll_ms = static_cast<int16_t>(clamp<int>(
                value, param_bounds::CHORD_ROLL_MIN, param_bounds::CHORD_ROLL_MAX));
            break;
        case ParamId::ChordMinNotes:
            state.pendingChord.min_notes = static_cast<uint8_t>(clamp<int>(
                value, param_bounds::CHORD_MIN_NOTES_MIN, param_bounds::CHORD_MIN_NOTES_MAX));
            break;
        case ParamId::ChordMinInterval:
            state.pendingChord.min_interval = static_cast<uint8_t>(clamp<int>(
                value, param_bounds::CHORD_MIN_INTERVAL_MIN, param_bounds::CHORD_MIN_INTERVAL_MAX));
            break;
        case ParamId::ChordInversion:
            state.pendingChord.inversion = static_cast<InversionMode>(clamp<int>(
                value, 0, static_cast<int>(InversionMode::COUNT) - 1));
            break;
        case ParamId::ChordArpMode:
            state.pendingChord.arp_mode = static_cast<ArpMode>(clamp<int>(
                value, 0, static_cast<int>(ArpMode::COUNT) - 1));
            break;

        case ParamId::StrumOctave:
            state.pendingStrum.octave = static_cast<int8_t>(clamp<int>(
                value, param_bounds::STRUM_OCTAVE_MIN, param_bounds::STRUM_OCTAVE_MAX));
            break;
        case ParamId::StrumDuration:
            state.pendingStrum.note_duration_ms = static_cast<int16_t>(clamp<int>(
                value, param_bounds::STRUM_NOTE_DURATION_MIN, param_bounds::STRUM_NOTE_DURATION_MAX));
            break;
        case ParamId::StrumVelocity:
            state.pendingStrum.velocity = static_cast<uint8_t>(clamp<int>(
                value, param_bounds::STRUM_VELOCITY_MIN, param_bounds::STRUM_VELOCITY_MAX));
            break;
        case ParamId::StrumLayout: state.pendingStrum.limited_keys = value != 0; break;
        case ParamId::StrumMode:
            state.pendingStrum.mode = static_cast<StrumMode>(clamp<int>(
                value, 0, static_cast<int>(StrumMode::COUNT) - 1));
            break;
        case ParamId::StrumRoot:
            state.pendingStrum.root_pc = static_cast<uint8_t>(clamp<int>(
                value, param_bounds::STRUM_ROOT_MIN, param_bounds::STRUM_ROOT_MAX));
            break;
        case ParamId::StrumScale:
            state.pendingStrum.scale_type = static_cast<ScaleType>(clamp<int>(
                value, 0, static_cast<int>(ScaleType::COUNT) - 1));
            break;

        case ParamId::RhythmTempo:
            state.pendingRhythm.tempo = static_cast<uint16_t>(clamp<int>(
                value, param_bounds::TEMPO_MIN, param_bounds::TEMPO_MAX));
            break;
        case ParamId::RhythmSwing:
            state.pendingRhythm.swing = static_cast<int8_t>(clamp<int>(
                value, param_bounds::SWING_MIN, param_bounds::SWING_MAX));
            break;
        case ParamId::RhythmPattern:
            state.pendingRhythm.pattern = static_cast<uint8_t>(clamp<int>(
                value, param_bounds::RHYTHM_PATTERN_MIN, rhythmCount() - 1));
            break;
        case ParamId::RhythmMute:   state.pendingRhythm.muted   = value != 0; break;
        case ParamId::RhythmEnable: state.pendingRhythm.enabled = value != 0; break;
        case ParamId::RhythmClock:  state.config.midi_clock_enabled = value != 0; break;
        case ParamId::RhythmLed:    state.config.bpm_indicator       = value != 0; break;
        case ParamId::UsbMidi:      state.config.usb_midi_enabled    = value != 0; break;

        case ParamId::BassEnable:   state.pendingBass.enabled = value != 0; break;
        case ParamId::BassOctave:
            state.pendingBass.octave = static_cast<int8_t>(clamp<int>(
                value, param_bounds::BASS_OCTAVE_MIN, param_bounds::BASS_OCTAVE_MAX));
            break;
        case ParamId::BassDuration:
            state.pendingBass.note_duration_ms = static_cast<int16_t>(clamp<int>(
                value, param_bounds::BASS_NOTE_DURATION_MIN, param_bounds::BASS_NOTE_DURATION_MAX));
            break;
        case ParamId::BassVelocity:
            state.pendingBass.velocity = static_cast<uint8_t>(clamp<int>(
                value, param_bounds::BASS_VELOCITY_MIN, param_bounds::BASS_VELOCITY_MAX));
            break;
        case ParamId::BassChannel:
            state.pendingBass.channel = static_cast<uint8_t>(clamp<int>(
                value, param_bounds::BASS_CHANNEL_MIN, param_bounds::BASS_CHANNEL_MAX));
            break;
        case ParamId::BassPattern:
            state.pendingBass.pattern = static_cast<uint8_t>(clamp<int>(
                value, 0, bassCount() - 1));
            break;

        default: break;
    }
}

void paramToggle(StateManager& state, ParamId id, int valueA, int valueB) {
    int cur = paramIntValue(state, id);
    paramSet(state, id, (cur == valueA) ? valueB : valueA);
}
