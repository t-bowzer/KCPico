"""Rhythm pattern model (``/rhythms/*.json``).

Mirrors ``keybchord/lib/core/rhythm.h`` / ``rhythm.cpp``.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any

from .bounds import SWING_MAX, SWING_MIN, clamp

RHYTHM_STEPS_PER_BEAT = 4
RHYTHM_DEFAULT_VELOCITY = 100

# Built-in pattern names/files (rhythm.cpp), in index order.
RHYTHM_NAMES: tuple[str, ...] = (
    "Rock 1", "Rock 2", "Waltz", "Swing", "Slow Rock", "Bossa Nova",
    "Rhumba", "Tango", "March", "Samba", "Disco", "Foxtrot",
)
RHYTHM_FILES: tuple[str, ...] = (
    "rock1.json", "rock2.json", "waltz.json", "swing.json",
    "slow_rock.json", "bossa_nova.json", "rhumba.json", "tango.json",
    "march.json", "samba.json", "disco.json", "foxtrot.json",
)


@dataclass
class RhythmTrack:
    note: int = 0
    name: str = ""
    pattern: list[int] = field(default_factory=list)

    @classmethod
    def from_dict(cls, data: Any) -> "RhythmTrack":
        t = cls()
        if not isinstance(data, dict):
            return t
        note = data.get("note")
        if isinstance(note, int):
            t.note = clamp(note, 0, 127)
        name = data.get("name")
        if isinstance(name, str):
            t.name = name
        pattern = data.get("pattern")
        if isinstance(pattern, list):
            t.pattern = [
                clamp(int(v), 0, 127) if isinstance(v, int) else 0 for v in pattern
            ]
        return t

    def to_dict(self) -> dict:
        return {"note": self.note, "name": self.name, "pattern": list(self.pattern)}


@dataclass
class RhythmPattern:
    name: str = ""
    short_name: str = ""
    steps_per_bar: int = 16
    swing: int = 0
    tracks: list[RhythmTrack] = field(default_factory=list)

    @classmethod
    def from_dict(cls, data: Any) -> "RhythmPattern":
        p = cls()
        if not isinstance(data, dict):
            return p
        name = data.get("name")
        if isinstance(name, str):
            p.name = name
        short_name = data.get("short_name")
        if isinstance(short_name, str):
            p.short_name = short_name
        steps = data.get("steps_per_bar")
        if isinstance(steps, int):
            p.steps_per_bar = steps
        swing = data.get("swing")
        if isinstance(swing, int):
            p.swing = clamp(swing, SWING_MIN, SWING_MAX)
        tracks = data.get("tracks")
        if isinstance(tracks, list):
            p.tracks = [RhythmTrack.from_dict(t) for t in tracks if isinstance(t, dict)]
        return p

    def to_dict(self) -> dict:
        out: dict = {
            "name": self.name,
            "steps_per_bar": self.steps_per_bar,
            "swing": self.swing,
            "tracks": [t.to_dict() for t in self.tracks],
        }
        if self.short_name:
            out["short_name"] = self.short_name
        return out

    def beats_per_bar(self) -> int:
        return max(1, self.steps_per_bar // RHYTHM_STEPS_PER_BEAT)

    def has_tracks(self) -> bool:
        return bool(self.tracks)


def rhythm_index(names: list[str], name: str) -> int:
    """Resolve a pattern name to an index in ``names`` (or -1)."""
    for i, n in enumerate(names):
        if n == name:
            return i
    return -1


def rhythm_display_name(pattern: RhythmPattern, index: int, names: list[str]) -> str:
    """Name shown in the UI (authored name, or filename-based fallback)."""
    if pattern.name:
        return pattern.name
    if index < len(names):
        return names[index]
    return f"Pattern {index + 1}"
