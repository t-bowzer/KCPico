"""Per-preset number-key macros.

Mirrors the firmware's ``macros.h`` / preset macro JSON. Macros are only used
when ``number_keys`` is ``"macro"``; the top-row keys ``1``..``9``,``0`` trigger
them while the numpad remains the strum plate.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Optional

from .enums import (
    NUMBER_KEY_MODE_JSON,
    NumberKeyMode,
    parse_number_key_mode,
)

NUMBER_KEY_LABELS: tuple[str, ...] = (
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
)

MAX_MACRO_COMMANDS_PER_KEY = 16
MAX_MACRO_COMMANDS_PER_PRESET = 48
MAX_MACRO_NAME_LEN = 16

MACRO_COMMAND_TYPES: tuple[str, ...] = (
    "set", "toggle", "cycle", "inc", "dec", "drum_mute",
)

MACRO_VALUE_TYPES: frozenset[str] = frozenset({"set", "toggle", "cycle", "inc", "dec"})

DRUM_PIECE_KEYS: tuple[str, ...] = (
    "kick", "snare", "hihat", "open_hat", "rimshot", "clap", "crash",
    "ride", "bongo", "conga_lo", "conga_hi", "clave", "shaker",
)


@dataclass
class MacroCommand:
    type: str = "set"
    param: Optional[str] = None
    value: Any = None
    a: Any = None
    b: Any = None
    piece: Optional[str] = None

    @classmethod
    def from_dict(cls, data: Any) -> "MacroCommand":
        cmd = cls()
        if not isinstance(data, dict):
            return cmd
        typ = data.get("type")
        if isinstance(typ, str) and typ in MACRO_COMMAND_TYPES:
            cmd.type = typ
        param = data.get("param")
        if isinstance(param, str):
            cmd.param = param
        cmd.value = data.get("value")
        cmd.a = data.get("a")
        cmd.b = data.get("b")
        piece = data.get("piece")
        if isinstance(piece, str):
            cmd.piece = piece
        return cmd

    def to_dict(self) -> dict:
        out: dict = {"type": self.type}
        if self.type in MACRO_VALUE_TYPES and self.param is not None:
            out["param"] = self.param
        if self.type == "set" and self.value is not None:
            out["value"] = self.value
        if self.type == "toggle":
            if self.a is not None:
                out["a"] = self.a
            if self.b is not None:
                out["b"] = self.b
        if self.type == "drum_mute":
            if self.piece is not None:
                out["piece"] = self.piece
            if self.value is not None:
                out["value"] = self.value
        return out

    def summary(self) -> str:
        if self.type == "drum_mute":
            return f"{self.piece or '?'} {'on' if self.value in (True, 'on', 1) else 'off'}"
        if self.type == "set":
            return f"{self.param or '?'} = {self.value}"
        if self.type == "toggle":
            return f"{self.param or '?'}: {self.a} <-> {self.b}"
        return self.param or ""


def default_macro_name(label: str) -> str:
    return f"M{label}"


@dataclass
class MacroConfig:
    number_keys: NumberKeyMode = NumberKeyMode.STRUM
    names: dict[str, str] = field(
        default_factory=lambda: {
            label: default_macro_name(label) for label in NUMBER_KEY_LABELS
        }
    )
    keys: dict[str, list[MacroCommand]] = field(default_factory=dict)

    @classmethod
    def defaults(cls) -> "MacroConfig":
        return cls()

    def name_for(self, label: str) -> str:
        name = self.names.get(label, "")
        return name if name else default_macro_name(label)

    @classmethod
    def from_dict(cls, data: Any) -> "MacroConfig":
        cfg = cls()
        if not isinstance(data, dict):
            return cfg
        cfg.number_keys = parse_number_key_mode(data.get("number_keys"))
        macros = data.get("macros")
        if not isinstance(macros, dict):
            return cfg
        for label in NUMBER_KEY_LABELS:
            raw = macros.get(label)
            if isinstance(raw, list):
                raw_commands = raw
            elif isinstance(raw, dict):
                raw_commands = raw.get("commands")
                name = raw.get("name")
                if isinstance(name, str) and name.strip():
                    cfg.names[label] = name.strip()[:MAX_MACRO_NAME_LEN]
            else:
                continue
            if not isinstance(raw_commands, list):
                continue
            commands: list[MacroCommand] = []
            for item in raw_commands:
                cmd = MacroCommand.from_dict(item)
                if cmd.type == "drum_mute" and not cmd.piece:
                    continue
                if cmd.type in MACRO_VALUE_TYPES and not cmd.param:
                    continue
                commands.append(cmd)
            if commands:
                cfg.keys[label] = commands
        return cfg

    def to_dict(self) -> dict:
        macros: dict[str, dict] = {}
        for label in NUMBER_KEY_LABELS:
            commands = self.keys.get(label, [])
            name = self.name_for(label)
            if not commands and name == default_macro_name(label):
                continue
            macros[label] = {
                "name": name,
                "commands": [cmd.to_dict() for cmd in commands],
            }
        return {
            "number_keys": NUMBER_KEY_MODE_JSON[self.number_keys],
            "macros": macros,
        }

    def commands_for(self, label: str) -> list[MacroCommand]:
        return self.keys.setdefault(label, [])

    def total_commands(self) -> int:
        return sum(len(commands) for commands in self.keys.values())

    def can_add(self, label: str) -> bool:
        if label not in NUMBER_KEY_LABELS:
            return False
        if self.total_commands() >= MAX_MACRO_COMMANDS_PER_PRESET:
            return False
        return len(self.keys.get(label, [])) < MAX_MACRO_COMMANDS_PER_KEY
