"""Structured MIDI message helpers shared by macros and libraries."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Iterable, Optional

MAX_MIDI_BYTES = 256
MAX_MIDI_DELAY_MS = 10000
MIDI_CHANNEL_MIN = 1
MIDI_CHANNEL_MAX = 16

MIDI_MESSAGE_KINDS: tuple[str, ...] = (
    "note_on", "note_off", "cc", "program_change", "pitch_bend",
    "channel_pressure", "poly_pressure", "sysex", "raw",
)

_KIND_ALIASES = {
    "note": "note_on",
    "note_on": "note_on",
    "note_off": "note_off",
    "control_change": "cc",
    "cc": "cc",
    "program": "program_change",
    "program_change": "program_change",
    "pitch_bend": "pitch_bend",
    "pitchbend": "pitch_bend",
    "channel_pressure": "channel_pressure",
    "aftertouch": "channel_pressure",
    "poly_pressure": "poly_pressure",
    "poly_aftertouch": "poly_pressure",
    "sysex": "sysex",
    "raw": "raw",
}

_NOTE_NAMES = ("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B")
_CC_NAMES = {
    0: "Bank Select MSB", 1: "Modulation", 2: "Breath", 4: "Foot",
    5: "Portamento Time", 6: "Data Entry MSB", 7: "Volume", 8: "Balance",
    10: "Pan", 11: "Expression", 32: "Bank Select LSB", 38: "Data Entry LSB",
    64: "Hold 1", 65: "Portamento", 66: "Sostenuto", 67: "Soft",
    68: "Legato", 69: "Hold 2", 71: "Resonance", 72: "Release Time",
    73: "Attack Time", 74: "Cutoff", 75: "Decay Time", 76: "Vibrato Rate",
    77: "Vibrato Depth", 78: "Vibrato Delay", 84: "Portamento Control",
    91: "Reverb Send", 92: "Tremolo", 93: "Chorus Send", 94: "Celeste",
    95: "Phaser", 96: "Data Increment", 97: "Data Decrement",
    98: "NRPN LSB", 99: "NRPN MSB", 100: "RPN LSB", 101: "RPN MSB",
    120: "All Sound Off", 121: "Reset All Controllers",
    122: "Local On/Off", 123: "All Notes Off", 124: "Omni Off",
    125: "Omni On", 126: "Mono On", 127: "Poly On",
}


def clamp(value: int, minimum: int, maximum: int) -> int:
    return max(minimum, min(maximum, value))


def _as_int(value: Any, fallback: int = 0) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        return fallback


def note_name(note: int) -> str:
    note = clamp(note, 0, 127)
    return f"{_NOTE_NAMES[note % 12]}{note // 12 - 1}"


def cc_name(controller: int) -> str:
    return _CC_NAMES.get(controller, f"CC{controller}")


def normalize_kind(kind: Any) -> str:
    if isinstance(kind, str):
        return _KIND_ALIASES.get(kind.lower(), "raw")
    return "raw"


@dataclass
class MidiMessageSpec:
    kind: str = "cc"
    channel: int = 1
    number: int = 0
    value: int = 0
    bend: int = 0
    bytes: list[int] = field(default_factory=list)
    delay_ms: int = 0

    def normalized(self) -> "MidiMessageSpec":
        kind = normalize_kind(self.kind)
        spec = MidiMessageSpec(
            kind=kind,
            channel=clamp(_as_int(self.channel, 1), MIDI_CHANNEL_MIN, MIDI_CHANNEL_MAX),
            number=clamp(_as_int(self.number), 0, 127),
            value=clamp(_as_int(self.value), 0, 127),
            bend=clamp(_as_int(self.bend), -8192, 8191),
            bytes=[clamp(_as_int(b), 0, 255) for b in self.bytes],
            delay_ms=clamp(_as_int(self.delay_ms), 0, MAX_MIDI_DELAY_MS),
        )
        if kind == "program_change":
            spec.number = clamp(spec.number, 0, 127)
        elif kind == "channel_pressure":
            spec.value = clamp(_as_int(self.value), 0, 127)
        elif kind == "pitch_bend":
            spec.bend = clamp(_as_int(self.bend), -8192, 8191)
        elif kind in ("sysex", "raw"):
            spec.bytes = spec.bytes[:MAX_MIDI_BYTES]
        return spec

    def encoded(self) -> list[int]:
        spec = self.normalized()
        ch = (spec.channel - 1) & 0x0F
        kind = spec.kind
        if kind == "note_on":
            return [0x90 | ch, spec.number, spec.value]
        if kind == "note_off":
            return [0x80 | ch, spec.number, spec.value]
        if kind == "cc":
            return [0xB0 | ch, spec.number, spec.value]
        if kind == "program_change":
            return [0xC0 | ch, spec.number]
        if kind == "channel_pressure":
            return [0xD0 | ch, spec.value]
        if kind == "poly_pressure":
            return [0xA0 | ch, spec.number, spec.value]
        if kind == "pitch_bend":
            bend = spec.bend + 8192
            return [0xE0 | ch, bend & 0x7F, (bend >> 7) & 0x7F]
        data = list(spec.bytes)
        if kind == "sysex":
            if not data or data[0] != 0xF0:
                data.insert(0, 0xF0)
            if data[-1] != 0xF7:
                data.append(0xF7)
        return data[:MAX_MIDI_BYTES]

    def summary(self) -> str:
        spec = self.normalized()
        suffix = f" ch{spec.channel}"
        if spec.kind == "note_on":
            return f"Note On {note_name(spec.number)} vel {spec.value}{suffix}"
        if spec.kind == "note_off":
            return f"Note Off {note_name(spec.number)}{suffix}"
        if spec.kind == "cc":
            return f"{cc_name(spec.number)} = {spec.value}{suffix}"
        if spec.kind == "program_change":
            return f"Program {spec.number + 1}{suffix}"
        if spec.kind == "channel_pressure":
            return f"Channel Pressure {spec.value}{suffix}"
        if spec.kind == "poly_pressure":
            return f"Poly Pressure {note_name(spec.number)} {spec.value}{suffix}"
        if spec.kind == "pitch_bend":
            return f"Pitch Bend {spec.bend}{suffix}"
        if spec.kind == "sysex":
            return f"SysEx {len(spec.encoded())} bytes"
        return f"Raw MIDI {len(spec.bytes)} bytes"

    def to_dict(self) -> dict:
        spec = self.normalized()
        out: dict[str, Any] = {"type": spec.kind}
        if spec.kind in ("sysex", "raw"):
            out["bytes"] = list(spec.bytes)
        else:
            out["channel"] = spec.channel
            if spec.kind in ("note_on", "note_off"):
                out["note"] = spec.number
                out["velocity"] = spec.value
            elif spec.kind == "cc":
                out["controller"] = spec.number
                out["value"] = spec.value
            elif spec.kind == "program_change":
                out["program"] = spec.number
            elif spec.kind == "channel_pressure":
                out["pressure"] = spec.value
            elif spec.kind == "poly_pressure":
                out["note"] = spec.number
                out["pressure"] = spec.value
            elif spec.kind == "pitch_bend":
                out["bend"] = spec.bend
        if spec.delay_ms:
            out["delay_ms"] = spec.delay_ms
        return out

    @classmethod
    def from_dict(cls, data: Any) -> "MidiMessageSpec":
        if not isinstance(data, dict):
            return cls(kind="raw")
        kind = normalize_kind(data.get("type", data.get("kind")))
        raw_bytes = data.get("bytes")
        if not isinstance(raw_bytes, list) and kind in ("sysex", "raw"):
            raw_bytes = data.get("data")
        spec = cls(
            kind=kind,
            channel=_as_int(data.get("channel"), 1),
            number=_as_int(
                data.get("note", data.get("controller",
                                         data.get("program", data.get("number")))),
                0,
            ),
            value=_as_int(
                data.get("velocity", data.get("value", data.get("pressure"))),
                0,
            ),
            bend=_as_int(data.get("bend"), 0),
            bytes=list(raw_bytes) if isinstance(raw_bytes, list) else [],
            delay_ms=_as_int(data.get("delay_ms"), 0),
        )
        if kind == "channel_pressure":
            spec.value = _as_int(data.get("pressure", data.get("value")), 0)
        if kind == "program_change":
            spec.number = _as_int(data.get("program", 0), 0)
        return spec


def parse_hex_bytes(text: str) -> list[int]:
    cleaned = text.replace(",", " ").replace("0x", " ").replace("0X", " ")
    result: list[int] = []
    for token in cleaned.split():
        if not token:
            continue
        if len(token) > 2 and all(c in "0123456789abcdefABCDEF" for c in token):
            for i in range(0, len(token), 2):
                chunk = token[i:i + 2]
                if len(chunk) == 2:
                    result.append(int(chunk, 16))
            continue
        result.append(int(token, 16))
    return result


def format_hex_bytes(data: Iterable[int]) -> str:
    return " ".join(f"{b & 0xFF:02X}" for b in data)


def decode_midi_bytes(data: Iterable[int]) -> MidiMessageSpec:
    raw = [b & 0xFF for b in data]
    if not raw:
        return MidiMessageSpec(kind="raw")
    status = raw[0]
    if status == 0xF0 or (status >= 0xF0 and 0xF7 in raw):
        return MidiMessageSpec(kind="sysex", bytes=raw)
    if status < 0x80 or status >= 0xF0:
        return MidiMessageSpec(kind="raw", bytes=raw)
    cmd = status & 0xF0
    channel = (status & 0x0F) + 1
    if cmd in (0x80, 0x90, 0xA0, 0xB0) and len(raw) >= 3:
        kind = {0x80: "note_off", 0x90: "note_on",
                0xA0: "poly_pressure", 0xB0: "cc"}[cmd]
        return MidiMessageSpec(kind=kind, channel=channel,
                               number=raw[1] & 0x7F, value=raw[2] & 0x7F)
    if cmd in (0xC0, 0xD0) and len(raw) >= 2:
        kind = "program_change" if cmd == 0xC0 else "channel_pressure"
        return MidiMessageSpec(kind=kind, channel=channel,
                               number=raw[1] & 0x7F,
                               value=raw[1] & 0x7F)
    if cmd == 0xE0 and len(raw) >= 3:
        bend = ((raw[2] & 0x7F) << 7 | (raw[1] & 0x7F)) - 8192
        return MidiMessageSpec(kind="pitch_bend", channel=channel, bend=bend)
    return MidiMessageSpec(kind="raw", bytes=raw)


def describe_midi_bytes(data: Iterable[int]) -> str:
    return decode_midi_bytes(data).summary()


def validate_midi_message(spec: MidiMessageSpec) -> Optional[str]:
    encoded = spec.encoded()
    if not encoded:
        return "MIDI message is empty"
    if len(encoded) > MAX_MIDI_BYTES:
        return f"MIDI message exceeds {MAX_MIDI_BYTES} bytes"
    if any(b < 0 or b > 255 for b in encoded):
        return "MIDI bytes must be 0-255"
    return None
