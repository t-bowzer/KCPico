"""Numeric bounds and parameter metadata.

Mirrors ``keybchord/lib/core/params.h`` (the ``param_bounds`` namespace) and the
``config.json`` bounds in ``config.cpp``. This module is the single source of
truth for both the JSON validator and the bounds-aware UI widgets.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Optional

from .enums import (
    ArpMode,
    EditMenu,
    InversionMode,
    PlayMode,
    ScaleType,
    StrumMode,
    VoicingMode,
)

# --- Global config bounds (config.cpp) -------------------------------------
BASE_ROOT_MIDI_MIN, BASE_ROOT_MIDI_MAX = 0, 127
NOTE_RANGE_MIN, NOTE_RANGE_MAX = 0, 127
DISPLAY_REVERT_MS_MIN, DISPLAY_REVERT_MS_MAX = 250, 5000
DISPLAY_PROMPT_MS_MIN, DISPLAY_PROMPT_MS_MAX = 1000, 30000
DISPLAY_CURSOR_MS_MIN, DISPLAY_CURSOR_MS_MAX = 500, 30000
DISPLAY_MENU_MS_MIN, DISPLAY_MENU_MS_MAX = 500, 30000
LED_FLASH_MS_MIN, LED_FLASH_MS_MAX = 5, 500

# --- param_bounds (params.h) -----------------------------------------------
NOTE_DURATION_MIN, NOTE_DURATION_MAX = 50, 4000
NOTE_DURATION_STEP = 50
VELOCITY_MIN, VELOCITY_MAX = 1, 127
PAN_MIN, PAN_MAX = 0, 127
OCTAVE_MIN, OCTAVE_MAX = -3, 3
CHANNEL_MIN, CHANNEL_MAX = 1, 16
CHORD_ROLL_MIN, CHORD_ROLL_MAX = -2000, 2000
CHORD_ROLL_STEP = 10
CHORD_MIN_NOTES_MIN, CHORD_MIN_NOTES_MAX = 2, 6
CHORD_MIN_INTERVAL_MIN, CHORD_MIN_INTERVAL_MAX = 0, 12
STRUM_ROOT_MIN, STRUM_ROOT_MAX = 0, 11
TEMPO_MIN, TEMPO_MAX = 40, 260
SWING_MIN, SWING_MAX = -75, 75
SWING_STEP = 5
DRUM_NOTE_MIN, DRUM_NOTE_MAX = 0, 127
DRUM_VELOCITY_MIN, DRUM_VELOCITY_MAX = 0, 127
DRUM_VELOCITY_OFF = 128  # mutes the piece entirely

# Pattern library caps (rhythm.h / bass.h).
RHYTHM_COUNT = 12
BASS_COUNT = 9
MAX_RHYTHMS = 32
MAX_BASS_PATTERNS = 32

NUM_BANKS = 10
NUM_SLOTS = 8


@dataclass(frozen=True)
class ParamSpec:
    """Metadata describing one editable parameter.

    ``key`` uses the firmware's keymap param name. ``kind`` is one of
    ``"int"``, ``"bool"``, ``"enum"``, or ``"pattern"``. For enum params,
    ``values`` maps a display label to its integer value; for pattern params
    the choices are resolved at runtime against the loaded libraries.
    """

    key: str
    label: str
    group: str
    kind: str
    minimum: Optional[int] = None
    maximum: Optional[int] = None
    step: Optional[int] = None
    values: Optional[dict[str, int]] = None


def _enum_values(*pairs: tuple[str, object]) -> dict[str, int]:
    return {label: int(member) for label, member in pairs}


PARAM_SPECS: dict[str, ParamSpec] = {}


def _add(*specs: ParamSpec) -> None:
    for spec in specs:
        PARAM_SPECS[spec.key] = spec


_add(
    # Chord
    ParamSpec("chord_octave", "Octave", "Chord", "int", OCTAVE_MIN, OCTAVE_MAX, 1),
    ParamSpec(
        "chord_mode",
        "Play mode",
        "Chord",
        "enum",
        values=_enum_values(
            ("Held", PlayMode.HELD),
            ("Press-to-play", PlayMode.PRESS_TO_PLAY),
            ("Arpeggio", PlayMode.ARPEGGIO),
            ("Arp-hold", PlayMode.ARP_HOLD),
            ("Silent", PlayMode.SILENT),
        ),
    ),
    ParamSpec(
        "chord_voicing",
        "Voicing",
        "Chord",
        "enum",
        values=_enum_values(
            ("Root position", VoicingMode.ROOT_POSITION),
            ("Smart", VoicingMode.SMART),
            ("Down", VoicingMode.DOWN),
            ("Up", VoicingMode.UP),
        ),
    ),
    ParamSpec("chord_duration", "Note duration (ms)", "Chord", "int",
              NOTE_DURATION_MIN, NOTE_DURATION_MAX, NOTE_DURATION_STEP),
    ParamSpec("chord_velocity", "Velocity", "Chord", "int", VELOCITY_MIN, VELOCITY_MAX, 1),
    ParamSpec("chord_pan", "Pan (CC10)", "Chord", "int", PAN_MIN, PAN_MAX, 1),
    ParamSpec("chord_roll", "Chord roll (ms)", "Chord", "int",
              CHORD_ROLL_MIN, CHORD_ROLL_MAX, CHORD_ROLL_STEP),
    ParamSpec("chord_min_notes", "Min notes", "Chord", "int",
              CHORD_MIN_NOTES_MIN, CHORD_MIN_NOTES_MAX, 1),
    ParamSpec("chord_min_interval", "Min interval", "Chord", "int",
              CHORD_MIN_INTERVAL_MIN, CHORD_MIN_INTERVAL_MAX, 1),
    ParamSpec(
        "chord_inversion",
        "Inversion",
        "Chord",
        "enum",
        values=_enum_values(
            ("Root", InversionMode.ROOT),
            ("First", InversionMode.FIRST),
            ("Second", InversionMode.SECOND),
            ("Third", InversionMode.THIRD),
        ),
    ),
    ParamSpec(
        "arp_mode",
        "Arpeggio pattern",
        "Chord",
        "enum",
        values=_enum_values(
            ("Up", ArpMode.UP),
            ("Down", ArpMode.DOWN),
            ("Up-down", ArpMode.UP_DOWN),
            ("Alternating", ArpMode.ALTERNATING),
            ("Random", ArpMode.RANDOM),
        ),
    ),
    # Strum
    ParamSpec("strum_octave", "Octave", "Strum", "int", OCTAVE_MIN, OCTAVE_MAX, 1),
    ParamSpec("strum_duration", "Note duration (ms)", "Strum", "int",
              NOTE_DURATION_MIN, NOTE_DURATION_MAX, NOTE_DURATION_STEP),
    ParamSpec("strum_velocity", "Velocity", "Strum", "int", VELOCITY_MIN, VELOCITY_MAX, 1),
    ParamSpec("strum_layout", "Limited keys", "Strum", "bool"),
    ParamSpec(
        "strum_mode",
        "Mode",
        "Strum",
        "enum",
        values=_enum_values(
            ("Follow chord", StrumMode.FOLLOW_CHORD),
            ("Scale", StrumMode.SCALE),
            ("Piano", StrumMode.PIANO),
        ),
    ),
    ParamSpec("strum_root", "Root pitch class", "Strum", "int",
              STRUM_ROOT_MIN, STRUM_ROOT_MAX, 1),
    ParamSpec(
        "strum_scale",
        "Scale / mode",
        "Strum",
        "enum",
        values=_enum_values(
            ("Ionian", ScaleType.IONIAN),
            ("Dorian", ScaleType.DORIAN),
            ("Phrygian", ScaleType.PHRYGIAN),
            ("Lydian", ScaleType.LYDIAN),
            ("Mixolydian", ScaleType.MIXOLYDIAN),
            ("Aeolian", ScaleType.AEOLIAN),
            ("Locrian", ScaleType.LOCRIAN),
            ("Harmonic minor", ScaleType.HARMONIC_MINOR),
            ("Melodic minor", ScaleType.MELODIC_MINOR),
            ("Major pentatonic", ScaleType.MAJOR_PENTATONIC),
            ("Minor pentatonic", ScaleType.MINOR_PENTATONIC),
            ("Blues", ScaleType.BLUES),
        ),
    ),
    # Rhythm
    ParamSpec("tempo", "Tempo (BPM)", "Rhythm", "int", TEMPO_MIN, TEMPO_MAX, 1),
    ParamSpec("swing", "Swing", "Rhythm", "int", SWING_MIN, SWING_MAX, SWING_STEP),
    ParamSpec("rhythm_pattern", "Pattern", "Rhythm", "pattern"),
    ParamSpec("rhythm_mute", "Muted", "Rhythm", "bool"),
    ParamSpec("rhythm_enable", "Enabled", "Rhythm", "bool"),
    ParamSpec("rhythm_clock", "MIDI clock", "Rhythm", "bool"),
    ParamSpec("rhythm_led", "LED indicator", "Rhythm", "bool"),
    # Bass
    ParamSpec("bass_enable", "Enabled", "Bass", "bool"),
    ParamSpec("bass_octave", "Octave", "Bass", "int", OCTAVE_MIN, OCTAVE_MAX, 1),
    ParamSpec("bass_duration", "Note duration (ms)", "Bass", "int",
              NOTE_DURATION_MIN, NOTE_DURATION_MAX, NOTE_DURATION_STEP),
    ParamSpec("bass_velocity", "Velocity", "Bass", "int", VELOCITY_MIN, VELOCITY_MAX, 1),
    ParamSpec("bass_channel", "Channel", "Bass", "int", CHANNEL_MIN, CHANNEL_MAX, 1),
    ParamSpec("bass_pattern", "Pattern", "Bass", "pattern"),
    # Drum (per-piece; not keymap-bindable but part of ParamId)
    ParamSpec("drum_kick", "Kick", "Drum", "int", DRUM_NOTE_MIN, DRUM_NOTE_MAX, 1),
    ParamSpec("drum_kick_vel", "Kick velocity", "Drum", "int",
              DRUM_VELOCITY_MIN, DRUM_VELOCITY_OFF, 1),
    ParamSpec("drum_snare", "Snare", "Drum", "int", DRUM_NOTE_MIN, DRUM_NOTE_MAX, 1),
    ParamSpec("drum_snare_vel", "Snare velocity", "Drum", "int",
              DRUM_VELOCITY_MIN, DRUM_VELOCITY_OFF, 1),
    ParamSpec("drum_hihat", "Hi-hat", "Drum", "int", DRUM_NOTE_MIN, DRUM_NOTE_MAX, 1),
    ParamSpec("drum_hihat_vel", "Hi-hat velocity", "Drum", "int",
              DRUM_VELOCITY_MIN, DRUM_VELOCITY_OFF, 1),
    ParamSpec("drum_open_hat", "Open hat", "Drum", "int", DRUM_NOTE_MIN, DRUM_NOTE_MAX, 1),
    ParamSpec("drum_open_hat_vel", "Open hat velocity", "Drum", "int",
              DRUM_VELOCITY_MIN, DRUM_VELOCITY_OFF, 1),
    ParamSpec("drum_rimshot", "Rimshot", "Drum", "int", DRUM_NOTE_MIN, DRUM_NOTE_MAX, 1),
    ParamSpec("drum_rimshot_vel", "Rimshot velocity", "Drum", "int",
              DRUM_VELOCITY_MIN, DRUM_VELOCITY_OFF, 1),
    ParamSpec("drum_clap", "Clap", "Drum", "int", DRUM_NOTE_MIN, DRUM_NOTE_MAX, 1),
    ParamSpec("drum_clap_vel", "Clap velocity", "Drum", "int",
              DRUM_VELOCITY_MIN, DRUM_VELOCITY_OFF, 1),
    ParamSpec("drum_crash", "Crash", "Drum", "int", DRUM_NOTE_MIN, DRUM_NOTE_MAX, 1),
    ParamSpec("drum_crash_vel", "Crash velocity", "Drum", "int",
              DRUM_VELOCITY_MIN, DRUM_VELOCITY_OFF, 1),
    ParamSpec("drum_ride", "Ride", "Drum", "int", DRUM_NOTE_MIN, DRUM_NOTE_MAX, 1),
    ParamSpec("drum_ride_vel", "Ride velocity", "Drum", "int",
              DRUM_VELOCITY_MIN, DRUM_VELOCITY_OFF, 1),
    ParamSpec("drum_bongo", "Bongo", "Drum", "int", DRUM_NOTE_MIN, DRUM_NOTE_MAX, 1),
    ParamSpec("drum_bongo_vel", "Bongo velocity", "Drum", "int",
              DRUM_VELOCITY_MIN, DRUM_VELOCITY_OFF, 1),
    ParamSpec("drum_conga_lo", "Conga low", "Drum", "int", DRUM_NOTE_MIN, DRUM_NOTE_MAX, 1),
    ParamSpec("drum_conga_lo_vel", "Conga low velocity", "Drum", "int",
              DRUM_VELOCITY_MIN, DRUM_VELOCITY_OFF, 1),
    ParamSpec("drum_conga_hi", "Conga high", "Drum", "int", DRUM_NOTE_MIN, DRUM_NOTE_MAX, 1),
    ParamSpec("drum_conga_hi_vel", "Conga high velocity", "Drum", "int",
              DRUM_VELOCITY_MIN, DRUM_VELOCITY_OFF, 1),
    ParamSpec("drum_clave", "Clave", "Drum", "int", DRUM_NOTE_MIN, DRUM_NOTE_MAX, 1),
    ParamSpec("drum_clave_vel", "Clave velocity", "Drum", "int",
              DRUM_VELOCITY_MIN, DRUM_VELOCITY_OFF, 1),
    ParamSpec("drum_shaker", "Shaker", "Drum", "int", DRUM_NOTE_MIN, DRUM_NOTE_MAX, 1),
    ParamSpec("drum_shaker_vel", "Shaker velocity", "Drum", "int",
              DRUM_VELOCITY_MIN, DRUM_VELOCITY_OFF, 1),
)


def clamp(value: int, minimum: int, maximum: int) -> int:
    """Clamp an integer into ``[minimum, maximum]`` (mirrors the firmware)."""
    return max(minimum, min(value, maximum))
