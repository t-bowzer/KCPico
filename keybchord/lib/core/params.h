#pragma once

#include <cstdint>


enum class PlayMode : uint8_t {
    Held = 0,
    PressToPlay,
    Arpeggio,
    Silent,
    COUNT
};

enum class VoicingMode : uint8_t {
    RootPosition = 0,
    Smart,
    COUNT
};

enum class InversionMode : uint8_t {
    Root = 0,
    First,
    Second,
    Third,
    COUNT
};

enum class ArpMode : uint8_t {
    Up = 0,
    Down,
    UpDown,
    Alternating,
    Random,
    COUNT
};

// Strum note-pool source (Upgrade-Plan "Strum engine upgrades").
enum class StrumMode : uint8_t {
    FollowChord = 0,   // default: active chord's pitch classes
    Scale,             // selectable root + scale/mode
    Piano,             // selectable root, chromatic (each key = +1 semitone)
    COUNT
};

// Scale/mode choices for strum Scale mode (Upgrade-Plan): standard
// major/minor/harmonic/melodic + the 7 modes + pentatonic + blues. Interval
// tables live in strum.cpp.
enum class ScaleType : uint8_t {
    Ionian = 0,          // major
    Dorian,
    Phrygian,
    Lydian,
    Mixolydian,
    Aeolian,             // natural minor
    Locrian,
    HarmonicMinor,
    MelodicMinor,
    MajorPentatonic,
    MinorPentatonic,
    Blues,
    COUNT
};

// Parameter-edit menu (FR-D2). None = main menu (direct single-key shortcuts);
// otherwise the user is inside a Chord/Strum/Rhythm/Bass/Drum edit menu where
// F-keys select a parameter and +/- change its value.
enum class EditMenu : uint8_t {
    None = 0,
    Chord,
    Strum,
    Rhythm,
    Bass,
    Drum,
    COUNT
};

struct ChordParams {
    PlayMode      play_mode        = PlayMode::Held;
    int8_t        octave           = 0;
    int16_t       note_duration_ms = 500;
    uint8_t       velocity         = 100;
    uint8_t       pan              = 64;
    VoicingMode   voicing_mode     = VoicingMode::RootPosition;
    int16_t       chord_roll_ms    = 0;    // signed; + = ascending, - = descending
    uint8_t       min_notes        = 3;    // pad with octave notes to >= this count
    uint8_t       min_interval     = 0;    // min semitones between adjacent notes (0 = off)
    InversionMode inversion        = InversionMode::Root;
    ArpMode       arp_mode         = ArpMode::Up;
    uint8_t       channel          = 1;

    static ChordParams defaults();
};

struct StrumParams {
    int8_t    octave           = 1;
    int16_t   note_duration_ms = 300;
    uint8_t   velocity         = 90;
    bool      limited_keys     = false;
    StrumMode mode             = StrumMode::FollowChord;
    uint8_t   root_pc          = 0;             // scale/piano root pitch class (0 = C)
    ScaleType scale_type       = ScaleType::Ionian;
    uint8_t   channel          = 2;

    static StrumParams defaults();
};

// Per-preset GM drum-kit mapping (FR-R3) plus per-piece velocity override
// (Upgrade-Plan "Rhythm engine upgrades"). A velocity of 0 means "follow the
// pattern's authored step velocity"; 1..127 forces a fixed velocity; 128
// (DRUM_VELOCITY_OFF) mutes the piece.
struct DrumMap {
    uint8_t kick           = 36;   // Bass Drum 1
    uint8_t kick_vel       = 0;
    uint8_t snare          = 38;   // Acoustic Snare
    uint8_t snare_vel      = 0;
    uint8_t hihat          = 42;   // Closed Hi-Hat
    uint8_t hihat_vel      = 0;
    uint8_t open_hat       = 46;   // Open Hi-Hat
    uint8_t open_hat_vel   = 0;
    uint8_t rimshot        = 37;   // Side Stick
    uint8_t rimshot_vel    = 0;
    uint8_t clap           = 39;   // Hand Clap
    uint8_t clap_vel       = 0;
    uint8_t crash          = 49;   // Crash Cymbal 1
    uint8_t crash_vel      = 0;
    uint8_t ride           = 51;   // Ride Cymbal 1
    uint8_t ride_vel       = 0;
    uint8_t bongo          = 61;   // Low Bongo
    uint8_t bongo_vel      = 0;
    uint8_t conga_lo       = 62;   // Mute Hi Conga
    uint8_t conga_lo_vel   = 0;
    uint8_t conga_hi       = 63;   // Open Hi Conga
    uint8_t conga_hi_vel   = 0;
    uint8_t clave          = 75;   // Claves
    uint8_t clave_vel      = 0;
    uint8_t shaker         = 82;   // Shaker
    uint8_t shaker_vel     = 0;

