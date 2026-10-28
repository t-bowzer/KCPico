"""Keyboard geometry and display labels for the keymap diagram.

The physical layout mirrors a full-size ANSI 104-key keyboard. The fixed chord
and strum mappings mirror ``keybchord/lib/core/keymap.cpp`` and ``strum.cpp``.
Only the configurable bindings come from ``KeymapConfig``.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Iterable, Optional

from ..models.enums import ChordQuality, NumberKeyMode
from ..models.keymap import (
    BINDABLE_KEYS,
    BOOL_PARAMS,
    KEYMAP_PARAM_NAMES,
    MODIFIERS,
    PARAM_VALUE_TABLE,
    KeyAction,
)
from ..models.macro import (
    NUMBER_KEY_LABELS,
    MacroCommand,
    MacroConfig,
    default_macro_name,
)
from ..models.preset import DRUM_PIECES


@dataclass(frozen=True)
class KeySpec:
    name: str
    label: str
    x: float
    y: float
    w: float = 1.0
    h: float = 1.0
    kind: str = "other"
    chord_root: str = ""
    chord_quality: Optional[ChordQuality] = None
    strum_full: bool = False
    strum_limited: bool = False
    fixed: str = ""


@dataclass(frozen=True)
class ActionLabel:
    short: str
    detail: str


LAYER_ALL = "All"
LAYER_NONE = "None"
LAYER_ORDER = (LAYER_ALL, LAYER_NONE, *MODIFIERS)

CHORD_ROOTS: tuple[str, ...] = (
    "Db", "Ab", "Eb", "Bb", "F", "C", "G", "D", "A", "E", "B", "F#",
)

_CHORD_ROWS: tuple[tuple[ChordQuality, tuple[str, ...]], ...] = (
    (
        ChordQuality.MAJOR,
        ("Tab", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "["),
    ),
    (
        ChordQuality.MINOR,
        ("Caps", "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'"),
    ),
    (
        ChordQuality.SEVENTH,
        ("LShift", "Z", "X", "C", "V", "B", "N", "M", ",", ".", "/", "RShift"),
    ),
)

CHORD_GRID: dict[str, tuple[str, ChordQuality]] = {
    key: (CHORD_ROOTS[column], quality)
    for quality, keys in _CHORD_ROWS
    for column, key in enumerate(keys)
}

CHORD_QUALITY_LABELS: dict[ChordQuality, str] = {
    ChordQuality.MAJOR: "Maj",
    ChordQuality.MINOR: "Min",
    ChordQuality.SEVENTH: "7th",
}

_NUMBER_ROW_STRUM: tuple[str, ...] = (
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
)

_NUMPAD_STRUM_FULL: tuple[str, ...] = (
    "Kp0", "KpDot", "Kp1", "Kp2", "Kp3", "Kp4", "Kp5", "Kp6", "Kp7",
    "Kp8", "Kp9", "NumLock", "KpSlash", "KpStar",
)

_NUMPAD_STRUM_LIMITED: tuple[str, ...] = (
    "Kp0", "KpDot", "Kp2", "Kp3", "Kp5", "Kp6", "Kp8", "Kp9", "KpSlash",
    "KpStar",
)

STRUM_FULL: frozenset[str] = frozenset(_NUMBER_ROW_STRUM) | frozenset(
    _NUMPAD_STRUM_FULL
)
STRUM_LIMITED: frozenset[str] = frozenset(_NUMPAD_STRUM_LIMITED)

RESERVED_KEYS: frozenset[str] = frozenset(
    {"Esc", "Backtick", "Enter", "Backspace", "Mute", "VolumeUp", "VolumeDown"}
)

HARDCODED_COMBOS: dict[tuple[Optional[str], str], str] = {
    ("Super", "Esc"): "Panic",
    **{("Super", str(slot)): f"Load preset {slot}" for slot in range(1, 9)},
}

RESERVED_DESCRIPTIONS: dict[str, str] = {
    "Esc": "Clear edit / cancel (hold at power-on for bootloader)",
    "Backtick": "Leftmost chord-column modifier",
    "Enter": "Reserved",
    "Backspace": "Reserved",
    "Mute": "Fixed: same as Enter",
    "VolumeUp": "Fixed: tempo + (main) / value + (menu)",
    "VolumeDown": "Fixed: tempo - (main) / value - (menu)",
}


def _k(name: str, label: str, x: float, y: float,
       w: float = 1.0, h: float = 1.0) -> tuple[str, str, float, float, float, float]:
    return (name, label, x, y, w, h)


_FUNCTION_ROW = (
    _k("Esc", "Esc", 0.0, 0.0),
    _k("F1", "F1", 2.0, 0.0),
    _k("F2", "F2", 3.0, 0.0),
    _k("F3", "F3", 4.0, 0.0),
    _k("F4", "F4", 5.0, 0.0),
    _k("F5", "F5", 6.5, 0.0),
    _k("F6", "F6", 7.5, 0.0),
    _k("F7", "F7", 8.5, 0.0),
    _k("F8", "F8", 9.5, 0.0),
    _k("F9", "F9", 11.0, 0.0),
    _k("F10", "F10", 12.0, 0.0),
    _k("F11", "F11", 13.0, 0.0),
    _k("F12", "F12", 14.0, 0.0),
    _k("PrtSc", "PrtSc", 15.25, 0.0),
    _k("ScLk", "ScrLk", 16.25, 0.0),
    _k("Pause", "Pause", 17.25, 0.0),
)

_NUMBER_ROW = (
    _k("Backtick", "`", 0.0, 1.25),
    _k("1", "1", 1.0, 1.25),
    _k("2", "2", 2.0, 1.25),
    _k("3", "3", 3.0, 1.25),
    _k("4", "4", 4.0, 1.25),
    _k("5", "5", 5.0, 1.25),
    _k("6", "6", 6.0, 1.25),
    _k("7", "7", 7.0, 1.25),
    _k("8", "8", 8.0, 1.25),
    _k("9", "9", 9.0, 1.25),
    _k("0", "0", 10.0, 1.25),
    _k("-", "-", 11.0, 1.25),
    _k("=", "=", 12.0, 1.25),
    _k("Backspace", "Bksp", 13.0, 1.25, 2.0),
)

_TAB_ROW = (
    _k("Tab", "Tab", 0.0, 2.25, 1.5),
    _k("Q", "Q", 1.5, 2.25),
    _k("W", "W", 2.5, 2.25),
    _k("E", "E", 3.5, 2.25),
    _k("R", "R", 4.5, 2.25),
    _k("T", "T", 5.5, 2.25),
    _k("Y", "Y", 6.5, 2.25),
    _k("U", "U", 7.5, 2.25),
    _k("I", "I", 8.5, 2.25),
    _k("O", "O", 9.5, 2.25),
    _k("P", "P", 10.5, 2.25),
    _k("[", "[", 11.5, 2.25),
    _k("]", "]", 12.5, 2.25),
    _k("\\", "\\", 13.5, 2.25, 1.5),
)

_CAPS_ROW = (
    _k("Caps", "Caps", 0.0, 3.25, 1.75),
    _k("A", "A", 1.75, 3.25),
    _k("S", "S", 2.75, 3.25),
    _k("D", "D", 3.75, 3.25),
    _k("F", "F", 4.75, 3.25),
    _k("G", "G", 5.75, 3.25),
    _k("H", "H", 6.75, 3.25),
    _k("J", "J", 7.75, 3.25),
    _k("K", "K", 8.75, 3.25),
    _k("L", "L", 9.75, 3.25),
    _k(";", ";", 10.75, 3.25),
    _k("'", "'", 11.75, 3.25),
    _k("Enter", "Enter", 12.75, 3.25, 2.25),
)

_SHIFT_ROW = (
    _k("LShift", "Shift", 0.0, 4.25, 2.25),
    _k("Z", "Z", 2.25, 4.25),
    _k("X", "X", 3.25, 4.25),
    _k("C", "C", 4.25, 4.25),
    _k("V", "V", 5.25, 4.25),
    _k("B", "B", 6.25, 4.25),
    _k("N", "N", 7.25, 4.25),
    _k("M", "M", 8.25, 4.25),
    _k(",", ",", 9.25, 4.25),
    _k(".", ".", 10.25, 4.25),
    _k("/", "/", 11.25, 4.25),
    _k("RShift", "Shift", 12.25, 4.25, 2.75),
)

_BOTTOM_ROW = (
    _k("LCtrl", "Ctrl", 0.0, 5.25, 1.25),
    _k("LSuper", "Super", 1.25, 5.25, 1.25),
    _k("LAlt", "Alt", 2.5, 5.25, 1.25),
    _k("Space", "Space", 3.75, 5.25, 6.25),
    _k("RAlt", "Alt", 10.0, 5.25, 1.25),
    _k("RSuper", "Super", 11.25, 5.25, 1.25),
    _k("Menu", "Menu", 12.5, 5.25, 1.25),
    _k("RCtrl", "Ctrl", 13.75, 5.25, 1.25),
)

_NAV_CLUSTER = (
    _k("Insert", "Ins", 15.25, 1.25),
    _k("Home", "Home", 16.25, 1.25),
    _k("PgUp", "PgUp", 17.25, 1.25),
    _k("Delete", "Del", 15.25, 2.25),
    _k("End", "End", 16.25, 2.25),
    _k("PgDn", "PgDn", 17.25, 2.25),
    _k("Up", "\u2191", 16.25, 4.25),
    _k("Left", "\u2190", 15.25, 5.25),
    _k("Down", "\u2193", 16.25, 5.25),
    _k("Right", "\u2192", 17.25, 5.25),
)

_NUMPAD = (
    _k("NumLock", "NumLk", 18.5, 1.25),
    _k("KpSlash", "/", 19.5, 1.25),
    _k("KpStar", "*", 20.5, 1.25),
    _k("KpMinus", "-", 21.5, 1.25),
    _k("Kp7", "7", 18.5, 2.25),
    _k("Kp8", "8", 19.5, 2.25),
    _k("Kp9", "9", 20.5, 2.25),
    _k("KpPlus", "+", 21.5, 2.25, 1.0, 2.0),
    _k("Kp4", "4", 18.5, 3.25),
    _k("Kp5", "5", 19.5, 3.25),
    _k("Kp6", "6", 20.5, 3.25),
    _k("Kp1", "1", 18.5, 4.25),
    _k("Kp2", "2", 19.5, 4.25),
    _k("Kp3", "3", 20.5, 4.25),
    _k("KpEnter", "Enter", 21.5, 4.25, 1.0, 2.0),
    _k("Kp0", "0", 18.5, 5.25, 2.0),
    _k("KpDot", ".", 20.5, 5.25),
)

_MEDIA_ROW = (
    _k("VolumeDown", "Vol-", 0.0, 6.5),
    _k("VolumeUp", "Vol+", 1.0, 6.5),
    _k("Mute", "Mute", 2.0, 6.5),
    _k("PlayPause", "Play", 3.0, 6.5),
    _k("Stop", "Stop", 4.0, 6.5),
    _k("PrevTrack", "Prev", 5.0, 6.5),
    _k("NextTrack", "Next", 6.0, 6.5),
    _k("Rewind", "Rew", 7.0, 6.5),
    _k("FastForward", "FF", 8.0, 6.5),
    _k("Eject", "Eject", 9.0, 6.5),
    _k("MediaSelect", "Media", 10.0, 6.5),
    _k("Mail", "Mail", 11.0, 6.5),
    _k("Calculator", "Calc", 12.0, 6.5),
    _k("BrowserSearch", "B.Search", 13.0, 6.5),
    _k("BrowserHome", "Home", 14.0, 6.5),
    _k("BrowserBack", "Back", 15.0, 6.5),
    _k("BrowserForward", "Fwd", 16.0, 6.5),
    _k("BrowserStop", "B.Stop", 17.0, 6.5),
    _k("BrowserRefresh", "Refresh", 18.0, 6.5),
    _k("BrowserFavorites", "Fav", 19.0, 6.5),
)

_KEY_GEOMETRY = (
    _FUNCTION_ROW + _NUMBER_ROW + _TAB_ROW + _CAPS_ROW + _SHIFT_ROW
    + _BOTTOM_ROW + _NAV_CLUSTER + _NUMPAD
)

_BINDABLE_SET = frozenset(BINDABLE_KEYS)


def _kind_for(name: str) -> str:
    if name in CHORD_GRID:
        return "chord"
    if name in STRUM_FULL:
        return "strum"
    if name in RESERVED_KEYS:
        return "reserved"
    if name in _BINDABLE_SET:
        return "bindable"
    return "other"


def _build_keys(geometry=_KEY_GEOMETRY) -> tuple[KeySpec, ...]:
    specs: list[KeySpec] = []
    for name, label, x, y, w, h in geometry:
        chord_root = ""
        quality: Optional[ChordQuality] = None
        if name in CHORD_GRID:
            chord_root, quality = CHORD_GRID[name]
        specs.append(
            KeySpec(
                name=name,
                label=label,
                x=x,
                y=y,
                w=w,
                h=h,
                kind=_kind_for(name),
                chord_root=chord_root,
                chord_quality=quality,
                strum_full=name in STRUM_FULL,
                strum_limited=name in STRUM_LIMITED,
                fixed=RESERVED_DESCRIPTIONS.get(name, ""),
            )
        )
    return tuple(specs)


ANSI_104_KEYS: tuple[KeySpec, ...] = _build_keys()
MEDIA_KEYS: tuple[KeySpec, ...] = _build_keys(_MEDIA_ROW)
ALL_KEYS: tuple[KeySpec, ...] = ANSI_104_KEYS + MEDIA_KEYS
KEY_BY_NAME: dict[str, KeySpec] = {spec.name: spec for spec in ALL_KEYS}
KEY_DISPLAY_LABELS: dict[str, str] = {
    spec.name: spec.label for spec in ALL_KEYS
}

LAYOUT_WIDTH: float = max(spec.x + spec.w for spec in ALL_KEYS)
LAYOUT_HEIGHT: float = max(spec.y + spec.h for spec in ALL_KEYS)


PARAM_LABELS: dict[str, str] = {
    "chord_octave": "Chord oct",
    "chord_mode": "Play mode",
    "chord_voicing": "Voicing",
    "chord_duration": "Chord dur",
    "chord_velocity": "Chord vel",
    "chord_pan": "Pan",
    "chord_roll": "Roll",
    "chord_min_notes": "Min notes",
    "chord_min_interval": "Min int",
    "chord_inversion": "Inversion",
    "arp_mode": "Arp mode",
    "chord_pattern": "Chord pat",
    "strum_octave": "Strum oct",
    "strum_duration": "Strum dur",
    "strum_velocity": "Strum vel",
    "strum_layout": "Layout",
    "strum_mode": "Strum mode",
    "strum_root": "Strum root",
    "strum_scale": "Scale",
    "tempo": "Tempo",
    "swing": "Swing",
    "rhythm_pattern": "Rhythm",
    "rhythm_mute": "Rhythm mute",
    "rhythm_enable": "Rhythm",
    "rhythm_clock": "Clock out",
    "rhythm_led": "LED",
    "usb_midi": "USB MIDI",
    "bass_enable": "Bass",
    "bass_octave": "Bass oct",
    "bass_duration": "Bass dur",
    "bass_velocity": "Bass vel",
    "bass_channel": "Bass ch",
    "bass_pattern": "Bass pat",
}

MENU_LABELS: dict[str, str] = {
    "chord": "Chord menu",
    "strum": "Strum menu",
    "rhythm": "Rhythm menu",
    "bass": "Bass menu",
    "drum": "Drum menu",
}

PARAM_VALUE_LABELS: dict[str, dict[Any, str]] = {
    "chord_mode": {
        "held": "Held",
        "press": "Press",
        "arpeggio": "Arpeggio",
        "arp_hold": "Arp hold",
        "silent": "Silent",
        "pattern": "Pattern",
        "pattern_hold": "Pattern hold",
    },
    "chord_voicing": {
        "root": "Root",
        "smart": "Smart",
        "down": "Down",
        "up": "Up",
    },
    "chord_inversion": {
        "root": "Root",
        "first": "1st",
        "second": "2nd",
        "third": "3rd",
    },
    "arp_mode": {
        "up": "Up",
        "down": "Down",
        "up_down": "Up-down",
        "alternating": "Alternating",
        "random": "Random",
    },
    "strum_mode": {
        "chord": "Follow chord",
        "scale": "Scale",
        "piano": "Piano",
    },
    "strum_scale": {
        "ionian": "Ionian",
        "dorian": "Dorian",
        "phrygian": "Phrygian",
        "lydian": "Lydian",
        "mixolydian": "Mixolydian",
        "aeolian": "Aeolian",
        "locrian": "Locrian",
        "harmonic_minor": "Harm min",
        "melodic_minor": "Mel min",
        "major_pentatonic": "Maj pent",
        "minor_pentatonic": "Min pent",
        "blues": "Blues",
    },
    "strum_layout": {
        0: "Full",
        1: "Limited",
        "on": "Limited",
        "off": "Full",
        "true": "Limited",
        "false": "Full",
    },
}

DRUM_LABELS: dict[int, str] = {
    index: label for index, (_key, label, _note) in enumerate(DRUM_PIECES)
}

MODIFIER_COLORS: dict[str, str] = {
    "Ctrl": "#2d7dd2",
    "Alt": "#e08a2e",
    "Super": "#9b59b6",
}

CHORD_QUALITY_COLORS: dict[ChordQuality, str] = {
    ChordQuality.MAJOR: "#2b3a2f",
    ChordQuality.MINOR: "#293040",
    ChordQuality.SEVENTH: "#3a2f26",
}


def binding_index(
    bindings: Iterable[Any],
) -> tuple[dict[str, list[int]], dict[tuple[Optional[str], str], int]]:
    by_key: dict[str, list[int]] = {}
    by_combo: dict[tuple[Optional[str], str], int] = {}
    for index, binding in enumerate(bindings):
        by_key.setdefault(binding.key, []).append(index)
        by_combo.setdefault((binding.modifier, binding.key), index)
    return by_key, by_combo


def matches_layer(modifier: Optional[str], layer: str) -> bool:
    if layer == LAYER_ALL:
        return True
    if layer == LAYER_NONE:
        return modifier is None
    return modifier == layer


def _value_label(param: str, value: Any, *,
                 rhythm_names: Iterable[str] = (),
                 bass_names: Iterable[str] = (),
                 chord_pattern_names: Iterable[str] = ()) -> str:
    if value is None:
        return ""
    if param == "strum_layout":
        table = PARAM_VALUE_LABELS["strum_layout"]
        if isinstance(value, bool):
            return "Limited" if value else "Full"
        if isinstance(value, str):
            return table.get(value.lower(), value)
        if isinstance(value, int) and value in (0, 1):
            return table[value]
    if param in BOOL_PARAMS:
        if isinstance(value, str):
            return "on" if value.lower() in ("on", "true", "1") else "off"
        return "on" if value else "off"
    labels = PARAM_VALUE_LABELS.get(param)
    numeric = PARAM_VALUE_TABLE.get(param)
    if labels is not None:
        if isinstance(value, str):
            return labels.get(value, value)
        if numeric is not None:
            for name, number in numeric.items():
                if number == value:
                    return labels.get(name, name)
    if param in ("rhythm_pattern", "bass_pattern", "chord_pattern"):
        names = {
            "rhythm_pattern": list(rhythm_names),
            "bass_pattern": list(bass_names),
            "chord_pattern": list(chord_pattern_names),
        }[param]
        if isinstance(value, int) and 0 <= value < len(names):
            return names[value]
        return str(value)
    if numeric is not None and isinstance(value, int):
        for name, number in numeric.items():
            if number == value:
                return name
    return str(value)


def format_action(action: Any, *,
                  rhythm_names: Iterable[str] = (),
                  bass_names: Iterable[str] = (),
                  chord_pattern_names: Iterable[str] = ()) -> ActionLabel:
    param = action.param or ""
    label = PARAM_LABELS.get(param, param or "Parameter")
    action_type = action.type

    if action_type == "open_menu":
        menu = MENU_LABELS.get(action.menu or "", "Menu")
        return ActionLabel(menu, f"Open {menu.lower()}")

    if action_type == "cycle":
        return ActionLabel(label, f"Cycle {label}")

    if action_type == "inc":
        return ActionLabel(f"{label} +", f"Increment {label}")

    if action_type == "dec":
        return ActionLabel(f"{label} \u2212", f"Decrement {label}")

    if action_type == "set":
        value = _value_label(
            param, action.value, rhythm_names=rhythm_names,
            bass_names=bass_names, chord_pattern_names=chord_pattern_names,
        )
        return ActionLabel(f"{label}: {value}", f"Set {label} to {value}")

    if action_type == "toggle":
        a = _value_label(
            param, action.a, rhythm_names=rhythm_names,
            bass_names=bass_names, chord_pattern_names=chord_pattern_names,
        )
        b = _value_label(
            param, action.b, rhythm_names=rhythm_names,
            bass_names=bass_names, chord_pattern_names=chord_pattern_names,
        )
        if param in BOOL_PARAMS or param == "strum_layout":
            short = f"{label} on/off"
        else:
            short = f"{label}: {a}/{b}"
        return ActionLabel(short, f"Toggle {label}: {a} \u2194 {b}")

    if action_type == "ext":
        names = {9: "Add9", 11: "Add11", 13: "Add13"}
        ext = names.get(action.value, f"Add{action.value}")
        return ActionLabel(ext, f"Hold for {ext.lower()}")

    if action_type == "drum_mute":
        drum = DRUM_LABELS.get(action.index or 0, f"Drum {(action.index or 0) + 1}")
        return ActionLabel(f"Mute {drum}", f"Toggle mute for {drum}")

    if action_type == "preset_prev":
        return ActionLabel("Prev preset", "Select the previous preset")
    if action_type == "preset_next":
        return ActionLabel("Next preset", "Select the next preset")
    if action_type == "preset_bank_prev":
        return ActionLabel("Prev bank", "Select the previous preset bank")
    if action_type == "preset_bank_next":
        return ActionLabel("Next bank", "Select the next preset bank")
    if action_type == "preset_load":
        slot = action.slot if action.slot is not None else 0
        return ActionLabel(f"Load P{slot + 1}", f"Load preset slot {slot + 1}")
    if action_type == "preset_save":
        return ActionLabel("Save preset", "Save the current preset")
    if action_type == "preset_clear":
        return ActionLabel("Clear preset", "Clear the current preset")
    if action_type == "tap_tempo":
        return ActionLabel("Tap tempo", "Tap to set the rhythm tempo")

    return ActionLabel(action_type, action_type)


def is_macro_key(name: str, macro_config: Optional[MacroConfig]) -> bool:
    return (
        macro_config is not None
        and macro_config.number_keys == NumberKeyMode.MACRO
        and name in NUMBER_KEY_LABELS
    )


def macro_command_label(
    command: MacroCommand,
    *,
    rhythm_names: Iterable[str] = (),
    bass_names: Iterable[str] = (),
    chord_pattern_names: Iterable[str] = (),
) -> ActionLabel:
    if command.type == "drum_mute":
        piece = next(
            (label for key, label, _note in DRUM_PIECES if key == command.piece),
            command.piece or "Drum",
        )
        on = command.value in (True, "on", 1)
        short = f"{piece}: {'mute' if on else 'unmute'}"
        detail = f"{'Mute' if on else 'Unmute'} {piece}"
        return ActionLabel(short, detail)
    action = KeyAction(
        type=command.type,
        param=command.param,
        value=command.value,
        a=command.a,
        b=command.b,
    )
    return format_action(
        action,
        rhythm_names=rhythm_names,
        bass_names=bass_names,
        chord_pattern_names=chord_pattern_names,
    )


def macro_key_cell_text(macro_config: Optional[MacroConfig], label: str) -> str:
    if macro_config is None:
        return "Macro"
    name = macro_config.name_for(label)
    count = len(macro_config.keys.get(label, []))
    if count == 0 and name == default_macro_name(label):
        return "Macro"
    return name


def macro_key_summary(
    macro_config: Optional[MacroConfig],
    label: str,
    *,
    rhythm_names: Iterable[str] = (),
    bass_names: Iterable[str] = (),
    chord_pattern_names: Iterable[str] = (),
) -> str:
    if macro_config is None:
        return "No macro"
    commands = macro_config.keys.get(label, [])
    if not commands:
        return "Empty macro"
    if len(commands) == 1:
        return macro_command_label(
            commands[0],
            rhythm_names=rhythm_names,
            bass_names=bass_names,
            chord_pattern_names=chord_pattern_names,
        ).short
    return f"{macro_config.name_for(label)} ({len(commands)} commands)"


def combo_label(modifier: Optional[str], key: str) -> str:
    return f"{modifier}+{key}" if modifier else key


def describe_key(spec: KeySpec, *, macro_config: Optional[MacroConfig] = None,
                 preset_label: str = "") -> str:
    if is_macro_key(spec.name, macro_config):
        count = len(macro_config.keys.get(spec.name, []))
        where = f" for {preset_label}" if preset_label else ""
        return f"Per-preset macro{where} ({count} commands)"
    if spec.kind == "chord" and spec.chord_quality is not None:
        quality = CHORD_QUALITY_LABELS[spec.chord_quality]
        return f"Chord {spec.chord_root} {quality}"
    if spec.kind == "strum":
        layout = "full + limited" if spec.strum_limited else "full"
        return f"Strum plate ({layout})"
    if spec.kind == "reserved":
        return spec.fixed or "Reserved"
    if spec.kind == "bindable":
        return "Configurable key"
    return "Unmapped key"


def all_param_labels_known() -> bool:
    return all(name in PARAM_LABELS for name in KEYMAP_PARAM_NAMES)
