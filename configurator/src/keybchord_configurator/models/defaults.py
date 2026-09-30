"""Built-in rhythm and bass patterns.

These mirror the firmware's shipped defaults in ``defaults.cpp`` and are used to
seed a fresh configuration directory and to drive the editors' default
templates. They match the patterns the firmware provisions to ``/rhythms/`` and
``/bass/`` on first boot.
"""

from __future__ import annotations

_HAT = [1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0]

# (name, steps_per_bar, swing, [(note, track_name, pattern), ...])
BUILTIN_RHYTHMS: tuple[dict, ...] = (
    {
        "name": "Rock 1",
        "steps_per_bar": 16,
        "swing": 0,
        "tracks": [
            {"note": 36, "name": "kick", "pattern": [1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0]},
            {"note": 38, "name": "snare", "pattern": [0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0]},
            {"note": 42, "name": "hihat", "pattern": list(_HAT)},
        ],
    },
    {
        "name": "Rock 2",
        "steps_per_bar": 16,
        "swing": 0,
        "tracks": [
            {"note": 36, "name": "kick", "pattern": [1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0]},
            {"note": 38, "name": "snare", "pattern": [0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0]},
            {"note": 42, "name": "hihat", "pattern": list(_HAT)},
            {"note": 46, "name": "open_hat", "pattern": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0]},
        ],
    },
    {
        "name": "Waltz",
        "steps_per_bar": 12,
        "swing": 0,
        "tracks": [
            {"note": 36, "name": "kick", "pattern": [1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]},
            {"note": 38, "name": "snare", "pattern": [0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0]},
            {"note": 42, "name": "hihat", "pattern": [1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0]},
        ],
    },
    {
        "name": "Swing",
        "steps_per_bar": 16,
        "swing": 50,
        "tracks": [
            {"note": 36, "name": "kick", "pattern": [1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0]},
            {"note": 38, "name": "snare", "pattern": [0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0]},
            {"note": 42, "name": "hihat", "pattern": list(_HAT)},
        ],
    },
    {
        "name": "Slow Rock",
        "steps_per_bar": 16,
        "swing": 25,
        "tracks": [
            {"note": 36, "name": "kick", "pattern": [1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]},
            {"note": 38, "name": "snare", "pattern": [0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0]},
            {"note": 42, "name": "hihat", "pattern": list(_HAT)},
        ],
    },
    {
        "name": "Bossa Nova",
        "steps_per_bar": 16,
        "swing": 0,
        "tracks": [
            {"note": 36, "name": "kick", "pattern": [1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0]},
            {"note": 37, "name": "rimshot", "pattern": [0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0]},
            {"note": 51, "name": "ride", "pattern": list(_HAT)},
            {"note": 82, "name": "shaker", "pattern": [0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1]},
        ],
    },
    {
        "name": "Rhumba",
        "steps_per_bar": 16,
        "swing": 0,
        "tracks": [
            {"note": 36, "name": "kick", "pattern": [1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0]},
            {"note": 62, "name": "conga_lo", "pattern": [0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0]},
            {"note": 63, "name": "conga_hi", "pattern": [0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1]},
            {"note": 75, "name": "clave", "pattern": [1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0]},
        ],
    },
    {
        "name": "Tango",
        "steps_per_bar": 16,
        "swing": 0,
        "tracks": [
            {"note": 36, "name": "kick", "pattern": [1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0]},
            {"note": 38, "name": "snare", "pattern": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0]},
            {"note": 42, "name": "hihat", "pattern": list(_HAT)},
            {"note": 39, "name": "clap", "pattern": [0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1]},
        ],
    },
    {
        "name": "March",
        "steps_per_bar": 16,
        "swing": 0,
        "tracks": [
            {"note": 36, "name": "kick", "pattern": [1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0]},
            {"note": 38, "name": "snare", "pattern": [0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0]},
            {"note": 42, "name": "hihat", "pattern": list(_HAT)},
            {"note": 49, "name": "crash", "pattern": [1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]},
        ],
    },
    {
        "name": "Samba",
        "steps_per_bar": 16,
        "swing": 0,
        "tracks": [
            {"note": 36, "name": "kick", "pattern": [1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0]},
            {"note": 38, "name": "snare", "pattern": [0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0]},
            {"note": 82, "name": "shaker", "pattern": list(_HAT)},
            {"note": 61, "name": "bongo", "pattern": [0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1]},
        ],
    },
    {
        "name": "Disco",
        "steps_per_bar": 16,
        "swing": 0,
        "tracks": [
            {"note": 36, "name": "kick", "pattern": [1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0]},
            {"note": 38, "name": "snare", "pattern": [0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0]},
            {"note": 42, "name": "hihat", "pattern": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]},
            {"note": 46, "name": "open_hat", "pattern": [0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0]},
        ],
    },
    {
        "name": "Foxtrot",
        "steps_per_bar": 16,
        "swing": 25,
        "tracks": [
            {"note": 36, "name": "kick", "pattern": [1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0]},
            {"note": 38, "name": "snare", "pattern": [0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0]},
            {"note": 42, "name": "hihat", "pattern": list(_HAT)},
            {"note": 51, "name": "ride", "pattern": [0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0]},
        ],
    },
)

