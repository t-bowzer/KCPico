#pragma once

#include <cstdint>


enum class PlayMode : uint8_t {
    Held = 0,
    PressToPlay,
    Arpeggio,
    Rhythm,
    Silent,
    COUNT
};

enum class VoicingMode : uint8_t {
    RootPosition = 0,
    Smart,
    COUNT
};

struct ChordParams {
    PlayMode    play_mode        = PlayMode::Held;
    int8_t      octave           = 0;
    int16_t     note_duration_ms = 500;
    uint8_t     velocity         = 100;
    uint8_t     pan              = 64;
    VoicingMode voicing_mode     = VoicingMode::RootPosition;
    bool        add9             = false;
    bool        add11            = false;
    bool        add13            = false;
    uint8_t     channel          = 1;

    static ChordParams defaults();
};

struct StrumParams {
    int8_t  octave           = 1;
    int16_t note_duration_ms = 300;
    uint8_t velocity         = 90;
    bool    limited_keys     = false;
    uint8_t channel          = 2;

    static StrumParams defaults();
};

struct RhythmParams {
    bool    enabled = false;
    uint16_t tempo  = 120;
    uint8_t  swing  = 0;
    bool    muted   = false;
    uint8_t channel = 10;

    static RhythmParams defaults();
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

constexpr int16_t STRUM_NOTE_DURATION_MIN = 50;
constexpr int16_t STRUM_NOTE_DURATION_MAX = 4000;
constexpr int8_t  STRUM_OCTAVE_MIN = -3;
constexpr int8_t  STRUM_OCTAVE_MAX = 3;
constexpr uint8_t STRUM_VELOCITY_MIN = 1;
constexpr uint8_t STRUM_VELOCITY_MAX = 127;
constexpr uint8_t STRUM_CHANNEL_MIN = 1;
constexpr uint8_t STRUM_CHANNEL_MAX = 16;

constexpr uint16_t TEMPO_MIN = 40;
constexpr uint16_t TEMPO_MAX = 260;
constexpr uint8_t  SWING_MIN = 0;
constexpr uint8_t  SWING_MAX = 75;
constexpr uint8_t  RHYTHM_CHANNEL_MIN = 1;
constexpr uint8_t  RHYTHM_CHANNEL_MAX = 16;

constexpr uint8_t MIDI_NOTE_MIN = 0;
constexpr uint8_t MIDI_NOTE_MAX = 127;

constexpr int16_t NOTE_DURATION_MIN = 50;
constexpr int16_t NOTE_DURATION_MAX = 4000;

} // namespace param_bounds

template<typename T>
inline T clamp(T value, T min, T max) {
    return (value < min) ? min : ((value > max) ? max : value);
}


