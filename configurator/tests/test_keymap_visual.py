"""Keyboard layout and keymap display formatting."""

from keybchord_configurator.models.enums import ChordQuality
from keybchord_configurator.models.keymap import (
    ACTION_TYPES,
    BINDABLE_KEYS,
    KEYMAP_PARAM_NAMES,
    KeyAction,
    default_keymap,
)
from keybchord_configurator.ui.keymap_visual import (
    ANSI_104_KEYS,
    CHORD_GRID,
    CHORD_ROOTS,
    HARDCODED_COMBOS,
    KEY_BY_NAME,
    LAYER_ALL,
    LAYER_NONE,
    LAYOUT_HEIGHT,
    LAYOUT_WIDTH,
    STRUM_FULL,
    STRUM_LIMITED,
    all_param_labels_known,
    binding_index,
    combo_label,
    describe_key,
    format_action,
    matches_layer,
)


def test_layout_is_full_size_ansi_104():
    assert len(ANSI_104_KEYS) == 104
    names = [spec.name for spec in ANSI_104_KEYS]
    assert len(names) == len(set(names))
    positions = [(spec.x, spec.y) for spec in ANSI_104_KEYS]
    assert len(positions) == len(set(positions))
    assert max(spec.x + spec.w for spec in ANSI_104_KEYS) == LAYOUT_WIDTH
    assert max(spec.y + spec.h for spec in ANSI_104_KEYS) == LAYOUT_HEIGHT


def test_every_bindable_key_has_one_physical_key():
    for key in BINDABLE_KEYS:
        matches = [spec for spec in ANSI_104_KEYS if spec.name == key]
        assert len(matches) == 1, key
        assert matches[0].kind == "bindable"


def test_chord_grid_is_complete():
    assert len(CHORD_GRID) == 36
    assert CHORD_ROOTS == (
        "Db", "Ab", "Eb", "Bb", "F", "C", "G", "D", "A", "E", "B", "F#",
    )
    for quality in ChordQuality:
        count = sum(1 for _root, q in CHORD_GRID.values() if q == quality)
        assert count == 12
    for key, (root, quality) in CHORD_GRID.items():
        spec = KEY_BY_NAME[key]
        assert spec.kind == "chord"
        assert spec.chord_root == root
        assert spec.chord_quality == quality


def test_strum_plate_sets():
    assert len(STRUM_FULL) == 24
    assert len(STRUM_LIMITED) == 10
    assert STRUM_LIMITED.issubset(STRUM_FULL)
    assert {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"}.issubset(
        STRUM_FULL
    )
    assert {"Kp0", "KpDot", "Kp1", "Kp9", "NumLock", "KpSlash", "KpStar"}.issubset(
        STRUM_FULL
    )
    assert {"Kp2", "Kp3", "Kp5", "Kp6", "Kp8", "Kp9"}.issubset(STRUM_LIMITED)
    assert "Kp1" not in STRUM_LIMITED
    for key in STRUM_FULL:
        assert KEY_BY_NAME[key].strum_full
        assert KEY_BY_NAME[key].kind == "strum"


def test_reserved_and_hardcoded_keys_are_described():
    assert HARDCODED_COMBOS[("Super", "Esc")] == "Panic"
    assert HARDCODED_COMBOS[("Super", "1")] == "Load preset 1"
    assert ("Super", "9") not in HARDCODED_COMBOS
    assert "clear" in KEY_BY_NAME["Esc"].fixed.lower()
    assert describe_key(KEY_BY_NAME["Q"]).startswith("Chord ")
    assert "Strum" in describe_key(KEY_BY_NAME["Kp5"])


def test_build_binding_index_groups_modifier_variants():
    by_key, by_combo = binding_index(default_keymap().bindings)
    assert len(by_key["F1"]) == 3
    assert by_combo[(None, "F1")] == 0
    assert by_combo[("Ctrl", "F1")] is not None
    assert by_combo[("Alt", "F1")] is not None
    assert by_key["Space"]


def test_layer_matching():
    assert matches_layer("Ctrl", LAYER_ALL)
    assert matches_layer(None, LAYER_ALL)
    assert matches_layer(None, LAYER_NONE)
    assert not matches_layer("Ctrl", LAYER_NONE)
    assert matches_layer("Ctrl", "Ctrl")
    assert not matches_layer("Alt", "Ctrl")


