"""Model serialization round-trip and defaults."""

import json
from pathlib import Path

import pytest

from keybchord_configurator.models.bass import BASS_NAMES, BassPattern
from keybchord_configurator.models.chordpattern import CHORD_PATTERN_NAMES, ChordPattern
from keybchord_configurator.models.config import AppConfig
from keybchord_configurator.models.defaults import (
    BUILTIN_BASS,
    BUILTIN_CHORD_PATTERNS,
    BUILTIN_RHYTHMS,
)
from keybchord_configurator.models.keymap import KeymapConfig, default_keymap
from keybchord_configurator.models.preset import DRUM_PIECES, PresetSlot
from keybchord_configurator.models.rhythm import RHYTHM_NAMES, RhythmPattern

EXAMPLES = Path(__file__).resolve().parents[2] / "examples"


def test_appconfig_defaults_roundtrip():
    cfg = AppConfig.defaults()
    data = cfg.to_dict()
    assert data["midi"]["din_enabled"] is True
    assert data["chord"]["base_root_midi"] == 60
    assert data["chord"]["note_range"] == [48, 84]
    assert data["startup_preset"] == "B1:P1"
    assert AppConfig.from_dict(data).to_dict() == data


def test_appconfig_inverted_note_range_is_swapped():
    cfg = AppConfig.from_dict({"chord": {"note_range": [84, 48]}})
    assert cfg.note_range_low == 48
    assert cfg.note_range_high == 84


def test_preset_defaults_match_schema():
    slot = PresetSlot.defaults()
    data = slot.to_dict()
    assert data["name"] == "Default"
    assert data["chord"]["play_mode"] == "held"
    assert data["chord"]["voicing_mode"] == "root_position"
    assert data["strum"]["mode"] == "follow_chord"
    assert data["bass"]["pattern"] == "Walking"
    assert data["rhythm"]["pattern"] == "Rock 1"
    assert set(data["rhythm"]["drums"].keys()) == {
        *[k for k, _l, _d in DRUM_PIECES],
        *[k + "_vel" for k, _l, _d in DRUM_PIECES],
    }


def test_preset_partial_falls_back_to_defaults():
    slot = PresetSlot.from_dict({"name": "Lush", "chord": {"velocity": 110}})
    assert slot.name == "Lush"
    assert slot.chord.velocity == 110
    assert slot.chord.pan == 64
    assert slot.strum.octave == 1


def test_rhythm_example_roundtrip():
    data = json.loads((EXAMPLES / "test_rhythm.json").read_text())
    pattern = RhythmPattern.from_dict(data)
    assert pattern.name == "Test Groove"
    assert pattern.short_name == "TG"
    assert pattern.steps_per_bar == 16
    assert len(pattern.tracks) == 5
    assert pattern.to_dict()["tracks"][0]["pattern"][0] == 1


def test_bass_example_roundtrip():
    data = json.loads((EXAMPLES / "test_bass.json").read_text())
    pattern = BassPattern.from_dict(data)
    assert pattern.name == "Test Bass"
    assert pattern.steps[0] == 0
    assert pattern.steps[8] == 2
    assert pattern.sustain_steps[0] == 8
    assert pattern.valid()
    assert pattern.to_dict()["steps"] == data["steps"]


def test_builtin_tables_consistent():
    assert len(BUILTIN_RHYTHMS) == len(RHYTHM_NAMES) == 12
    assert len(BUILTIN_BASS) == len(BASS_NAMES) == 9
    assert len(BUILTIN_CHORD_PATTERNS) == len(CHORD_PATTERN_NAMES) == 9
    for i, data in enumerate(BUILTIN_RHYTHMS):
        pattern = RhythmPattern.from_dict(data)
        assert pattern.name == RHYTHM_NAMES[i]
    for i, data in enumerate(BUILTIN_BASS):
        pattern = BassPattern.from_dict(data)
        assert pattern.name == BASS_NAMES[i]
    for i, data in enumerate(BUILTIN_CHORD_PATTERNS):
        pattern = ChordPattern.from_dict(data)
        assert pattern.name == CHORD_PATTERN_NAMES[i]


def test_chord_pattern_roundtrip():
    data = {"name": "Alberti", "steps_per_bar": 16,
            "steps": [1, 0, 0, 0, 4, 0, 0, 0, 2, 0, 0, 0, 4, 0, 0, 0]}
    pattern = ChordPattern.from_dict(data)
    assert pattern.name == "Alberti"
    assert pattern.steps[0] == 1
    assert pattern.steps[4] == 4
    assert pattern.valid()
    assert pattern.to_dict()["steps"] == data["steps"]


