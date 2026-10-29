"""Per-preset chord sequencer (``/sequences/bNpM.json``).

Mirrors ``keybchord/lib/core/sequence.{h,cpp}``. A preset owns ten sequence
slots. Each slot is a loop of ``bars`` bars with ``steps_per_bar`` sixteenth
notes; events are chord changes encoded compactly as ``[step, chord_id]``.

``chord_id = root_pc * 128 + type * 8 + extension_mask`` where the extension
mask bits are add9=1, add11=2, add13=4. Recorded chord settings use a fixed
11-value array (see :data:`LIVE_SETTING_KEYS`); ``live_mask`` bit *i* means
"use the live value for setting *i*" during playback.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Iterable, Optional

from .bounds import (
    CHORD_MIN_INTERVAL_MAX,
    CHORD_MIN_INTERVAL_MIN,
    CHORD_MIN_NOTES_MAX,
    CHORD_MIN_NOTES_MIN,
    CHORD_ROLL_MAX,
    CHORD_ROLL_MIN,
    MAX_CHORD_PATTERNS,
    MAX_SEQUENCE_EVENTS,
    NOTE_DURATION_MAX,
    NOTE_DURATION_MIN,
    OCTAVE_MAX,
    OCTAVE_MIN,
    PAN_MAX,
    PAN_MIN,
    SEQUENCE_BARS_CHOICES,
    SEQUENCE_DEFAULT_STEPS,
    SEQUENCE_LIVE_MASK_ALL,
    SEQUENCE_MAX_STEPS_PER_BAR,
    SEQUENCE_MIN_STEPS_PER_BAR,
    SEQUENCE_SETTING_COUNT,
    SEQUENCE_SLOTS,
    VELOCITY_MAX,
    VELOCITY_MIN,
    clamp,
)
from .enums import (
    ArpMode,
    ChordType,
    InversionMode,
    VoicingMode,
)

# Fixed order used by the firmware's `recorded` array and live bitmask.
LIVE_SETTING_KEYS: tuple[str, ...] = (
    "octave",
    "note_duration_ms",
    "velocity",
    "pan",
    "voicing_mode",
    "chord_roll_ms",
    "min_notes",
    "min_interval",
    "inversion",
    "arp_mode",
    "pattern",
)

LIVE_SETTING_LABELS: tuple[str, ...] = (
    "Octave",
    "Note duration",
    "Velocity",
    "Pan",
    "Voicing",
    "Chord roll",
    "Min notes",
    "Min interval",
    "Inversion",
    "Arp mode",
    "Chord pattern",
)

ROOT_NAMES: tuple[str, ...] = (
    "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B",
)

CHORD_TYPE_SUFFIX: dict[ChordType, str] = {
    ChordType.MAJOR: "",
    ChordType.MINOR: "m",
    ChordType.DOM7: "7",
    ChordType.MAJ7: "maj7",
    ChordType.MIN7: "m7",
    ChordType.DIM: "dim",
    ChordType.DIM7: "dim7",
    ChordType.AUG: "aug",
    ChordType.SUS4: "sus4",
    ChordType.SUS2: "sus2",
    ChordType.MIN7B5: "m7b5",
}


def _int(value: Any, minimum: int, maximum: int, fallback: int) -> int:
    try:
        return clamp(int(value), minimum, maximum)
    except (TypeError, ValueError):
        return fallback


def _enum_int(value: Any, enum_type, fallback):
    if isinstance(value, int) and not isinstance(value, bool):
        for member in enum_type:
            if int(member) == value:
                return member
    return fallback


def clamp_bars(value: Any) -> int:
    """Round a requested bar count to the nearest supported 1/2/4/8 choice."""
    try:
        n = int(value)
    except (TypeError, ValueError):
        return 4
    if n <= 1:
        return 1
    if n <= 2:
        return 2
    if n <= 4:
        return 4
    return 8


def clamp_steps(value: Any) -> int:
    return _int(value, SEQUENCE_MIN_STEPS_PER_BAR, SEQUENCE_MAX_STEPS_PER_BAR,
                SEQUENCE_DEFAULT_STEPS)


def chord_id(root_pc: int, chord_type: ChordType, add9: bool = False,
             add11: bool = False, add13: bool = False) -> int:
    root = clamp(int(root_pc), 0, 11)
    typ = clamp(int(chord_type), 0, int(ChordType.MIN7B5))
    ext = (1 if add9 else 0) | (2 if add11 else 0) | (4 if add13 else 0)
    return root * 128 + typ * 8 + ext


def parse_chord_id(value: Any) -> Optional[tuple[int, ChordType, bool, bool, bool]]:
    if not isinstance(value, int) or isinstance(value, bool) or value < 0:
        return None
    root, rem = divmod(value, 128)
    typ, ext = divmod(rem, 8)
    if root > 11 or typ > int(ChordType.MIN7B5):
        return None
    return (root, ChordType(typ), bool(ext & 1), bool(ext & 2), bool(ext & 4))


def chord_label(value: int) -> str:
    parsed = parse_chord_id(value)
    if parsed is None:
        return ""
    root, typ, add9, add11, add13 = parsed
    text = ROOT_NAMES[root] + CHORD_TYPE_SUFFIX[typ]
    if add9:
        text += "+9"
    if add11:
        text += "+11"
    if add13:
        text += "+13"
    return text


@dataclass
class RecordedSettings:
    """The chord-parameter snapshot captured when a sequence is recorded."""

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
    pattern: int = 0

    @classmethod
    def defaults(cls) -> "RecordedSettings":
        return cls()

    @classmethod
    def from_list(cls, data: Any) -> "RecordedSettings":
        out = cls.defaults()
        if not isinstance(data, list):
            return out
        values = list(data) + [None] * SEQUENCE_SETTING_COUNT
        out.octave = _int(values[0], OCTAVE_MIN, OCTAVE_MAX, out.octave)
        out.note_duration_ms = _int(
            values[1], NOTE_DURATION_MIN, NOTE_DURATION_MAX, out.note_duration_ms)
        out.velocity = _int(values[2], VELOCITY_MIN, VELOCITY_MAX, out.velocity)
        out.pan = _int(values[3], PAN_MIN, PAN_MAX, out.pan)
        out.voicing_mode = _enum_int(values[4], VoicingMode, out.voicing_mode)
        out.chord_roll_ms = _int(
            values[5], CHORD_ROLL_MIN, CHORD_ROLL_MAX, out.chord_roll_ms)
        out.min_notes = _int(
            values[6], CHORD_MIN_NOTES_MIN, CHORD_MIN_NOTES_MAX, out.min_notes)
        out.min_interval = _int(
            values[7], CHORD_MIN_INTERVAL_MIN, CHORD_MIN_INTERVAL_MAX,
            out.min_interval)
        out.inversion = _enum_int(values[8], InversionMode, out.inversion)
        out.arp_mode = _enum_int(values[9], ArpMode, out.arp_mode)
        out.pattern = _int(values[10], 0, MAX_CHORD_PATTERNS - 1, out.pattern)
        return out

    def to_list(self) -> list[int]:
        return [
            self.octave,
            self.note_duration_ms,
            self.velocity,
            self.pan,
            int(self.voicing_mode),
            self.chord_roll_ms,
            self.min_notes,
            self.min_interval,
            int(self.inversion),
            int(self.arp_mode),
            self.pattern,
        ]


@dataclass
class SequenceEvent:
    step: int = 0
    root_pc: int = 0
    chord_type: ChordType = ChordType.MAJOR
    add9: bool = False
    add11: bool = False
    add13: bool = False

    @property
    def chord_id(self) -> int:
        return chord_id(self.root_pc, self.chord_type,
                        self.add9, self.add11, self.add13)

    @property
    def label(self) -> str:
        return chord_label(self.chord_id)

    @classmethod
    def from_dict(cls, data: Any) -> Optional["SequenceEvent"]:
        step: Optional[int] = None
        raw_chord: Any = None
        if isinstance(data, list) and len(data) >= 2:
            step = data[0] if isinstance(data[0], int) else None
            raw_chord = data[1]
        elif isinstance(data, dict):
            step = data.get("step") if isinstance(data.get("step"), int) else None
            raw_chord = data.get("chord")
        if step is None:
            return None
        parsed = parse_chord_id(raw_chord)
        if parsed is None:
            return None
        root, typ, add9, add11, add13 = parsed
        return cls(step=step, root_pc=root, chord_type=typ,
                   add9=add9, add11=add11, add13=add13)

    def to_dict(self) -> list[int]:
        return [self.step, self.chord_id]


@dataclass
class Sequence:
    bars: int = 4
    steps_per_bar: int = SEQUENCE_DEFAULT_STEPS
    recorded: RecordedSettings = field(default_factory=RecordedSettings.defaults)
    live_mask: int = 0
    events: list[SequenceEvent] = field(default_factory=list)

    @classmethod
    def defaults(cls) -> "Sequence":
        return cls()

    @classmethod
    def from_dict(cls, data: Any) -> "Sequence":
        seq = cls.defaults()
        if not isinstance(data, dict):
            return seq
        seq.bars = clamp_bars(data.get("bars", seq.bars))
        seq.steps_per_bar = clamp_steps(
            data.get("steps_per_bar", seq.steps_per_bar))
        seq.recorded = RecordedSettings.from_list(data.get("recorded"))
        seq.live_mask = _int(
            data.get("live", 0), 0, SEQUENCE_LIVE_MASK_ALL, 0)
        raw_events = data.get("events")
        if isinstance(raw_events, list):
            loop = seq.loop_steps
            for raw in raw_events:
                ev = SequenceEvent.from_dict(raw)
                if ev is None or ev.step < 0 or ev.step >= loop:
                    continue
                seq.set_event(ev.step, ev)
        return seq

    def to_dict(self) -> dict:
        out: dict = {
            "bars": self.bars,
            "steps_per_bar": self.steps_per_bar,
        }
        if self.recorded != RecordedSettings.defaults():
            out["recorded"] = self.recorded.to_list()
        if self.live_mask:
            out["live"] = self.live_mask
        if self.events:
            out["events"] = [ev.to_dict() for ev in self.events]
        return out

    @property
    def loop_steps(self) -> int:
        return self.bars * self.steps_per_bar

    def is_empty(self) -> bool:
        return not self.events

    def is_default(self) -> bool:
        return (
            not self.events
            and self.bars == 4
            and self.steps_per_bar == SEQUENCE_DEFAULT_STEPS
            and self.live_mask == 0
            and self.recorded == RecordedSettings.defaults()
        )

    def set_event(self, step: int, event: SequenceEvent) -> bool:
        if step < 0 or step >= self.loop_steps:
            return False
        event.step = step
        for i, existing in enumerate(self.events):
            if existing.step == step:
                self.events[i] = event
                return True
        if len(self.events) >= MAX_SEQUENCE_EVENTS:
            return False
        self.events.append(event)
        self.events.sort(key=lambda e: e.step)
        return True

    def remove_event(self, step: int) -> None:
        self.events = [e for e in self.events if e.step != step]

    def event_at(self, step: int) -> Optional[SequenceEvent]:
        for event in self.events:
            if event.step == step:
                return event
            if event.step > step:
                break
        return None

    def set_live(self, index: int, live: bool) -> None:
        if not 0 <= index < SEQUENCE_SETTING_COUNT:
            return
        bit = 1 << index
        if live:
            self.live_mask |= bit
        else:
            self.live_mask &= ~bit

    def is_live(self, index: int) -> bool:
        return bool(self.live_mask & (1 << index))


@dataclass
class SequencerLibrary:
    slots: list[Sequence] = field(
        default_factory=lambda: [Sequence.defaults() for _ in range(SEQUENCE_SLOTS)]
    )

    def __post_init__(self) -> None:
        while len(self.slots) < SEQUENCE_SLOTS:
            self.slots.append(Sequence.defaults())
        del self.slots[SEQUENCE_SLOTS:]

    @classmethod
    def from_dict(cls, data: Any) -> "SequencerLibrary":
        lib = cls()
        if isinstance(data, dict):
            raw = data.get("sequences")
        elif isinstance(data, list):
            raw = data
        else:
            return lib
        if not isinstance(raw, list):
            return lib
        for i, item in enumerate(raw[:SEQUENCE_SLOTS]):
            if isinstance(item, dict):
                lib.slots[i] = Sequence.from_dict(item)
        return lib

    def to_dict(self) -> dict:
        return {
            "version": 1,
            "sequences": [
                None if slot.is_default() else slot.to_dict()
                for slot in self.slots
            ],
        }

    def is_empty(self) -> bool:
        return all(slot.is_default() for slot in self.slots)

    def slot(self, index: int) -> Sequence:
        index = clamp(index, 0, SEQUENCE_SLOTS - 1)
        return self.slots[index]


def sequence_file_name(bank: int, slot: int) -> str:
    """File name (not path) for a zero-based preset bank/slot."""
    return f"b{bank + 1}p{slot + 1}.json"


def iter_sequence_files() -> Iterable[tuple[int, int, str]]:
    from .bounds import NUM_BANKS, NUM_SLOTS

    for bank in range(NUM_BANKS):
        for slot in range(NUM_SLOTS):
            yield bank, slot, sequence_file_name(bank, slot)
