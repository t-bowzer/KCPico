"""Validator parity tests (mirroring firmware rules)."""

import pytest

from keybchord_configurator.models.config import AppConfig
from keybchord_configurator.models.defaults import (
    BUILTIN_BASS,
    BUILTIN_CHORD_PATTERNS,
    BUILTIN_RHYTHMS,
)
from keybchord_configurator.models.keymap import default_keymap
from keybchord_configurator.validation.validator import (
    validate_bass,
    validate_chord_pattern,
    validate_config,
    validate_keymap,
    validate_preset_bank,
    validate_rhythm,
)


def test_default_keymap_is_valid():
    result = validate_keymap(default_keymap().to_dict())
    assert result.ok(), result.errors


def test_keymap_missing_bindings():
    assert not validate_keymap({}).ok()
    assert not validate_keymap({"bindings": "nope"}).ok()


def test_keymap_reserved_key():
    data = {"bindings": [{"keys": ["1"], "action": {"type": "tap_tempo"}}]}
    result = validate_keymap(data)
    assert any("reserved" in e for e in result.errors)


def test_keymap_unknown_key():
    data = {"bindings": [{"keys": ["Tab"], "action": {"type": "tap_tempo"}}]}
    result = validate_keymap(data)
    assert any("unknown key 'Tab'" in e for e in result.errors)


def test_keymap_duplicate_binding():
    data = {"bindings": [
        {"keys": ["F1"], "action": {"type": "tap_tempo"}},
        {"keys": ["F1"], "action": {"type": "tap_tempo"}},
    ]}
    result = validate_keymap(data)
    assert any("already bound" in e for e in result.errors)


def test_keymap_bad_action_type():
    data = {"bindings": [{"keys": ["F1"], "action": {"type": "explode"}}]}
    result = validate_keymap(data)
    assert any("unknown action type" in e for e in result.errors)


def test_keymap_invalid_enum_value():
    data = {"bindings": [
        {"keys": ["F1"], "action": {"type": "set", "param": "chord_mode",
                                    "value": "nope"}},
    ]}
    result = validate_keymap(data)
    assert any("invalid value 'nope'" in e for e in result.errors)


def test_keymap_required_coverage():
    # A keymap with only a tap_tempo binding is missing all required coverage.
    data = {"bindings": [{"keys": ["F1"], "action": {"type": "tap_tempo"}}]}
    result = validate_keymap(data)
    missing = [e for e in result.errors if e.startswith("missing required")]
    assert len(missing) == 8  # 5 menus + preset_prev/next/save


def test_keymap_pattern_value_resolution():
    data = {"bindings": [
        {"keys": ["F1"], "action": {"type": "set", "param": "bass_pattern",
                                    "value": "Whole"}},
    ]}
    assert any("invalid value" in e for e in validate_keymap(data).errors) is False
    data2 = {"bindings": [
        {"keys": ["F1"], "action": {"type": "set", "param": "bass_pattern",
                                    "value": "NoSuchPattern"}},
    ]}
    assert any("invalid value" in e for e in validate_keymap(data2).errors)


def test_config_validates_clean():
    assert validate_config(AppConfig.defaults().to_dict()).ok()


def test_config_out_of_range_warns():
    data = AppConfig.defaults().to_dict()
    data["led"]["flash_ms"] = 99999
    result = validate_config(data)
    assert any("flash_ms" in w for w in result.warnings)


def test_config_bad_type_errors():
    result = validate_config({"display": "not-an-object"})
    assert any("'display' must be an object" in e for e in result.errors)


def test_preset_unknown_pattern_errors():
    slot = {
        "bass": {"pattern": "Does Not Exist"},
        "rhythm": {"pattern": "Also Missing"},
        "chord": {"pattern": "Nope"},
    }
    result = validate_preset_bank([slot] * 8)
    assert any("bass" in e for e in result.errors)
    assert any("rhythm" in e for e in result.errors)
    assert any("chord" in e for e in result.errors)


def test_rhythm_empty_tracks_errors():
    assert not validate_rhythm({"name": "X", "tracks": []}).ok()


def test_bass_empty_steps_errors():
    assert not validate_bass({"name": "X", "steps": []}).ok()


def test_chord_pattern_empty_steps_errors():
    assert not validate_chord_pattern({"name": "X", "steps": []}).ok()


def test_all_builtins_validate_clean():
    for i, data in enumerate(BUILTIN_RHYTHMS):
        assert validate_rhythm(data, filename=f"{i}.json").ok()
    for i, data in enumerate(BUILTIN_BASS):
        assert validate_bass(data, filename=f"{i}.json").ok()
    for i, data in enumerate(BUILTIN_CHORD_PATTERNS):
        assert validate_chord_pattern(data, filename=f"{i}.json").ok()


def test_arrows_are_bindable():
    from keybchord_configurator.models.keymap import BINDABLE_KEYS
    for key in ("Left", "Down", "Right", "Up"):
        assert key in BINDABLE_KEYS