def test_chord_pattern_clamps_bitmask():
    pattern = ChordPattern.from_dict({"name": "X", "steps": [1, 99, -3, 7]})
    assert pattern.steps == [1, 15, 0, 7]


def test_keymap_default_roundtrip():
    km = default_keymap()
    data = km.to_dict()
    assert data["bindings"][0]["keys"] == ["F1"]
    assert KeymapConfig.from_dict(data).to_dict() == data


def test_preset_macros_default_and_roundtrip():
    from keybchord_configurator.models.enums import NumberKeyMode
    from keybchord_configurator.models.macro import MacroCommand

    slot = PresetSlot.defaults()
    data = slot.to_dict()
    assert data["number_keys"] == "strum"
    assert data["macros"] == {}

    slot.macros.number_keys = NumberKeyMode.MACRO
    slot.macros.names["1"] = "Verse"
    slot.macros.keys["1"] = [
        MacroCommand(type="set", param="rhythm_mute", value="off"),
        MacroCommand(type="drum_mute", piece="kick", value="on"),
    ]
    slot.macros.keys["0"] = [
        MacroCommand(type="toggle", param="rhythm_enable", a=1, b=0),
    ]

    loaded = PresetSlot.from_dict(slot.to_dict())
    assert loaded.macros.number_keys == NumberKeyMode.MACRO
    assert loaded.macros.name_for("1") == "Verse"
    assert loaded.macros.name_for("0") == "M0"
    assert len(loaded.macros.keys["1"]) == 2
    assert loaded.macros.keys["1"][0].param == "rhythm_mute"
    assert loaded.macros.keys["1"][1].piece == "kick"
    assert loaded.macros.keys["0"][0].type == "toggle"


def test_old_preset_without_macros_loads_defaults():
    from keybchord_configurator.models.enums import NumberKeyMode

    slot = PresetSlot.from_dict({"name": "Old", "chord": {"velocity": 80}})
    assert slot.macros.number_keys == NumberKeyMode.STRUM
    assert slot.macros.keys == {}


def test_legacy_macro_array_gets_default_name():
    from keybchord_configurator.models.macro import MacroConfig
    from keybchord_configurator.models.enums import NumberKeyMode

    cfg = MacroConfig.from_dict({
        "number_keys": "macro",
        "macros": {
            "1": [{"type": "set", "param": "tempo", "value": 88}],
        },
    })
    assert cfg.number_keys == NumberKeyMode.MACRO
    assert cfg.name_for("1") == "M1"
    assert len(cfg.keys["1"]) == 1


def test_midi_message_encoding_roundtrip():
    from keybchord_configurator.models.midi import (
        MidiMessageSpec,
        decode_midi_bytes,
        parse_hex_bytes,
        format_hex_bytes,
    )

    cases = [
        MidiMessageSpec(kind="note_on", channel=2, number=60, value=100),
        MidiMessageSpec(kind="note_off", channel=2, number=60),
        MidiMessageSpec(kind="cc", channel=16, number=74, value=42),
        MidiMessageSpec(kind="program_change", channel=1, number=12),
        MidiMessageSpec(kind="pitch_bend", channel=3, bend=-4096),
        MidiMessageSpec(kind="channel_pressure", channel=4, value=99),
        MidiMessageSpec(kind="poly_pressure", channel=5, number=64, value=20),
        MidiMessageSpec(kind="sysex", bytes=[0x41, 0x10, 0x42, 0xF7]),
        MidiMessageSpec(kind="raw", bytes=[0xF8]),
    ]
    for spec in cases:
        encoded = spec.encoded()
        decoded = decode_midi_bytes(encoded)
        assert decoded.encoded() == encoded
    assert parse_hex_bytes("F0 41, 10 42 F7") == [0xF0, 0x41, 0x10, 0x42, 0xF7]
    assert format_hex_bytes([0, 15, 255]) == "00 0F FF"


def test_midi_macro_roundtrip():
    from keybchord_configurator.models.macro import MacroCommand

    command = MacroCommand(
        type="midi", bytes=[0xB0, 7, 100], delay_ms=250)
    assert command.to_dict() == {
        "type": "midi", "bytes": [0xB0, 7, 100], "delay_ms": 250}
    loaded = MacroCommand.from_dict(command.to_dict())
    assert loaded == command
    assert "Volume" in command.summary()


