"""User-loadable MIDI command libraries."""

from __future__ import annotations

from dataclasses import dataclass, field, replace
from typing import Any, Optional

from .macro import MacroCommand
from .midi import MidiMessageSpec


@dataclass
class MidiLibraryCommand:
    name: str
    messages: list[MidiMessageSpec] = field(default_factory=list)
    group: str = ""

    @classmethod
    def from_dict(cls, data: Any, group: str = "") -> "MidiLibraryCommand | None":
        if not isinstance(data, dict):
            return None
        name = data.get("name")
        if not isinstance(name, str) or not name.strip():
            name = "Unnamed command"
        raw_messages = data.get("messages")
        if not isinstance(raw_messages, list):
            if "type" in data or "kind" in data:
                raw_messages = [data]
            else:
                raw_messages = []
        messages = [
            MidiMessageSpec.from_dict(item).normalized()
            for item in raw_messages if isinstance(item, dict)
        ]
        messages = [m for m in messages if m.encoded()]
        if not messages:
            return None
        return cls(name=name.strip(), messages=messages, group=group)

    def macro_commands(self, channel: Optional[int] = None) -> list[MacroCommand]:
        """Expand to macro commands, optionally overriding the MIDI channel.

        SysEx and raw messages ignore the channel field, so they are unchanged.
        """
        commands: list[MacroCommand] = []
        for msg in self.messages:
            spec = replace(msg, channel=channel) if channel is not None else msg
            commands.append(MacroCommand(
                type="midi", bytes=spec.encoded(), delay_ms=spec.delay_ms))
        return commands

    def summary(self) -> str:
        if len(self.messages) == 1:
            return self.messages[0].summary()
        return f"{self.messages[0].summary()} (+{len(self.messages) - 1})"

    def to_dict(self) -> dict:
        return {
            "name": self.name,
            "messages": [message.to_dict() for message in self.messages],
        }


@dataclass
class MidiLibraryGroup:
    name: str
    commands: list[MidiLibraryCommand] = field(default_factory=list)

    def to_dict(self) -> dict:
        return {
            "name": self.name,
            "commands": [command.to_dict() for command in self.commands],
        }


@dataclass
class MidiLibrary:
    name: str
    device: str = ""
    description: str = ""
    source: str = ""
    groups: list[MidiLibraryGroup] = field(default_factory=list)

    @classmethod
    def from_dict(cls, data: Any) -> "MidiLibrary | None":
        if not isinstance(data, dict):
            return None
        name = data.get("name")
        if not isinstance(name, str) or not name.strip():
            return None
        library = cls(
            name=name.strip(),
            device=str(data.get("device", "")),
            description=str(data.get("description", "")),
            source=str(data.get("source", "")),
        )

        raw_groups = data.get("groups")
        if isinstance(raw_groups, list):
            for raw_group in raw_groups:
                if not isinstance(raw_group, dict):
                    continue
                group_name = str(raw_group.get("name", "Commands"))
                commands = raw_group.get("commands")
                if not isinstance(commands, list):
                    continue
                group = MidiLibraryGroup(name=group_name)
                for raw_command in commands:
                    command = MidiLibraryCommand.from_dict(raw_command, group_name)
                    if command is not None:
                        group.commands.append(command)
                if group.commands:
                    library.groups.append(group)

        raw_commands = data.get("commands")
        if isinstance(raw_commands, list):
            group = MidiLibraryGroup(name="Commands")
            for raw_command in raw_commands:
                command = MidiLibraryCommand.from_dict(raw_command, "Commands")
                if command is not None:
                    group.commands.append(command)
            if group.commands:
                library.groups.append(group)

        return library if library.groups else None

    def command_count(self) -> int:
        return sum(len(group.commands) for group in self.groups)

    def to_dict(self) -> dict:
        return {
            "schema": 1,
            "name": self.name,
            "device": self.device,
            "description": self.description,
            "source": self.source,
            "groups": [group.to_dict() for group in self.groups],
        }
