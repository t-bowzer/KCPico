"""Keymap model (``/keymap.json``).

Mirrors ``keybchord/lib/core/keymap_config.cpp`` and ``keymap.h``. The keymap
works entirely at the *name* level (the firmware resolves names to HID usages),
so the app needs no HID usage table.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Optional

from .enums import EditMenu, EDIT_MENU_BY_JSON, EDIT_MENU_JSON

# --- Key / modifier name tables (keymap_config.cpp) ------------------------
MODIFIERS: tuple[str, ...] = ("Ctrl", "Alt", "Super")

# Bindable (non-reserved) keys, in the firmware's table order.
BINDABLE_KEYS: tuple[str, ...] = (
    "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
    "PrtSc", "ScLk", "Pause",
    "Insert", "Home", "PgUp", "Delete", "End", "PgDn",
    "Space", "=", "-", "KpPlus", "KpMinus", "KpEnter",
    "Left", "Down", "Right", "Up",
)

# Keys the firmware's name table knows but that are reserved (number-row strum
# keys). They resolve to "key is reserved" rather than "unknown key".
RESERVED_NAMED_KEYS: frozenset[str] = frozenset(
    {"1", "2", "3", "4", "5", "6", "7", "8"}
)

# --- Parameter names (keymap_config.cpp kParamNames) -----------------------
KEYMAP_PARAM_NAMES: tuple[str, ...] = (
    "chord_octave", "chord_mode", "chord_voicing", "chord_duration",
    "chord_velocity", "chord_pan", "chord_roll", "chord_min_notes",
    "chord_min_interval", "chord_inversion", "arp_mode",
    "strum_octave", "strum_duration", "strum_velocity", "strum_layout",
    "strum_mode", "strum_root", "strum_scale",
    "tempo", "swing", "rhythm_pattern", "rhythm_mute", "rhythm_enable",
    "rhythm_clock", "rhythm_led",
    "bass_enable", "bass_octave", "bass_duration", "bass_velocity",
    "bass_channel", "bass_pattern",
)

# Parameters whose value is a boolean (accept on/off, true/false, 1/0).
BOOL_PARAMS: frozenset[str] = frozenset(
    {
        "strum_layout", "rhythm_mute", "rhythm_enable",
        "rhythm_clock", "rhythm_led", "bass_enable",
    }
)

# Enum short value tables (keymap.json spellings — differ from preset JSON for
# play mode, voicing, and strum mode).
PLAY_MODE_SHORT: dict[str, int] = {
    "held": 0, "press": 1, "arpeggio": 2, "arp_hold": 3, "silent": 4,
}
VOICING_SHORT: dict[str, int] = {"root": 0, "smart": 1, "down": 2, "up": 3}
INVERSION_SHORT: dict[str, int] = {"root": 0, "first": 1, "second": 2, "third": 3}
ARP_SHORT: dict[str, int] = {
    "up": 0, "down": 1, "up_down": 2, "alternating": 3, "random": 4,
}
STRUM_MODE_SHORT: dict[str, int] = {"chord": 0, "scale": 1, "piano": 2}
SCALE_SHORT: dict[str, int] = {
    "ionian": 0, "dorian": 1, "phrygian": 2, "lydian": 3,
    "mixolydian": 4, "aeolian": 5, "locrian": 6,
    "harmonic_minor": 7, "melodic_minor": 8,
    "major_pentatonic": 9, "minor_pentatonic": 10, "blues": 11,
}

# Which short table (or None) applies to a given param.
PARAM_VALUE_TABLE: dict[str, dict[str, int]] = {
    "chord_mode": PLAY_MODE_SHORT,
    "chord_voicing": VOICING_SHORT,
    "chord_inversion": INVERSION_SHORT,
    "arp_mode": ARP_SHORT,
    "strum_mode": STRUM_MODE_SHORT,
    "strum_scale": SCALE_SHORT,
}

# --- Action types ----------------------------------------------------------
ACTION_TYPES: tuple[str, ...] = (
    "open_menu", "cycle", "set", "inc", "dec", "toggle", "ext", "drum_mute",
    "preset_prev", "preset_next", "preset_bank_prev", "preset_bank_next",
    "preset_load", "preset_save", "preset_clear", "tap_tempo",
)

# Actions that carry a parameter.
PARAM_ACTIONS: frozenset[str] = frozenset({"cycle", "set", "inc", "dec", "toggle"})

# Required keymap coverage (keymap_config.cpp §4.5): every menu reachable, plus
# preset navigation/save.
REQUIRED_MENUS: tuple[EditMenu, ...] = (
    EditMenu.CHORD, EditMenu.STRUM, EditMenu.RHYTHM, EditMenu.BASS, EditMenu.DRUM,
)
REQUIRED_ACTIONS: tuple[str, ...] = ("preset_prev", "preset_next", "preset_save")


@dataclass
class KeyAction:
    type: str = "cycle"
    param: Optional[str] = None
    menu: Optional[str] = None
    value: Any = None  # int | str, for "set"
    a: Any = None      # int | str, for "toggle"
    b: Any = None      # int | str, for "toggle"
    slot: Optional[int] = None   # preset_load
    index: Optional[int] = None  # drum_mute

    @classmethod
    def from_dict(cls, data: Any) -> "KeyAction":
        a = cls()
        if not isinstance(data, dict):
            return a
        typ = data.get("type")
        if isinstance(typ, str):
            a.type = typ
        a.param = data.get("param") if isinstance(data.get("param"), str) else None
        a.menu = data.get("menu") if isinstance(data.get("menu"), str) else None
        a.value = data.get("value")
        a.a = data.get("a")
        a.b = data.get("b")
        slot = data.get("slot")
        a.slot = slot if isinstance(slot, int) else None
        index = data.get("index")
        a.index = index if isinstance(index, int) else None
        return a

    def to_dict(self) -> dict:
        out: dict = {"type": self.type}
        if self.param is not None:
            out["param"] = self.param
        if self.type == "open_menu" and self.menu is not None:
            out["menu"] = self.menu
        if self.type in ("set", "ext") and self.value is not None:
            out["value"] = self.value
        if self.type == "toggle":
            if self.a is not None:
                out["a"] = self.a
            if self.b is not None:
                out["b"] = self.b
        if self.type == "preset_load" and self.slot is not None:
            out["slot"] = self.slot
        if self.type == "drum_mute" and self.index is not None:
            out["index"] = self.index
        return out


@dataclass
class KeyBinding:
    modifier: Optional[str] = None  # "Ctrl" | "Alt" | "Super" | None
    key: str = "F1"
    action: KeyAction = field(default_factory=KeyAction)

    @classmethod
    def from_dict(cls, data: Any) -> "KeyBinding":
        b = cls()
        if not isinstance(data, dict):
            return b
        keys = data.get("keys")
        if isinstance(keys, list):
            for k in keys:
                if isinstance(k, str):
                    if k in MODIFIERS:
                        b.modifier = k
                    else:
                        b.key = k
        b.action = KeyAction.from_dict(data.get("action"))
        return b

    def to_dict(self) -> dict:
        keys = [self.modifier, self.key] if self.modifier else [self.key]
        return {"keys": keys, "action": self.action.to_dict()}


@dataclass
class KeymapConfig:
    bindings: list[KeyBinding] = field(default_factory=list)

    @classmethod
    def from_dict(cls, data: Any) -> "KeymapConfig":
        cfg = cls()
        if isinstance(data, dict):
            bindings = data.get("bindings")
            if isinstance(bindings, list):
                cfg.bindings = [
                    KeyBinding.from_dict(b) for b in bindings if isinstance(b, dict)
                ]
        return cfg

    def to_dict(self) -> dict:
        return {"bindings": [b.to_dict() for b in self.bindings]}


def default_keymap() -> KeymapConfig:
    """The canonical default keymap (mirrors ``KeymapConfig::defaults``)."""
    raw = [
        {"keys": ["F1"], "action": {"type": "cycle", "param": "chord_mode"}},
        {"keys": ["F2"], "action": {"type": "cycle", "param": "arp_mode"}},
        {"keys": ["F3"], "action": {"type": "cycle", "param": "bass_pattern"}},
        {"keys": ["F4"], "action": {"type": "cycle", "param": "rhythm_pattern"}},
        {"keys": ["F5"], "action": {"type": "toggle", "param": "rhythm_enable", "a": 1, "b": 0}},
        {"keys": ["F6"], "action": {"type": "toggle", "param": "rhythm_mute", "a": 1, "b": 0}},
        {"keys": ["F7"], "action": {"type": "toggle", "param": "bass_enable", "a": 1, "b": 0}},
        {"keys": ["F8"], "action": {"type": "toggle", "param": "rhythm_clock", "a": 1, "b": 0}},
        {"keys": ["F9"], "action": {"type": "open_menu", "menu": "chord"}},
        {"keys": ["F10"], "action": {"type": "open_menu", "menu": "strum"}},
        {"keys": ["F11"], "action": {"type": "open_menu", "menu": "rhythm"}},
        {"keys": ["F12"], "action": {"type": "open_menu", "menu": "bass"}},
        {"keys": ["PrtSc"], "action": {"type": "set", "param": "chord_inversion", "value": "first"}},
        {"keys": ["ScLk"], "action": {"type": "set", "param": "chord_inversion", "value": "second"}},
        {"keys": ["Pause"], "action": {"type": "set", "param": "chord_inversion", "value": "third"}},
        {"keys": ["="], "action": {"type": "inc", "param": "chord_octave"}},
        {"keys": ["-"], "action": {"type": "dec", "param": "chord_octave"}},
        {"keys": ["KpPlus"], "action": {"type": "inc", "param": "strum_octave"}},
        {"keys": ["KpMinus"], "action": {"type": "dec", "param": "strum_octave"}},
        {"keys": ["Left"], "action": {"type": "ext", "value": 9}},
        {"keys": ["Down"], "action": {"type": "ext", "value": 11}},
        {"keys": ["Right"], "action": {"type": "ext", "value": 13}},
        {"keys": ["PgUp"], "action": {"type": "inc", "param": "tempo"}},
        {"keys": ["PgDn"], "action": {"type": "dec", "param": "tempo"}},
        {"keys": ["Space"], "action": {"type": "tap_tempo"}},
        {"keys": ["Home"], "action": {"type": "preset_prev"}},
        {"keys": ["End"], "action": {"type": "preset_next"}},
        {"keys": ["Insert"], "action": {"type": "preset_save"}},
        {"keys": ["Delete"], "action": {"type": "preset_clear"}},
        {"keys": ["Super", "Home"], "action": {"type": "preset_bank_prev"}},
        {"keys": ["Super", "End"], "action": {"type": "preset_bank_next"}},
        {"keys": ["Super", "F11"], "action": {"type": "open_menu", "menu": "drum"}},
        {"keys": ["Ctrl", "F1"], "action": {"type": "cycle", "param": "chord_voicing"}},
        {"keys": ["Ctrl", "F2"], "action": {"type": "cycle", "param": "chord_inversion"}},
        {"keys": ["Ctrl", "F3"], "action": {"type": "cycle", "param": "arp_mode"}},
        {"keys": ["Ctrl", "F4"], "action": {"type": "toggle", "param": "chord_roll", "a": 0, "b": 50}},
        {"keys": ["Ctrl", "F5"], "action": {"type": "toggle", "param": "rhythm_led", "a": 1, "b": 0}},
        {"keys": ["Ctrl", "="], "action": {"type": "inc", "param": "chord_velocity"}},
        {"keys": ["Ctrl", "-"], "action": {"type": "dec", "param": "chord_velocity"}},
        {"keys": ["Ctrl", "KpEnter"], "action": {"type": "cycle", "param": "strum_layout"}},
        {"keys": ["Ctrl", "KpPlus"], "action": {"type": "inc", "param": "strum_scale"}},
        {"keys": ["Ctrl", "KpMinus"], "action": {"type": "dec", "param": "strum_scale"}},
        {"keys": ["Alt", "KpEnter"], "action": {"type": "cycle", "param": "strum_mode"}},
        {"keys": ["Alt", "KpPlus"], "action": {"type": "inc", "param": "strum_root"}},
        {"keys": ["Alt", "KpMinus"], "action": {"type": "dec", "param": "strum_root"}},
        {"keys": ["Alt", "="], "action": {"type": "inc", "param": "bass_octave"}},
        {"keys": ["Alt", "-"], "action": {"type": "dec", "param": "bass_octave"}},
        {"keys": ["Alt", "F1"], "action": {"type": "drum_mute", "index": 0}},
        {"keys": ["Alt", "F2"], "action": {"type": "drum_mute", "index": 1}},
        {"keys": ["Alt", "F3"], "action": {"type": "drum_mute", "index": 2}},
        {"keys": ["Alt", "F4"], "action": {"type": "drum_mute", "index": 3}},
        {"keys": ["Alt", "F5"], "action": {"type": "drum_mute", "index": 4}},
        {"keys": ["Alt", "F6"], "action": {"type": "drum_mute", "index": 5}},
        {"keys": ["Alt", "F7"], "action": {"type": "drum_mute", "index": 6}},
        {"keys": ["Alt", "F8"], "action": {"type": "drum_mute", "index": 7}},
        {"keys": ["Alt", "F9"], "action": {"type": "drum_mute", "index": 8}},
        {"keys": ["Alt", "F10"], "action": {"type": "drum_mute", "index": 9}},
        {"keys": ["Alt", "F11"], "action": {"type": "drum_mute", "index": 10}},
        {"keys": ["Alt", "F12"], "action": {"type": "drum_mute", "index": 11}},
    ]
    return KeymapConfig.from_dict({"bindings": raw})
