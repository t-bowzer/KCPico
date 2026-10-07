"""Chord pattern model (``/chordpatterns/*.json``).

Mirrors ``keybchord/lib/core/chordpattern.h`` / ``chordpattern.cpp``.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any

from .bounds import clamp

# Chord-degree bits (bitmask per step).
CHORD_DEGREE_ROOT = 0x01
CHORD_DEGREE_THIRD = 0x02
CHORD_DEGREE_FIFTH = 0x04
CHORD_DEGREE_SEVENTH = 0x08
CHORD_MASK_SEVENTH = 0x0F

# Built-in pattern names/files (chordpattern.cpp), in index order.
CHORD_PATTERN_NAMES: tuple[str, ...] = (
    "Block", "Alberti", "Broken Up", "Broken Down", "Pulse",
    "Oom-Pah", "Roll", "Split", "Arpeggio",
)
CHORD_PATTERN_FILES: tuple[str, ...] = (
    "block.json", "alberti.json", "broken_up.json", "broken_down.json",
    "pulse.json", "oom_pah.json", "roll.json", "split.json", "arpeggio.json",
)


@dataclass
class ChordPattern:
    name: str = ""
    steps_per_bar: int = 16
    steps: list[int] = field(default_factory=list)
    sustain_steps: list[int] = field(default_factory=list)

    @classmethod
    def from_dict(cls, data: Any) -> "ChordPattern":
        p = cls()
        if not isinstance(data, dict):
            return p
        name = data.get("name")
        if isinstance(name, str):
            p.name = name
        steps = data.get("steps_per_bar")
        if isinstance(steps, int):
            p.steps_per_bar = steps
        step_data = data.get("steps")
        if isinstance(step_data, list):
            p.steps = [
                clamp(int(v), 0, CHORD_MASK_SEVENTH) if isinstance(v, int) else 0
                for v in step_data
            ]
        sustain = data.get("sustain_steps")
        if isinstance(sustain, list):
            p.sustain_steps = [
                clamp(int(v), 0, 64) if isinstance(v, int) else 0 for v in sustain
            ]
        return p

    def to_dict(self) -> dict:
        out: dict = {
            "name": self.name,
            "steps_per_bar": self.steps_per_bar,
            "steps": list(self.steps),
        }
        if self.sustain_steps:
            out["sustain_steps"] = list(self.sustain_steps)
        return out

    def valid(self) -> bool:
        return bool(self.steps)


def chord_pattern_index(names: list[str], name: str) -> int:
    for i, n in enumerate(names):
        if n == name:
            return i
    return -1
