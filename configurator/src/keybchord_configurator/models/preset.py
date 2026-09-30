"""Preset model (``/presets/bankN.json`` — arrays of 8 slots).

Mirrors ``keybchord/lib/core/presets.cpp`` and ``params.h``. Enum names use the
preset JSON spellings (``press_to_play``, ``root_position``, ``follow_chord``),
which differ from the keymap short names.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any

from .bounds import (
    BASS_COUNT,
    CHANNEL_MAX,
    CHANNEL_MIN,
    CHORD_MIN_INTERVAL_MAX,
    CHORD_MIN_INTERVAL_MIN,
    CHORD_MIN_NOTES_MAX,
    CHORD_MIN_NOTES_MIN,
    CHORD_ROLL_MAX,
    CHORD_ROLL_MIN,
    DRUM_NOTE_MAX,
    DRUM_NOTE_MIN,
    DRUM_VELOCITY_MAX,
    DRUM_VELOCITY_MIN,
    NOTE_DURATION_MAX,
    NOTE_DURATION_MIN,
    OCTAVE_MAX,
    OCTAVE_MIN,
    PAN_MAX,
    PAN_MIN,
    RHYTHM_COUNT,
    STRUM_ROOT_MAX,
    STRUM_ROOT_MIN,
    SWING_MAX,
    SWING_MIN,
    TEMPO_MAX,
    TEMPO_MIN,
    VELOCITY_MAX,
    VELOCITY_MIN,
    clamp,
)
from .enums import (
    ArpMode,
    InversionMode,
    PlayMode,
    ScaleType,
    StrumMode,
    VoicingMode,
    ARP_MODE_JSON,
    INVERSION_JSON,
    PLAY_MODE_JSON,
    SCALE_TYPE_JSON,
    STRUM_MODE_JSON,
    VOICING_MODE_JSON,
    parse_arp_mode,
    parse_inversion,
    parse_play_mode,
    parse_scale_type,
    parse_strum_mode,
    parse_voicing_mode,
)

# (json key, display label, default GM note) for the 13 percussion pieces.
DRUM_PIECES: tuple[tuple[str, str, int], ...] = (
    ("kick", "Kick", 36),
    ("snare", "Snare", 38),
    ("hihat", "Hi-Hat", 42),
    ("open_hat", "Open Hat", 46),
    ("rimshot", "Rimshot", 37),
    ("clap", "Clap", 39),
    ("crash", "Crash", 49),
    ("ride", "Ride", 51),
    ("bongo", "Bongo", 61),
    ("conga_lo", "Conga Low", 62),
    ("conga_hi", "Conga High", 63),
    ("clave", "Clave", 75),
    ("shaker", "Shaker", 82),
)


def _int(value, minimum, maximum, fallback):
    try:
        return clamp(int(value), minimum, maximum)
    except (TypeError, ValueError):
        return fallback


def _bool(value, fallback):
    if isinstance(value, bool):
        return value
    return fallback


@dataclass
class DrumMap:
    """Per-preset GM drum-kit note remap + velocity override."""

    notes: dict[str, int] = field(default_factory=dict)
    velocities: dict[str, int] = field(default_factory=dict)

    @classmethod
    def defaults(cls) -> "DrumMap":
        return cls(
            notes={key: default for key, _label, default in DRUM_PIECES},
            velocities={key: 0 for key, _label, _default in DRUM_PIECES},
        )

    @classmethod
    def from_dict(cls, data: Any) -> "DrumMap":
        dm = cls.defaults()
        if not isinstance(data, dict):
            return dm
        for key, _label, default in DRUM_PIECES:
            if key in data:
                dm.notes[key] = _int(data[key], DRUM_NOTE_MIN, DRUM_NOTE_MAX, default)
            vel_key = key + "_vel"
            if vel_key in data:
                dm.velocities[key] = _int(
                    data[vel_key], DRUM_VELOCITY_MIN, 128, dm.velocities[key]
                )
        return dm

    def to_dict(self) -> dict:
        out: dict = {}
        for key, _label, _default in DRUM_PIECES:
            out[key] = self.notes[key]
            out[key + "_vel"] = self.velocities[key]
        return out

    def note_for(self, key: str) -> int:
        return self.notes[key]

    def velocity_for(self, key: str) -> int:
        return self.velocities[key]


@dataclass
class ChordParams:
    play_mode: PlayMode = PlayMode.HELD
    octave: int = 0
    note_duration_ms: int = 500
    velocity: int = 100
    pan: int = 64
    voicing_mode: VoicingMode = VoicingMode.ROOT_POSITION
    chord_roll_ms: int = 0
    min_notes: int = 3
    min_interval: int = 0
    inversion: InversionMode = InversionMode.ROOT
    arp_mode: ArpMode = ArpMode.UP
    channel: int = 1

    @classmethod
    def defaults(cls) -> "ChordParams":
        return cls()

    @classmethod
    def from_dict(cls, data: Any) -> "ChordParams":
        p = cls()
        if not isinstance(data, dict):
            return p
        p.play_mode = parse_play_mode(data.get("play_mode"))
        p.octave = _int(data.get("octave"), OCTAVE_MIN, OCTAVE_MAX, p.octave)
        p.note_duration_ms = _int(
            data.get("note_duration_ms"), NOTE_DURATION_MIN, NOTE_DURATION_MAX,
            p.note_duration_ms,
        )
        p.velocity = _int(data.get("velocity"), VELOCITY_MIN, VELOCITY_MAX, p.velocity)
        p.pan = _int(data.get("pan"), PAN_MIN, PAN_MAX, p.pan)
        p.voicing_mode = parse_voicing_mode(data.get("voicing_mode"))
        p.chord_roll_ms = _int(
            data.get("chord_roll_ms"), CHORD_ROLL_MIN, CHORD_ROLL_MAX, p.chord_roll_ms
        )
        p.min_notes = _int(
            data.get("min_notes"), CHORD_MIN_NOTES_MIN, CHORD_MIN_NOTES_MAX, p.min_notes
        )
        p.min_interval = _int(
            data.get("min_interval"), CHORD_MIN_INTERVAL_MIN, CHORD_MIN_INTERVAL_MAX,
            p.min_interval,
        )
        p.inversion = parse_inversion(data.get("inversion"))
        p.arp_mode = parse_arp_mode(data.get("arp_mode"))
        p.channel = _int(data.get("channel"), CHANNEL_MIN, CHANNEL_MAX, p.channel)
        return p

    def to_dict(self) -> dict:
        return {
            "play_mode": PLAY_MODE_JSON[self.play_mode],
            "octave": self.octave,
            "note_duration_ms": self.note_duration_ms,
            "velocity": self.velocity,
            "pan": self.pan,
            "voicing_mode": VOICING_MODE_JSON[self.voicing_mode],
            "chord_roll_ms": self.chord_roll_ms,
            "min_notes": self.min_notes,
            "min_interval": self.min_interval,
            "inversion": INVERSION_JSON[self.inversion],
            "arp_mode": ARP_MODE_JSON[self.arp_mode],
            "channel": self.channel,
        }


@dataclass
class StrumParams:
    octave: int = 1
    note_duration_ms: int = 300
    velocity: int = 90
    limited_keys: bool = False
    mode: StrumMode = StrumMode.FOLLOW_CHORD
    root_pc: int = 0
    scale_type: ScaleType = ScaleType.IONIAN
    channel: int = 2

    @classmethod
    def defaults(cls) -> "StrumParams":
        return cls()

    @classmethod
    def from_dict(cls, data: Any) -> "StrumParams":
        p = cls()
        if not isinstance(data, dict):
            return p
        p.octave = _int(data.get("octave"), OCTAVE_MIN, OCTAVE_MAX, p.octave)
        p.note_duration_ms = _int(
            data.get("note_duration_ms"), NOTE_DURATION_MIN, NOTE_DURATION_MAX,
            p.note_duration_ms,
        )
        p.velocity = _int(data.get("velocity"), VELOCITY_MIN, VELOCITY_MAX, p.velocity)
        p.limited_keys = _bool(data.get("limited_keys"), p.limited_keys)
        p.mode = parse_strum_mode(data.get("mode"))
        p.root_pc = _int(data.get("root_pc"), STRUM_ROOT_MIN, STRUM_ROOT_MAX, p.root_pc)
        p.scale_type = parse_scale_type(data.get("scale_type"))
        p.channel = _int(data.get("channel"), CHANNEL_MIN, CHANNEL_MAX, p.channel)
        return p

    def to_dict(self) -> dict:
        return {
            "octave": self.octave,
            "note_duration_ms": self.note_duration_ms,
            "velocity": self.velocity,
            "limited_keys": self.limited_keys,
            "mode": STRUM_MODE_JSON[self.mode],
            "root_pc": self.root_pc,
            "scale_type": SCALE_TYPE_JSON[self.scale_type],
            "channel": self.channel,
        }


@dataclass
class BassParams:
    enabled: bool = False
    octave: int = -1
    note_duration_ms: int = 150
    velocity: int = 90
    channel: int = 3
    pattern: str = "Walking"

    @classmethod
    def defaults(cls) -> "BassParams":
        return cls()

    @classmethod
    def from_dict(cls, data: Any) -> "BassParams":
        p = cls()
        if not isinstance(data, dict):
            return p
        p.enabled = _bool(data.get("enabled"), p.enabled)
        p.octave = _int(data.get("octave"), OCTAVE_MIN, OCTAVE_MAX, p.octave)
        p.note_duration_ms = _int(
            data.get("note_duration_ms"), NOTE_DURATION_MIN, NOTE_DURATION_MAX,
            p.note_duration_ms,
        )
        p.velocity = _int(data.get("velocity"), VELOCITY_MIN, VELOCITY_MAX, p.velocity)
        p.channel = _int(data.get("channel"), CHANNEL_MIN, CHANNEL_MAX, p.channel)
        pattern = data.get("pattern")
        if isinstance(pattern, str):
            p.pattern = pattern
        return p

    def to_dict(self) -> dict:
        return {
            "enabled": self.enabled,
            "octave": self.octave,
            "note_duration_ms": self.note_duration_ms,
            "velocity": self.velocity,
            "channel": self.channel,
            "pattern": self.pattern,
        }


@dataclass
class RhythmParams:
    enabled: bool = False
    pattern: str = "Rock 1"
    tempo: int = 120
    swing: int = 0
    muted: bool = False
    channel: int = 10
    drums: DrumMap = field(default_factory=DrumMap.defaults)

    @classmethod
    def defaults(cls) -> "RhythmParams":
        return cls()

    @classmethod
    def from_dict(cls, data: Any) -> "RhythmParams":
        p = cls()
        if not isinstance(data, dict):
            return p
        p.enabled = _bool(data.get("enabled"), p.enabled)
        pattern = data.get("pattern")
        if isinstance(pattern, str):
            p.pattern = pattern
        p.tempo = _int(data.get("tempo"), TEMPO_MIN, TEMPO_MAX, p.tempo)
        p.swing = _int(data.get("swing"), SWING_MIN, SWING_MAX, p.swing)
        p.muted = _bool(data.get("muted"), p.muted)
        p.channel = _int(data.get("channel"), CHANNEL_MIN, CHANNEL_MAX, p.channel)
        p.drums = DrumMap.from_dict(data.get("drums"))
        return p

    def to_dict(self) -> dict:
        return {
            "enabled": self.enabled,
            "pattern": self.pattern,
            "tempo": self.tempo,
            "swing": self.swing,
            "muted": self.muted,
            "channel": self.channel,
            "drums": self.drums.to_dict(),
        }


@dataclass
class PresetSlot:
    name: str = "Default"
    chord: ChordParams = field(default_factory=ChordParams.defaults)
    strum: StrumParams = field(default_factory=StrumParams.defaults)
    bass: BassParams = field(default_factory=BassParams.defaults)
    rhythm: RhythmParams = field(default_factory=RhythmParams.defaults)

    @classmethod
    def defaults(cls) -> "PresetSlot":
        return cls()

    @classmethod
    def from_dict(cls, data: Any) -> "PresetSlot":
        p = cls()
        if not isinstance(data, dict):
            return p
        name = data.get("name")
        if isinstance(name, str):
            p.name = name
        p.chord = ChordParams.from_dict(data.get("chord"))
        p.strum = StrumParams.from_dict(data.get("strum"))
        p.bass = BassParams.from_dict(data.get("bass"))
        p.rhythm = RhythmParams.from_dict(data.get("rhythm"))
        return p

    def to_dict(self) -> dict:
        return {
            "name": self.name,
            "chord": self.chord.to_dict(),
            "strum": self.strum.to_dict(),
            "bass": self.bass.to_dict(),
            "rhythm": self.rhythm.to_dict(),
        }

    def display_name(self, bank: int, slot: int) -> str:
        if self.name and self.name != "Default":
            return self.name
        return f"B{bank + 1}:P{slot + 1}"