    bool operator==(const DrumMap& o) const {
        return kick == o.kick && kick_vel == o.kick_vel &&
               snare == o.snare && snare_vel == o.snare_vel &&
               hihat == o.hihat && hihat_vel == o.hihat_vel &&
               open_hat == o.open_hat && open_hat_vel == o.open_hat_vel &&
               rimshot == o.rimshot && rimshot_vel == o.rimshot_vel &&
               clap == o.clap && clap_vel == o.clap_vel &&
               crash == o.crash && crash_vel == o.crash_vel &&
               ride == o.ride && ride_vel == o.ride_vel &&
               bongo == o.bongo && bongo_vel == o.bongo_vel &&
               conga_lo == o.conga_lo && conga_lo_vel == o.conga_lo_vel &&
               conga_hi == o.conga_hi && conga_hi_vel == o.conga_hi_vel &&
               clave == o.clave && clave_vel == o.clave_vel &&
               shaker == o.shaker && shaker_vel == o.shaker_vel;
    }
};

struct RhythmParams {
    bool     enabled = false;
    uint16_t tempo   = 120;
    int8_t   swing   = 0;    // signed: + = laid-back, - = rushed (-75..+75)
    bool     muted   = false;
    uint8_t  channel = 10;
    uint8_t  pattern = 0;    // index into the rhythm list (0..RHYTHM_COUNT-1)
    DrumMap  drums;          // GM drum-code + velocity mapping for kick/snare/hats

    static RhythmParams defaults();
};

// Walking-bass pattern (Upgrade-Plan): the default interval cycle plus a set of
// user-selectable root/5th patterns. `Hold` sustains the root while the chord
// is sounding rather than being beat-driven.
enum class BassPattern : uint8_t {
    Walking = 0,   // root-3rd-5th-6th/7th cycle on each beat (spec 6.8)
    Whole,         // root, whole notes
    Half,          // root, half notes
    Quarter,       // root, quarter notes
    HalfAlt,       // root/5th alternating half notes
    QuarterAlt,    // root/5th alternating quarter notes
    ThreeFourAlt,  // root on beat 1, 5th on the last beat
    Hold,          // root sustained while the chord is held
    WalkNoSixth,   // root (half) -> 3rd (quarter) -> 5th (quarter) -> repeat
    COUNT
};

struct BassParams {
    bool        enabled          = false;
    int8_t      octave           = -1;
    int16_t     note_duration_ms = 150;
    uint8_t     velocity         = 90;
    uint8_t     channel          = 3;
    BassPattern pattern          = BassPattern::Walking;

    static BassParams defaults();
};

struct GlobalParams {
    uint8_t  base_root_midi   = 60;
    uint8_t  note_range_low   = 48;
    uint8_t  note_range_high  = 84;
    uint16_t display_revert_ms = 1500;
    uint16_t display_prompt_ms = 5000;
    bool     bpm_indicator       = true;
    uint8_t  led_flash_ms        = 40;
    bool     accent_downbeat     = true;
    uint8_t  accent_flash_ms     = 80;
    bool     led_only_when_rhythm = true;
    bool     midi_clock_enabled  = false;

    static GlobalParams defaults();
};