def test_all_action_types_have_labels():
    samples = {
        "open_menu": KeyAction(type="open_menu", menu="chord"),
        "cycle": KeyAction(type="cycle", param="chord_mode"),
        "set": KeyAction(type="set", param="chord_inversion", value="first"),
        "inc": KeyAction(type="inc", param="tempo"),
        "dec": KeyAction(type="dec", param="tempo"),
        "toggle": KeyAction(type="toggle", param="rhythm_enable", a=1, b=0),
        "ext": KeyAction(type="ext", value=11),
        "drum_mute": KeyAction(type="drum_mute", index=2),
        "preset_prev": KeyAction(type="preset_prev"),
        "preset_next": KeyAction(type="preset_next"),
        "preset_bank_prev": KeyAction(type="preset_bank_prev"),
        "preset_bank_next": KeyAction(type="preset_bank_next"),
        "preset_load": KeyAction(type="preset_load", slot=2),
        "preset_save": KeyAction(type="preset_save"),
        "preset_clear": KeyAction(type="preset_clear"),
        "tap_tempo": KeyAction(type="tap_tempo"),
    }
    assert set(samples) == set(ACTION_TYPES)
    for action_type, action in samples.items():
        label = format_action(action)
        assert label.short
        assert label.detail
        assert label.short != action_type or action_type in ("preset_save",)


def test_all_keymap_params_have_display_labels():
    assert all_param_labels_known()
    assert set(KEYMAP_PARAM_NAMES)


def test_format_enum_bool_and_pattern_values():
    assert format_action(
        KeyAction(type="set", param="chord_mode", value="arp_hold")
    ).short == "Play mode: Arp hold"
    assert format_action(
        KeyAction(type="set", param="chord_inversion", value=2)
    ).short == "Inversion: 2nd"
    assert format_action(
        KeyAction(type="set", param="strum_scale", value="blues")
    ).short == "Scale: Blues"
    assert format_action(
        KeyAction(type="set", param="rhythm_enable", value=1)
    ).short == "Rhythm: on"
    assert format_action(
        KeyAction(type="set", param="strum_layout", value="on")
    ).short == "Layout: Limited"
    assert format_action(
        KeyAction(type="set", param="strum_layout", value=False)
    ).short == "Layout: Full"
    assert format_action(
        KeyAction(type="set", param="rhythm_pattern", value=1),
        rhythm_names=["Rock 1", "Bossa Nova"],
    ).short == "Rhythm: Bossa Nova"
    assert format_action(
        KeyAction(type="set", param="bass_pattern", value="Walking"),
        bass_names=["Walking", "Half"],
    ).short == "Bass pat: Walking"


def test_format_extensions_drums_and_presets():
    assert format_action(KeyAction(type="ext", value=9)).short == "Add9"
    assert format_action(KeyAction(type="ext", value=11)).short == "Add11"
    assert format_action(KeyAction(type="ext", value=13)).short == "Add13"
    assert format_action(
        KeyAction(type="drum_mute", index=0)
    ).short == "Mute Kick"
    assert format_action(
        KeyAction(type="preset_load", slot=0)
    ).short == "Load P1"
    assert format_action(
        KeyAction(type="preset_save")
    ).short == "Save preset"


def test_toggle_detail_shows_both_values():
    label = format_action(
        KeyAction(type="toggle", param="chord_roll", a=0, b=50)
    )
    assert "\u2194" in label.detail
    assert "0" in label.detail
    assert "50" in label.detail


def test_combo_label():
    assert combo_label(None, "F1") == "F1"
    assert combo_label("Ctrl", "F1") == "Ctrl+F1"


def test_macro_display_helpers():
    from keybchord_configurator.models.enums import NumberKeyMode
    from keybchord_configurator.models.macro import MacroCommand, MacroConfig
    from keybchord_configurator.ui.keymap_visual import (
        describe_key,
        is_macro_key,
        macro_command_label,
        macro_key_cell_text,
        macro_key_summary,
    )

    cfg = MacroConfig()
    cfg.number_keys = NumberKeyMode.MACRO
    cfg.keys["1"] = [
        MacroCommand(type="set", param="tempo", value=88),
        MacroCommand(type="drum_mute", piece="kick", value="on"),
    ]

    assert is_macro_key("1", cfg)
    assert not is_macro_key("Kp1", cfg)
    assert not is_macro_key("1", MacroConfig())
    assert macro_key_cell_text(cfg, "1") == "2 cmds"
    assert macro_key_cell_text(cfg, "2") == "Macro"
    assert "Tempo" in macro_command_label(cfg.keys["1"][0]).short
    assert "mute" in macro_command_label(cfg.keys["1"][1]).short.lower()
    assert "2 commands" in macro_key_summary(cfg, "1")
    assert "Per-preset macro" in describe_key(
        KEY_BY_NAME["1"], macro_config=cfg, preset_label="B1:P1")
