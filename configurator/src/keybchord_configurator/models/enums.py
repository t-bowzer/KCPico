"""Enum types and their name mappings.

These mirror the firmware's enums in ``keybchord/lib/core/params.h`` and the
JSON name tables in ``presets.cpp`` / ``keymap_config.cpp``. Integer values are
kept identical so serialized files round-trip byte-for-byte with the firmware's
integer indices.
"""

from __future__ import annotations

from enum import IntEnum


class PlayMode(IntEnum):
    """Chord play modes (``chord.play_mode``)."""

    HELD = 0
    PRESS_TO_PLAY = 1
    ARPEGGIO = 2
    ARP_HOLD = 3
    SILENT = 4
    PATTERN = 5
    PATTERN_HOLD = 6


class VoicingMode(IntEnum):
    """Chord voicing modes (``chord.voicing_mode``)."""

    ROOT_POSITION = 0
    SMART = 1
    DOWN = 2
    UP = 3


class InversionMode(IntEnum):
    """Chord inversions (``chord.inversion``)."""

    ROOT = 0
    FIRST = 1
    SECOND = 2
    THIRD = 3


class ArpMode(IntEnum):
    """Arpeggio patterns (``chord.arp_mode``)."""

    UP = 0
    DOWN = 1
    UP_DOWN = 2
    ALTERNATING = 3
    RANDOM = 4


class ArpRate(IntEnum):
    """Arpeggiator note subdivision (``chord.arp_rate``)."""

    WHOLE = 0
    HALF = 1
    QUARTER = 2
    EIGHTH = 3
    SIXTEENTH = 4
    THIRTY_SECOND = 5


class StrumMode(IntEnum):
    """Strum note-pool source (``strum.mode``)."""

    FOLLOW_CHORD = 0
    SCALE = 1
    PIANO = 2


class ScaleType(IntEnum):
    """Strum scale/mode choices (``strum.scale_type``)."""

    IONIAN = 0
    DORIAN = 1
    PHRYGIAN = 2
    LYDIAN = 3
    MIXOLYDIAN = 4
    AEOLIAN = 5
    LOCRIAN = 6
    HARMONIC_MINOR = 7
    MELODIC_MINOR = 8
    MAJOR_PENTATONIC = 9
    MINOR_PENTATONIC = 10
    BLUES = 11


class LedTarget(IntEnum):
    """Which keyboard LED(s) the BPM indicator flashes (``led.led``)."""

    ALL = 0
    SCROLL_LOCK = 1
    CAPS_LOCK = 2
    NUM_LOCK = 3


class EditMenu(IntEnum):
    """Edit menus reachable from the keymap (``open_menu``)."""

    CHORD = 0
    STRUM = 1
    RHYTHM = 2
    BASS = 3
    DRUM = 4
    SEQUENCE = 5
    CHORD_PATTERN = 6
    RHYTHM_PATTERN = 7
    BASS_PATTERN = 8


class NumberKeyMode(IntEnum):
    """Per-preset top-row number-key behaviour."""

    STRUM = 0
    MACRO = 1


class ChordQuality(IntEnum):
    """Chord grid row quality."""

    MAJOR = 0
    MINOR = 1
    SEVENTH = 2


class ChordType(IntEnum):
    """Resolved chord type (firmware ``ChordType`` order)."""

    MAJOR = 0
    MINOR = 1
    DOM7 = 2
    MAJ7 = 3
    MIN7 = 4
    DIM = 5
    DIM7 = 6
    AUG = 7
    SUS4 = 8
    SUS2 = 9
    MIN7B5 = 10


# JSON (preset) names. Keys are the enum members, values are the string the
# firmware writes/reads in ``presets.json`` files.
PLAY_MODE_JSON: dict[PlayMode, str] = {
    PlayMode.HELD: "held",
    PlayMode.PRESS_TO_PLAY: "press_to_play",
    PlayMode.ARPEGGIO: "arpeggio",
    PlayMode.ARP_HOLD: "arp_hold",
    PlayMode.SILENT: "silent",
    PlayMode.PATTERN: "pattern",
    PlayMode.PATTERN_HOLD: "pattern_hold",
}

VOICING_MODE_JSON: dict[VoicingMode, str] = {
    VoicingMode.ROOT_POSITION: "root_position",
    VoicingMode.SMART: "smart",
    VoicingMode.DOWN: "down",
    VoicingMode.UP: "up",
}

INVERSION_JSON: dict[InversionMode, str] = {
    InversionMode.ROOT: "root",
    InversionMode.FIRST: "first",
    InversionMode.SECOND: "second",
    InversionMode.THIRD: "third",
}

ARP_MODE_JSON: dict[ArpMode, str] = {
    ArpMode.UP: "up",
    ArpMode.DOWN: "down",
    ArpMode.UP_DOWN: "up_down",
    ArpMode.ALTERNATING: "alternating",
    ArpMode.RANDOM: "random",
}

ARP_RATE_JSON: dict[ArpRate, str] = {
    ArpRate.WHOLE: "whole",
    ArpRate.HALF: "half",
    ArpRate.QUARTER: "quarter",
    ArpRate.EIGHTH: "eighth",
    ArpRate.SIXTEENTH: "sixteenth",
    ArpRate.THIRTY_SECOND: "thirty_second",
}

STRUM_MODE_JSON: dict[StrumMode, str] = {
    StrumMode.FOLLOW_CHORD: "follow_chord",
    StrumMode.SCALE: "scale",
    StrumMode.PIANO: "piano",
}