namespace param_bounds {

constexpr int16_t CHORD_NOTE_DURATION_MIN = 50;
constexpr int16_t CHORD_NOTE_DURATION_MAX = 4000;
constexpr uint8_t CHORD_VELOCITY_MIN = 1;
constexpr uint8_t CHORD_VELOCITY_MAX = 127;
constexpr uint8_t CHORD_PAN_MIN = 0;
constexpr uint8_t CHORD_PAN_MAX = 127;
constexpr int8_t  CHORD_OCTAVE_MIN = -3;
constexpr int8_t  CHORD_OCTAVE_MAX = 3;
constexpr uint8_t CHORD_CHANNEL_MIN = 1;
constexpr uint8_t CHORD_CHANNEL_MAX = 16;
constexpr int16_t CHORD_ROLL_MIN = -2000;
constexpr int16_t CHORD_ROLL_MAX = 2000;
constexpr int16_t CHORD_ROLL_STEP = 10;
constexpr uint8_t CHORD_MIN_NOTES_MIN = 2;
constexpr uint8_t CHORD_MIN_NOTES_MAX = 6;
constexpr uint8_t CHORD_MIN_INTERVAL_MIN = 0;
constexpr uint8_t CHORD_MIN_INTERVAL_MAX = 12;

constexpr int16_t STRUM_NOTE_DURATION_MIN = 50;
constexpr int16_t STRUM_NOTE_DURATION_MAX = 4000;
constexpr int8_t  STRUM_OCTAVE_MIN = -3;
constexpr int8_t  STRUM_OCTAVE_MAX = 3;
constexpr uint8_t STRUM_VELOCITY_MIN = 1;
constexpr uint8_t STRUM_VELOCITY_MAX = 127;
constexpr uint8_t STRUM_CHANNEL_MIN = 1;
constexpr uint8_t STRUM_CHANNEL_MAX = 16;
constexpr uint8_t STRUM_ROOT_MIN = 0;
constexpr uint8_t STRUM_ROOT_MAX = 11;

constexpr uint16_t TEMPO_MIN = 40;
constexpr uint16_t TEMPO_MAX = 260;
constexpr int8_t   SWING_MIN = -75;
constexpr int8_t   SWING_MAX = 75;
constexpr uint8_t  RHYTHM_CHANNEL_MIN = 1;
constexpr uint8_t  RHYTHM_CHANNEL_MAX = 16;
constexpr uint8_t  RHYTHM_PATTERN_MIN = 0;
constexpr uint8_t  RHYTHM_PATTERN_MAX = 11;  // RHYTHM_COUNT - 1

constexpr int16_t BASS_NOTE_DURATION_MIN = 50;
constexpr int16_t BASS_NOTE_DURATION_MAX = 4000;
constexpr int8_t  BASS_OCTAVE_MIN = -3;
constexpr int8_t  BASS_OCTAVE_MAX = 3;
constexpr uint8_t BASS_VELOCITY_MIN = 1;
constexpr uint8_t BASS_VELOCITY_MAX = 127;
constexpr uint8_t BASS_CHANNEL_MIN = 1;
constexpr uint8_t BASS_CHANNEL_MAX = 16;

constexpr uint8_t DRUM_NOTE_MIN = 0;
constexpr uint8_t DRUM_NOTE_MAX = 127;
constexpr uint8_t DRUM_VELOCITY_MIN = 0;  // 0 = follow pattern velocity
constexpr uint8_t DRUM_VELOCITY_MAX = 127;
constexpr uint8_t DRUM_VELOCITY_OFF = 128;  // mute the piece entirely

constexpr uint8_t MIDI_NOTE_MIN = 0;
constexpr uint8_t MIDI_NOTE_MAX = 127;

constexpr int16_t NOTE_DURATION_MIN = 50;
constexpr int16_t NOTE_DURATION_MAX = 4000;

} // namespace param_bounds

template<typename T>
inline T clamp(T value, T min, T max) {
    return (value < min) ? min : ((value > max) ? max : value);
}
