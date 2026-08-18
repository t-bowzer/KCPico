#include "params.h"


ChordParams ChordParams::defaults() {
    ChordParams p;
    p.play_mode        = PlayMode::Held;
    p.octave           = 0;
    p.note_duration_ms = 500;
    p.velocity         = 100;
    p.pan              = 64;
    p.voicing_mode     = VoicingMode::RootPosition;
    p.chord_roll_ms    = 0;
    p.min_notes        = 3;
    p.min_interval     = 0;
    p.inversion        = InversionMode::Root;
    p.arp_mode         = ArpMode::Up;
    p.channel          = 1;
    return p;
}

StrumParams StrumParams::defaults() {
    StrumParams p;
    p.octave           = 1;
    p.note_duration_ms = 300;
    p.velocity         = 90;
    p.limited_keys     = false;
    p.mode             = StrumMode::FollowChord;
    p.root_pc          = 0;
    p.scale_type       = ScaleType::Ionian;
    p.channel          = 2;
    return p;
}

RhythmParams RhythmParams::defaults() {
    RhythmParams p;
    p.enabled = false;
    p.tempo   = 120;
    p.swing   = 0;
    p.muted   = false;
    p.channel = 10;
    p.pattern = 0;
    p.drums   = DrumMap{};
    return p;
}

BassParams BassParams::defaults() {
    BassParams p;
    p.enabled          = false;
    p.octave           = -1;
    p.note_duration_ms = 150;
    p.velocity         = 90;
    p.channel          = 3;
    return p;
}

GlobalParams GlobalParams::defaults() {
    GlobalParams p;
    p.base_root_midi       = 60;
    p.note_range_low       = 48;
    p.note_range_high      = 84;
    p.display_revert_ms    = 1500;
    p.display_prompt_ms    = 5000;
    p.bpm_indicator        = true;
    p.led_flash_ms         = 40;
    p.accent_downbeat      = true;
    p.accent_flash_ms      = 80;
    p.led_only_when_rhythm = true;
    p.midi_clock_enabled   = false;
    return p;
}