# (name, steps_per_bar, steps, sustain_steps?, hold?)
BUILTIN_BASS: tuple[dict, ...] = (
    {
        "name": "Walking",
        "steps_per_bar": 16,
        "steps": [0, -1, -1, -1, 1, -1, -1, -1, 2, -1, -1, -1, 3, -1, -1, -1],
    },
    {
        "name": "Whole",
        "steps_per_bar": 16,
        "steps": [0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1],
        "sustain_steps": [16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    },
    {
        "name": "Half",
        "steps_per_bar": 16,
        "steps": [0, -1, -1, -1, -1, -1, -1, -1, 0, -1, -1, -1, -1, -1, -1, -1],
        "sustain_steps": [8, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0],
    },
    {
        "name": "Quarter",
        "steps_per_bar": 16,
        "steps": [0, -1, -1, -1, 0, -1, -1, -1, 0, -1, -1, -1, 0, -1, -1, -1],
    },
    {
        "name": "Half Alt",
        "steps_per_bar": 16,
        "steps": [0, -1, -1, -1, -1, -1, -1, -1, 2, -1, -1, -1, -1, -1, -1, -1],
        "sustain_steps": [8, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0],
    },
    {
        "name": "Quarter Alt",
        "steps_per_bar": 16,
        "steps": [0, -1, -1, -1, 2, -1, -1, -1, 0, -1, -1, -1, 2, -1, -1, -1],
    },
    {
        "name": "3/4 Alt",
        "steps_per_bar": 16,
        "steps": [0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2, -1, -1, -1],
    },
    {
        "name": "Hold",
        "steps_per_bar": 16,
        "hold": True,
        "steps": [0],
    },
    {
        "name": "No 6th",
        "steps_per_bar": 16,
        "steps": [0, -1, -1, -1, -1, -1, -1, -1, 1, -1, -1, -1, 2, -1, -1, -1],
        "sustain_steps": [8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    },
)


def default_rhythm_template() -> dict:
    """A blank 16-step template for a new rhythm pattern."""
    return {
        "name": "New Rhythm",
        "short_name": "",
        "steps_per_bar": 16,
        "swing": 0,
        "tracks": [
            {"note": 36, "name": "kick", "pattern": [0] * 16},
            {"note": 38, "name": "snare", "pattern": [0] * 16},
            {"note": 42, "name": "hihat", "pattern": [0] * 16},
        ],
    }


def default_bass_template() -> dict:
    """A blank 16-step template for a new bass pattern."""
    return {
        "name": "New Bass",
        "steps_per_bar": 16,
        "steps": [-1] * 16,
        "sustain_steps": [0] * 16,
    }