def test_keymap_ext_action_valid():
    from keybchord_configurator.models.keymap import default_keymap
    data = default_keymap().to_dict()
    ext_bindings = [b for b in data["bindings"] if b["action"]["type"] == "ext"]
    assert len(ext_bindings) == 3
    assert {b["action"]["value"] for b in ext_bindings} == {9, 11, 13}
    assert validate_keymap(data).ok()


def test_keymap_ext_bad_value_rejected():
    data = {"bindings": [
        {"keys": ["Left"], "action": {"type": "ext", "value": 12}},
    ]}
    result = validate_keymap(data)
    assert any("must be 9, 11, or 13" in e for e in result.errors)


def _macro_slot(**macro_fields):
    from keybchord_configurator.models.preset import PresetSlot
    data = PresetSlot.defaults().to_dict()
    data.update(macro_fields)
    return data


def test_macros_valid_slot():
    data = _macro_slot(
        number_keys="macro",
        macros={
            "1": [
                {"type": "set", "param": "rhythm_mute", "value": "off"},
                {"type": "set", "param": "tempo", "value": 88},
                {"type": "drum_mute", "piece": "kick", "value": "on"},
            ],
            "2": [{"type": "toggle", "param": "rhythm_enable", "a": 1, "b": 0}],
        },
    )
    result = validate_preset_bank([data] * 8)
    assert result.ok(), result.errors


def test_macros_reject_bad_mode_type_and_piece():
    data = _macro_slot(
        number_keys="nope",
        macros={
            "1": [
                {"type": "explode", "param": "tempo"},
                {"type": "set", "param": "not_a_param", "value": 1},
                {"type": "drum_mute", "piece": "not_a_drum", "value": "on"},
            ],
        },
    )
    result = validate_preset_bank([data] * 8)
    text = "\n".join(result.errors)
    assert "number_keys" in text
    assert "unknown macro command type" in text
    assert "unknown macro param" in text
    assert "drum_mute 'piece'" in text


def test_macros_reject_over_limits():
    from keybchord_configurator.models.macro import (
        MAX_MACRO_COMMANDS_PER_KEY,
        MAX_MACRO_COMMANDS_PER_PRESET,
    )

    per_key = [{"type": "set", "param": "tempo", "value": 88}] * (
        MAX_MACRO_COMMANDS_PER_KEY + 1)
    data = _macro_slot(number_keys="macro", macros={"1": per_key})
    result = validate_preset_bank([data] * 8)
    assert any("key limit" in e for e in result.errors)

    macros = {}
    remaining = MAX_MACRO_COMMANDS_PER_PRESET
    for label in ("1", "2", "3", "4"):
        macros[label] = [{"type": "set", "param": "tempo", "value": 88}] * \
            min(MAX_MACRO_COMMANDS_PER_KEY, remaining + 1)
        remaining -= len(macros[label])
    data = _macro_slot(number_keys="macro", macros=macros)
    result = validate_preset_bank([data] * 8)
    assert any("preset limit" in e for e in result.errors)


def test_macros_reject_unknown_number_key():
    data = _macro_slot(
        number_keys="macro",
        macros={"99": [{"type": "set", "param": "tempo", "value": 88}]},
    )
    result = validate_preset_bank([data] * 8)
    assert any("unknown number key" in e for e in result.errors)


def test_macro_object_with_name_is_valid():
    data = _macro_slot(
        number_keys="macro",
        macros={
            "1": {
                "name": "Verse",
                "commands": [
                    {"type": "set", "param": "tempo", "value": 88},
                ],
            },
        },
    )
    result = validate_preset_bank([data] * 8)
    assert result.ok(), result.errors


def test_macro_midi_command_is_valid():
    data = _macro_slot(
        number_keys="macro",
        macros={
            "1": [
                {"type": "midi", "bytes": [0xB0, 7, 100]},
                {"type": "midi", "bytes": [0xF0, 0x41, 0x10, 0xF7],
                 "delay_ms": 250},
            ],
        },
    )
    result = validate_preset_bank([data] * 8)
    assert result.ok(), result.errors


def test_macro_midi_command_rejects_bad_bytes_and_delay():
    from keybchord_configurator.models.midi import (
        MAX_MIDI_BYTES,
        MAX_MIDI_DELAY_MS,
    )

    data = _macro_slot(
        number_keys="macro",
        macros={
            "1": [
                {"type": "midi", "bytes": []},
                {"type": "midi", "bytes": [1, 300]},
                {"type": "midi", "bytes": [1, 2, 3],
                 "delay_ms": MAX_MIDI_DELAY_MS + 1},
                {"type": "midi", "bytes": [0] * (MAX_MIDI_BYTES + 1)},
            ],
        },
    )
    result = validate_preset_bank([data] * 8)
    text = "\n".join(result.errors)
    assert "must not be empty" in text
    assert "must be an integer 0-255" in text
    assert "delay_ms" in text
    assert "exceeds" in text


def test_macro_name_too_long_errors():
    data = _macro_slot(
        number_keys="macro",
        macros={
            "1": {
                "name": "x" * 17,
                "commands": [{"type": "set", "param": "tempo", "value": 88}],
            },
        },
    )
    result = validate_preset_bank([data] * 8)
    assert any("'name' exceeds" in e for e in result.errors)