def test_midi_library_parses_groups_and_expands_messages():
    from keybchord_configurator.models.library import MidiLibrary

    library = MidiLibrary.from_dict({
        "name": "Test Device",
        "groups": [{
            "name": "Programs",
            "commands": [{
                "name": "Bank A Program 1",
                "messages": [
                    {"type": "cc", "channel": 1, "controller": 0, "value": 85},
                    {"type": "cc", "channel": 1, "controller": 32, "value": 64},
                    {"type": "program_change", "channel": 1, "program": 0},
                ],
            }],
        }],
    })
    assert library is not None
    assert library.command_count() == 1
    command = library.groups[0].commands[0]
    expanded = command.macro_commands()
    assert [c.bytes for c in expanded] == [
        [0xB0, 0, 85], [0xB0, 32, 64], [0xC0, 0]]
    assert all(c.type == "midi" for c in expanded)

    expanded16 = command.macro_commands(channel=16)
    assert [c.bytes for c in expanded16] == [
        [0xBF, 0, 85], [0xBF, 32, 64], [0xCF, 0]]


def test_midi_library_channel_override_leaves_sysex_unchanged():
    from keybchord_configurator.models.library import MidiLibraryCommand
    from keybchord_configurator.models.midi import MidiMessageSpec

    command = MidiLibraryCommand(name="Setup", messages=[
        MidiMessageSpec(kind="sysex", bytes=[0xF0, 0x7E, 0x7F, 0xF7]),
    ])
    assert command.macro_commands(channel=16)[0].bytes == [
        0xF0, 0x7E, 0x7F, 0xF7]


def test_sequence_chord_id_roundtrip_and_labels():
    from keybchord_configurator.models.enums import ChordType
    from keybchord_configurator.models.sequence import (
        SequenceEvent, chord_id, chord_label, parse_chord_id,
    )

    cid = chord_id(5, ChordType.MIN7, add9=True, add13=True)
    assert cid == 5 * 128 + int(ChordType.MIN7) * 8 + 5
    parsed = parse_chord_id(cid)
    assert parsed == (5, ChordType.MIN7, True, False, True)
    assert chord_label(cid) == "Fm7+9+13"
    assert SequenceEvent(root_pc=0, chord_type=ChordType.MAJOR).label == "C"
    assert parse_chord_id(99999) is None


def test_sequence_roundtrip_and_limits():
    from keybchord_configurator.models.enums import ChordType
    from keybchord_configurator.models.sequence import (
        MAX_SEQUENCE_EVENTS, Sequence, SequenceEvent, SequencerLibrary,
    )

    lib = SequencerLibrary()
    seq = lib.slot(1)
    seq.bars = 2
    seq.steps_per_bar = 16
    seq.live_mask = 0b101
    assert seq.set_event(0, SequenceEvent(step=0, root_pc=0,
                                          chord_type=ChordType.MAJOR))
    assert seq.set_event(16, SequenceEvent(step=16, root_pc=7,
                                           chord_type=ChordType.DOM7))
    assert not seq.set_event(32, SequenceEvent(step=32, root_pc=0,
                                               chord_type=ChordType.MAJOR))

    data = lib.to_dict()
    assert data["sequences"][0] is None
    assert data["sequences"][1]["bars"] == 2
    assert data["sequences"][1]["live"] == 5
    loaded = SequencerLibrary.from_dict(data)
    assert loaded.slot(1).bars == 2
    assert len(loaded.slot(1).events) == 2
    assert loaded.slot(1).events[1].root_pc == 7

    full = Sequence(bars=8)
    for i in range(MAX_SEQUENCE_EVENTS):
        assert full.set_event(i, SequenceEvent(step=i))
    assert not full.set_event(64, SequenceEvent(step=64))


def test_sequence_clamps_bars_and_steps():
    from keybchord_configurator.models.sequence import Sequence, clamp_bars

    assert clamp_bars(3) == 4
    assert clamp_bars(7) == 8
    seq = Sequence.from_dict({"bars": 3, "steps_per_bar": 999})
    assert seq.bars == 4
    assert seq.steps_per_bar == 64


def test_sequence_worst_case_json_fits_budget():
    import json

    from keybchord_configurator.models.enums import ChordType
    from keybchord_configurator.models.sequence import (
        MAX_SEQUENCE_EVENTS, SequenceEvent, SequencerLibrary,
    )

    lib = SequencerLibrary()
    for seq in lib.slots:
        seq.bars = 8
        seq.steps_per_bar = 16
        seq.recorded.octave = 1
        seq.recorded.velocity = 101
        seq.recorded.pattern = 3
        seq.live_mask = 0x5A5
        for i in range(MAX_SEQUENCE_EVENTS):
            seq.set_event(i * 2, SequenceEvent(
                root_pc=i % 12,
                chord_type=ChordType(i % 11),
                add9=bool(i & 1), add11=bool(i & 2), add13=bool(i & 4)))
    text = json.dumps(lib.to_dict(), separators=(",", ":"))
    assert len(text) < 7000
