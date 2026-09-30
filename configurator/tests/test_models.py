"""Model serialization round-trip and defaults."""

import json
from pathlib import Path

import pytest

from keybchord_configurator.models.bass import BASS_NAMES, BassPattern
from keybchord_configurator.models.config import AppConfig
from keybchord_configurator.models.defaults import BUILTIN_BASS, BUILTIN_RHYTHMS
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
    for i, data in enumerate(BUILTIN_RHYTHMS):
        pattern = RhythmPattern.from_dict(data)
        assert pattern.name == RHYTHM_NAMES[i]
    for i, data in enumerate(BUILTIN_BASS):
        pattern = BassPattern.from_dict(data)
        assert pattern.name == BASS_NAMES[i]


def test_keymap_default_roundtrip():
    km = default_keymap()
    data = km.to_dict()
    assert data["bindings"][0]["keys"] == ["F1"]
    assert KeymapConfig.from_dict(data).to_dict() == data
