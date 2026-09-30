"""Bass pattern model (``/bass/*.json``).

Mirrors ``keybchord/lib/core/bass.h`` / ``bass.cpp``.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any

from .bounds import clamp

# Chord-degree codes.
BASS_DEGREE_REST = -1
BASS_DEGREE_ROOT = 0
BASS_DEGREE_THIRD = 1
BASS_DEGREE_FIFTH = 2
BASS_DEGREE_SIXTH_SEVENTH = 3

BASS_DEGREE_LABELS: dict[int, str] = {
    BASS_DEGREE_REST: "Rest",
    BASS_DEGREE_ROOT: "Root",
    BASS_DEGREE_THIRD: "3rd",
    BASS_DEGREE_FIFTH: "5th",
    BASS_DEGREE_SIXTH_SEVENTH: "6th/7th",
}

# Built-in pattern names/files (bass.cpp), in index order.
BASS_NAMES: tuple[str, ...] = (
    "Walking", "Whole", "Half", "Quarter", "Half Alt",
    "Quarter Alt", "3/4 Alt", "Hold", "No 6th",
)
BASS_FILES: tuple[str, ...] = (
    "walking.json", "whole.json", "half.json", "quarter.json",
    "half_alt.json", "quarter_alt.json", "three_four_alt.json",
    "hold.json", "walk_no_6th.json",
)


@dataclass
class BassPattern:
    name: str = ""
    steps_per_bar: int = 16
    steps: list[int] = field(default_factory=list)
    sustain_steps: list[int] = field(default_factory=list)
    hold: bool = False

    @classmethod
    def from_dict(cls, data: Any) -> "BassPattern":
        p = cls()
        if not isinstance(data, dict):
            return p
        name = data.get("name")
        if isinstance(name, str):
            p.name = name
        steps = data.get("steps_per_bar")
        if isinstance(steps, int):
            p.steps_per_bar = steps
        hold = data.get("hold")
        if isinstance(hold, bool):
            p.hold = hold
        step_data = data.get("steps")
        if isinstance(step_data, list):
            p.steps = [
                clamp(int(v), -1, 3) if isinstance(v, int) else BASS_DEGREE_REST
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
        if self.hold:
            out["hold"] = True
        return out

    def valid(self) -> bool:
        return bool(self.steps)


def bass_index(names: list[str], name: str) -> int:
    for i, n in enumerate(names):
        if n == name:
            return i
    return -1