SCALE_TYPE_JSON: dict[ScaleType, str] = {
    ScaleType.IONIAN: "ionian",
    ScaleType.DORIAN: "dorian",
    ScaleType.PHRYGIAN: "phrygian",
    ScaleType.LYDIAN: "lydian",
    ScaleType.MIXOLYDIAN: "mixolydian",
    ScaleType.AEOLIAN: "aeolian",
    ScaleType.LOCRIAN: "locrian",
    ScaleType.HARMONIC_MINOR: "harmonic_minor",
    ScaleType.MELODIC_MINOR: "melodic_minor",
    ScaleType.MAJOR_PENTATONIC: "major_pentatonic",
    ScaleType.MINOR_PENTATONIC: "minor_pentatonic",
    ScaleType.BLUES: "blues",
}

LED_TARGET_JSON: dict[LedTarget, str] = {
    LedTarget.ALL: "all",
    LedTarget.SCROLL_LOCK: "scroll_lock",
    LedTarget.CAPS_LOCK: "caps_lock",
    LedTarget.NUM_LOCK: "num_lock",
}

NUMBER_KEY_MODE_JSON: dict[NumberKeyMode, str] = {
    NumberKeyMode.STRUM: "strum",
    NumberKeyMode.MACRO: "macro",
}

EDIT_MENU_JSON: dict[EditMenu, str] = {
    EditMenu.CHORD: "chord",
    EditMenu.STRUM: "strum",
    EditMenu.RHYTHM: "rhythm",
    EditMenu.BASS: "bass",
    EditMenu.DRUM: "drum",
    EditMenu.SEQUENCE: "sequence",
    EditMenu.CHORD_PATTERN: "chord_pattern",
    EditMenu.RHYTHM_PATTERN: "rhythm_pattern",
    EditMenu.BASS_PATTERN: "bass_pattern",
}

CHORD_TYPE_JSON: dict[ChordType, str] = {
    ChordType.MAJOR: "major",
    ChordType.MINOR: "minor",
    ChordType.DOM7: "dom7",
    ChordType.MAJ7: "maj7",
    ChordType.MIN7: "min7",
    ChordType.DIM: "dim",
    ChordType.DIM7: "dim7",
    ChordType.AUG: "aug",
    ChordType.SUS4: "sus4",
    ChordType.SUS2: "sus2",
    ChordType.MIN7B5: "min7b5",
}


def _reverse(mapping: dict) -> dict[str, object]:
    return {v: k for k, v in mapping.items()}


PLAY_MODE_BY_JSON: dict[str, PlayMode] = _reverse(PLAY_MODE_JSON)
VOICING_MODE_BY_JSON: dict[str, VoicingMode] = _reverse(VOICING_MODE_JSON)
INVERSION_BY_JSON: dict[str, InversionMode] = _reverse(INVERSION_JSON)
ARP_MODE_BY_JSON: dict[str, ArpMode] = _reverse(ARP_MODE_JSON)
ARP_RATE_BY_JSON: dict[str, ArpRate] = _reverse(ARP_RATE_JSON)
STRUM_MODE_BY_JSON: dict[str, StrumMode] = _reverse(STRUM_MODE_JSON)
SCALE_TYPE_BY_JSON: dict[str, ScaleType] = _reverse(SCALE_TYPE_JSON)
LED_TARGET_BY_JSON: dict[str, LedTarget] = _reverse(LED_TARGET_JSON)
EDIT_MENU_BY_JSON: dict[str, EditMenu] = _reverse(EDIT_MENU_JSON)
NUMBER_KEY_MODE_BY_JSON: dict[str, NumberKeyMode] = _reverse(NUMBER_KEY_MODE_JSON)
CHORD_TYPE_BY_JSON: dict[str, ChordType] = _reverse(CHORD_TYPE_JSON)


def parse_enum(name_to_member: dict[str, object], value, default: object):
    """Parse a JSON enum string (or int) into an enum member, else ``default``."""
    if isinstance(value, int):
        for member in name_to_member.values():
            if int(member) == value:
                return member
        return default
    if isinstance(value, str):
        return name_to_member.get(value, default)
    return default


def parse_play_mode(value) -> PlayMode:
    return parse_enum(PLAY_MODE_BY_JSON, value, PlayMode.HELD)


def parse_voicing_mode(value) -> VoicingMode:
    return parse_enum(VOICING_MODE_BY_JSON, value, VoicingMode.ROOT_POSITION)


def parse_inversion(value) -> InversionMode:
    return parse_enum(INVERSION_BY_JSON, value, InversionMode.ROOT)


def parse_arp_mode(value) -> ArpMode:
    return parse_enum(ARP_MODE_BY_JSON, value, ArpMode.UP)


def parse_arp_rate(value) -> ArpRate:
    return parse_enum(ARP_RATE_BY_JSON, value, ArpRate.SIXTEENTH)


def parse_strum_mode(value) -> StrumMode:
    return parse_enum(STRUM_MODE_BY_JSON, value, StrumMode.FOLLOW_CHORD)


def parse_scale_type(value) -> ScaleType:
    return parse_enum(SCALE_TYPE_BY_JSON, value, ScaleType.IONIAN)


def parse_led_target(value) -> LedTarget:
    return parse_enum(LED_TARGET_BY_JSON, value, LedTarget.NUM_LOCK)


def parse_number_key_mode(value) -> NumberKeyMode:
    return parse_enum(NUMBER_KEY_MODE_BY_JSON, value, NumberKeyMode.STRUM)
