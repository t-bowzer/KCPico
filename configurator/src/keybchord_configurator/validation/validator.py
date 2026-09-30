"""JSON validation.

Two layers:

* ``validate_keymap`` mirrors the firmware's strict keymap validation
  (``keymap_config.cpp``) exactly — the firmware *halts* on a bad keymap.
* ``validate_config`` / ``validate_preset_bank`` / ``validate_rhythm`` /
  ``validate_bass`` are stricter than the (lenient, silently-clamping) firmware
  loaders, so the configurator can catch mistakes the device would otherwise
  paper over. They report errors (reject/skip) and warnings (clamp/fallback).
"""

from __future__ import annotations

from typing import Any, Optional

from ..models.bass import BASS_NAMES, BASS_DEGREE_REST
from ..models.bounds import (
    BASE_ROOT_MIDI_MAX,
    BASE_ROOT_MIDI_MIN,
    BASS_COUNT,
    CHANNEL_MAX,
    CHANNEL_MIN,
    CHORD_MIN_INTERVAL_MAX,
    CHORD_MIN_INTERVAL_MIN,
    CHORD_MIN_NOTES_MAX,
    CHORD_MIN_NOTES_MIN,
    CHORD_ROLL_MAX,
    CHORD_ROLL_MIN,
    DISPLAY_CURSOR_MS_MAX,
    DISPLAY_CURSOR_MS_MIN,
    DISPLAY_MENU_MS_MAX,
    DISPLAY_MENU_MS_MIN,
    DISPLAY_PROMPT_MS_MAX,
    DISPLAY_PROMPT_MS_MIN,
    DISPLAY_REVERT_MS_MAX,
    DISPLAY_REVERT_MS_MIN,
    DRUM_NOTE_MAX,
    DRUM_NOTE_MIN,
    DRUM_VELOCITY_OFF,
    LED_FLASH_MS_MAX,
    LED_FLASH_MS_MIN,
    MAX_BASS_PATTERNS,
    MAX_RHYTHMS,
    NOTE_DURATION_MAX,
    NOTE_DURATION_MIN,
    NOTE_RANGE_MAX,
    NOTE_RANGE_MIN,
    NUM_BANKS,
    NUM_SLOTS,
    OCTAVE_MAX,
    OCTAVE_MIN,
    PAN_MAX,
    PAN_MIN,
    RHYTHM_COUNT,
    STRUM_ROOT_MAX,
    STRUM_ROOT_MIN,
    SWING_MAX,
    SWING_MIN,
    TEMPO_MAX,
    TEMPO_MIN,
    VELOCITY_MAX,
    VELOCITY_MIN,
)
from ..models.enums import (
    ARP_MODE_BY_JSON,
    EDIT_MENU_BY_JSON,
    EDIT_MENU_JSON,
    INVERSION_BY_JSON,
    LED_TARGET_BY_JSON,
    PLAY_MODE_BY_JSON,
    SCALE_TYPE_BY_JSON,
    STRUM_MODE_BY_JSON,
    VOICING_MODE_BY_JSON,
)
from ..models.keymap import (
    ACTION_TYPES,
    BINDABLE_KEYS,
    BOOL_PARAMS,
    KEYMAP_PARAM_NAMES,
    MODIFIERS,
    PARAM_VALUE_TABLE,
    REQUIRED_ACTIONS,
    REQUIRED_MENUS,
    RESERVED_NAMED_KEYS,
)
from ..models.preset import DRUM_PIECES
from ..models.rhythm import RHYTHM_NAMES
from .errors import ValidationResult


# --- helpers --------------------------------------------------------------

def _is_int(value: Any) -> bool:
    return isinstance(value, int) and not isinstance(value, bool)


def _is_bool(value: Any) -> bool:
    return isinstance(value, bool)


def _is_str(value: Any) -> bool:
    return isinstance(value, str)


def _check_int(result: ValidationResult, where: str, data: dict, key: str,
               minimum: int, maximum: int, required: bool = False) -> Optional[int]:
    if key not in data:
        if required:
            result.add_error(f"{where}: missing required field '{key}'")
        return None
    value = data[key]
    if not _is_int(value):
        result.add_error(f"{where}: '{key}' must be an integer")
        return None
    if value < minimum or value > maximum:
        result.add_warning(f"{where}: '{key}' = {value} out of range "
                           f"[{minimum}..{maximum}] (will be clamped)")
    return value


def _check_bool(result: ValidationResult, where: str, data: dict, key: str) -> None:
    if key not in data:
        return
    if not _is_bool(data[key]):
        result.add_error(f"{where}: '{key}' must be a boolean")


def _check_enum(result: ValidationResult, where: str, data: dict, key: str,
                table: dict[str, Any]) -> None:
    if key not in data:
        return
    value = data[key]
    if _is_str(value):
        if value not in table:
            result.add_error(f"{where}: '{key}' has unknown value '{value}'")
    elif _is_int(value):
        result.add_warning(f"{where}: '{key}' is an integer; a string is expected "
                           f"(will fall back to the default)")
    else:
        result.add_error(f"{where}: '{key}' must be a string")


def _parse_integer(s: str) -> Optional[int]:
    if not s:
        return None
    i = 0
    neg = False
    if s[0] in ("-", "+"):
        neg = s[0] == "-"
        i = 1
    if i >= len(s):
        return None
    v = 0
    for ch in s[i:]:
        if ch < "0" or ch > "9":
            return None
        v = v * 10 + (ord(ch) - ord("0"))
    return -v if neg else v


# --- keymap ---------------------------------------------------------------

def validate_keymap(data: Any, *, rhythm_names: Optional[list[str]] = None,
                    bass_names: Optional[list[str]] = None) -> ValidationResult:
    result = ValidationResult()
    rhythm_names = list(rhythm_names) if rhythm_names else list(RHYTHM_NAMES)
    bass_names = list(bass_names) if bass_names else list(BASS_NAMES)

    if not isinstance(data, dict) or not isinstance(data.get("bindings"), list):
        result.add_error("top-level 'bindings' array is required")
        return result

    seen: set[tuple[Optional[str], str]] = set()
    coverage_menus: set[str] = set()
    coverage_actions: set[str] = set()

    for idx, bv in enumerate(data["bindings"]):
        where = f"bindings[{idx}]"

        if not isinstance(bv, dict):
            result.add_error(f"{where}: not an object")
            continue

        keys = bv.get("keys")
        if not isinstance(keys, list) or len(keys) == 0 or len(keys) > 2:
            result.add_error(f"{where}: 'keys' must be 1 or 2 entries")
            continue

        mods: Optional[str] = None
        key: Optional[str] = None
        key_err = False

        for kv in keys:
            if not _is_str(kv):
                result.add_error(f"{where}: 'keys' entries must be strings")
                key_err = True
                break
            if kv in MODIFIERS:
                if mods is not None:
                    result.add_error(f"{where}: multiple modifiers in 'keys'")
                    key_err = True
                    break
                mods = kv
            elif kv in BINDABLE_KEYS or kv in RESERVED_NAMED_KEYS:
                if key is not None:
                    result.add_error(f"{where}: multiple keys in 'keys'")
                    key_err = True
                    break
                key = kv
            else:
                result.add_error(f"{where}: unknown key '{kv}'")
                key_err = True
                break

        if key_err:
            continue
        if key is None:
            result.add_error(f"{where}: no non-modifier key")
            continue
        if key in RESERVED_NAMED_KEYS:
            result.add_error(f"{where}: key is reserved")
            continue

        pair = (mods, key)
        if pair in seen:
            result.add_error(f"{where}: key already bound")
            continue
        seen.add(pair)

        act = bv.get("action")
        if not isinstance(act, dict):
            result.add_error(f"{where}: 'action' object is required")
            continue

        act_type = act.get("type")
        if not _is_str(act_type):
            result.add_error(f"{where}: action 'type' is required")
            continue
        if act_type not in ACTION_TYPES:
            result.add_error(f"{where}: unknown action type '{act_type}'")
            continue

        def need_param() -> Optional[str]:
            param = act.get("param")
            if not _is_str(param):
                result.add_error(f"{where}: action 'param' is required")
                return None
            if param not in KEYMAP_PARAM_NAMES:
                result.add_error(f"{where}: unknown param '{param}'")
                return None
            return param

        def need_value(field: str, param: str) -> Optional[int]:
            if field not in act:
                result.add_error(f"{where}: action '{field}' is required")
                return None
            value = act[field]
            if _is_int(value):
                return value
            if _is_str(value):
                parsed = _parse_keymap_value(param, value, rhythm_names, bass_names)
                if parsed is None:
                    result.add_error(f"{where}: invalid value '{value}' for '{field}'")
                    return None
                return parsed
            result.add_error(f"{where}: action '{field}' must be a string or integer")
            return None

        if act_type == "open_menu":
            menu = act.get("menu")
            if not _is_str(menu) or menu not in EDIT_MENU_BY_JSON:
                result.add_error(f"{where}: invalid 'menu'")
            else:
                coverage_menus.add(menu)
        elif act_type in ("cycle", "inc", "dec"):
            need_param()
        elif act_type == "set":
            param = need_param()
            if param is not None:
                need_value("value", param)
        elif act_type == "toggle":
            param = need_param()
            if param is not None:
                need_value("a", param)
                need_value("b", param)
        elif act_type == "ext":
            value = act.get("value")
            if not _is_int(value):
                result.add_error(f"{where}: ext 'value' (integer) is required")
            elif value not in (9, 11, 13):
                result.add_error(f"{where}: ext 'value' must be 9, 11, or 13")
        elif act_type == "drum_mute":
            index = act.get("index")
            if not _is_int(index):
                result.add_error(f"{where}: drum_mute 'index' is required")
            elif index < 0 or index > 11:
                result.add_error(f"{where}: drum_mute 'index' out of range")
        elif act_type == "preset_load":
            slot = act.get("slot")
            if not _is_int(slot):
                result.add_error(f"{where}: preset_load 'slot' (integer) is required")
            elif slot > 7:
                result.add_error(f"{where}: preset_load 'slot' out of range")
        else:
            # preset_prev/next/bank_prev/bank_next/save/clear, tap_tempo.
            coverage_actions.add(act_type)

    if result.ok():
        for menu in REQUIRED_MENUS:
            name = EDIT_MENU_JSON[menu]
            if name not in coverage_menus:
                result.add_error(f"missing required binding: menu {name}")
        for action in REQUIRED_ACTIONS:
            if action not in coverage_actions:
                result.add_error(f"missing required binding: {action}")

    return result


def _parse_keymap_value(param: str, value: str, rhythm_names: list[str],
                        bass_names: list[str]) -> Optional[int]:
    as_int = _parse_integer(value)
    if as_int is not None:
        return as_int
    if param in BOOL_PARAMS:
        if value in ("on", "true"):
            return 1
        if value in ("off", "false"):
            return 0
        return None
    table = PARAM_VALUE_TABLE.get(param)
    if table is not None:
        return table.get(value)
    if param == "rhythm_pattern":
        try:
            return rhythm_names.index(value)
        except ValueError:
            return None
    if param == "bass_pattern":
        try:
            return bass_names.index(value)
        except ValueError:
            return None
    return None


# --- config.json ----------------------------------------------------------

def validate_config(data: Any) -> ValidationResult:
    result = ValidationResult()
    if not isinstance(data, dict):
        result.add_error("config.json must be a JSON object")
        return result

    midi = data.get("midi")
    if isinstance(midi, dict):
        _check_bool(result, "midi", midi, "din_enabled")
        _check_bool(result, "midi", midi, "clock_enabled")
    elif midi is not None:
        result.add_error("'midi' must be an object")

    chord = data.get("chord")
    if isinstance(chord, dict):
        _check_int(result, "chord", chord, "base_root_midi",
                   BASE_ROOT_MIDI_MIN, BASE_ROOT_MIDI_MAX)
        note_range = chord.get("note_range")
        if note_range is not None:
            if not isinstance(note_range, list) or len(note_range) < 2 or \
                    not all(_is_int(x) for x in note_range[:2]):
                result.add_error("chord: 'note_range' must be an array of two integers")
            else:
                for i in range(2):
                    if note_range[i] < NOTE_RANGE_MIN or note_range[i] > NOTE_RANGE_MAX:
                        result.add_warning(
                            f"chord: note_range[{i}] out of range "
                            f"[{NOTE_RANGE_MIN}..{NOTE_RANGE_MAX}] (will be clamped)")
                if note_range[0] > note_range[1]:
                    result.add_warning("chord: note_range is inverted (will be swapped)")
    elif chord is not None:
        result.add_error("'chord' must be an object")

    display = data.get("display")
    if isinstance(display, dict):
        _check_int(result, "display", display, "revert_timeout_ms",
                   DISPLAY_REVERT_MS_MIN, DISPLAY_REVERT_MS_MAX)
        _check_int(result, "display", display, "prompt_timeout_ms",
                   DISPLAY_PROMPT_MS_MIN, DISPLAY_PROMPT_MS_MAX)
        _check_int(result, "display", display, "cursor_timeout_ms",
                   DISPLAY_CURSOR_MS_MIN, DISPLAY_CURSOR_MS_MAX)
        _check_int(result, "display", display, "menu_timeout_ms",
                   DISPLAY_MENU_MS_MIN, DISPLAY_MENU_MS_MAX)
    elif display is not None:
        result.add_error("'display' must be an object")

    led = data.get("led")
    if isinstance(led, dict):
        _check_bool(result, "led", led, "bpm_indicator")
        _check_enum(result, "led", led, "led", LED_TARGET_BY_JSON)
        _check_int(result, "led", led, "flash_ms", LED_FLASH_MS_MIN, LED_FLASH_MS_MAX)
    elif led is not None:
        result.add_error("'led' must be an object")

    startup = data.get("startup_preset")
    if startup is not None and not _is_str(startup):
        result.add_error("'startup_preset' must be a string")
    elif _is_str(startup) and startup not in ("",):
        _check_preset_location(result, startup, "'startup_preset'")

    logging = data.get("logging")
    if isinstance(logging, dict):
        _check_bool(result, "logging", logging, "debug_log")
        _check_bool(result, "logging", logging, "midi_monitor")
    elif logging is not None:
        result.add_error("'logging' must be an object")

    return result


def _check_preset_location(result: ValidationResult, value: str, label: str) -> None:
    import re
    m = re.fullmatch(r"[Bb](\d+):[Pp](\d+)", value)
    if not m:
        result.add_warning(f"{label} = '{value}' is not in B#:P# form")
        return
    bank = int(m.group(1))
    slot = int(m.group(2))
    if bank < 1 or bank > NUM_BANKS:
        result.add_error(f"{label} bank {bank} out of range 1..{NUM_BANKS}")
    if slot < 1 or slot > NUM_SLOTS:
        result.add_error(f"{label} slot {slot} out of range 1..{NUM_SLOTS}")


# --- presets --------------------------------------------------------------

def validate_preset_bank(data: Any, *, rhythm_names: Optional[list[str]] = None,
                         bass_names: Optional[list[str]] = None) -> ValidationResult:
    result = ValidationResult()
    rhythm_names = list(rhythm_names) if rhythm_names else list(RHYTHM_NAMES)
    bass_names = list(bass_names) if bass_names else list(BASS_NAMES)

    if not isinstance(data, list):
        result.add_error("preset bank must be a JSON array")
        return result
    if len(data) > NUM_SLOTS:
        result.add_warning(f"preset bank has {len(data)} slots; only "
                           f"{NUM_SLOTS} are used")
    if len(data) < NUM_SLOTS:
        result.add_warning(f"preset bank has {len(data)} slots; missing slots "
                           f"fall back to defaults")

    for idx, slot in enumerate(data):
        where = f"slot[{idx}]"
        if not isinstance(slot, dict):
            result.add_error(f"{where}: not an object")
            continue
        _validate_preset_slot(result, where, slot, rhythm_names, bass_names)

    return result


def _validate_preset_slot(result: ValidationResult, where: str, slot: dict,
                          rhythm_names: list[str], bass_names: list[str]) -> None:
    name = slot.get("name")
    if name is not None and not _is_str(name):
        result.add_error(f"{where}: 'name' must be a string")

    chord = slot.get("chord")
    if isinstance(chord, dict):
        _check_enum(result, f"{where}.chord", chord, "play_mode", PLAY_MODE_BY_JSON)
        _check_int(result, f"{where}.chord", chord, "octave", OCTAVE_MIN, OCTAVE_MAX)
        _check_int(result, f"{where}.chord", chord, "note_duration_ms",
                   NOTE_DURATION_MIN, NOTE_DURATION_MAX)
        _check_int(result, f"{where}.chord", chord, "velocity",
                   VELOCITY_MIN, VELOCITY_MAX)
        _check_int(result, f"{where}.chord", chord, "pan", PAN_MIN, PAN_MAX)
        _check_enum(result, f"{where}.chord", chord, "voicing_mode",
                    VOICING_MODE_BY_JSON)
        _check_int(result, f"{where}.chord", chord, "chord_roll_ms",
                   CHORD_ROLL_MIN, CHORD_ROLL_MAX)
        _check_int(result, f"{where}.chord", chord, "min_notes",
                   CHORD_MIN_NOTES_MIN, CHORD_MIN_NOTES_MAX)
        _check_int(result, f"{where}.chord", chord, "min_interval",
                   CHORD_MIN_INTERVAL_MIN, CHORD_MIN_INTERVAL_MAX)
        _check_enum(result, f"{where}.chord", chord, "inversion", INVERSION_BY_JSON)
        _check_enum(result, f"{where}.chord", chord, "arp_mode", ARP_MODE_BY_JSON)
        _check_int(result, f"{where}.chord", chord, "channel", CHANNEL_MIN, CHANNEL_MAX)
    elif chord is not None:
        result.add_error(f"{where}: 'chord' must be an object")

    strum = slot.get("strum")
    if isinstance(strum, dict):
        _check_int(result, f"{where}.strum", strum, "octave", OCTAVE_MIN, OCTAVE_MAX)
        _check_int(result, f"{where}.strum", strum, "note_duration_ms",
                   NOTE_DURATION_MIN, NOTE_DURATION_MAX)
        _check_int(result, f"{where}.strum", strum, "velocity",
                   VELOCITY_MIN, VELOCITY_MAX)
        _check_bool(result, f"{where}.strum", strum, "limited_keys")
        _check_enum(result, f"{where}.strum", strum, "mode", STRUM_MODE_BY_JSON)
        _check_int(result, f"{where}.strum", strum, "root_pc",
                   STRUM_ROOT_MIN, STRUM_ROOT_MAX)
        _check_enum(result, f"{where}.strum", strum, "scale_type", SCALE_TYPE_BY_JSON)
        _check_int(result, f"{where}.strum", strum, "channel", CHANNEL_MIN, CHANNEL_MAX)
    elif strum is not None:
        result.add_error(f"{where}: 'strum' must be an object")

    bass = slot.get("bass")
    if isinstance(bass, dict):
        _check_bool(result, f"{where}.bass", bass, "enabled")
        _check_int(result, f"{where}.bass", bass, "octave", OCTAVE_MIN, OCTAVE_MAX)
        _check_int(result, f"{where}.bass", bass, "note_duration_ms",
                   NOTE_DURATION_MIN, NOTE_DURATION_MAX)
        _check_int(result, f"{where}.bass", bass, "velocity", VELOCITY_MIN, VELOCITY_MAX)
        _check_int(result, f"{where}.bass", bass, "channel", CHANNEL_MIN, CHANNEL_MAX)
        pattern = bass.get("pattern")
        if _is_str(pattern) and pattern not in bass_names:
            result.add_error(f"{where}.bass: pattern '{pattern}' does not match a "
                             f"loaded bass pattern")
    elif bass is not None:
        result.add_error(f"{where}: 'bass' must be an object")

    rhythm = slot.get("rhythm")
    if isinstance(rhythm, dict):
        _check_bool(result, f"{where}.rhythm", rhythm, "enabled")
        _check_int(result, f"{where}.rhythm", rhythm, "tempo", TEMPO_MIN, TEMPO_MAX)
        _check_int(result, f"{where}.rhythm", rhythm, "swing", SWING_MIN, SWING_MAX)
        _check_bool(result, f"{where}.rhythm", rhythm, "muted")
        _check_int(result, f"{where}.rhythm", rhythm, "channel",
                   CHANNEL_MIN, CHANNEL_MAX)
        pattern = rhythm.get("pattern")
        if _is_str(pattern) and pattern not in rhythm_names:
            result.add_error(f"{where}.rhythm: pattern '{pattern}' does not match a "
                             f"loaded rhythm pattern")
        drums = rhythm.get("drums")
        if isinstance(drums, dict):
            for key, _label, _default in DRUM_PIECES:
                _check_int(result, f"{where}.rhythm.drums", drums, key,
                           DRUM_NOTE_MIN, DRUM_NOTE_MAX)
                _check_int(result, f"{where}.rhythm.drums", drums, key + "_vel",
                           0, DRUM_VELOCITY_OFF)
        elif drums is not None:
            result.add_error(f"{where}: 'rhythm.drums' must be an object")
    elif rhythm is not None:
        result.add_error(f"{where}: 'rhythm' must be an object")


# --- patterns -------------------------------------------------------------

def validate_rhythm(data: Any, *, filename: str = "") -> ValidationResult:
    result = ValidationResult()
    where = f"rhythm '{filename}'" if filename else "rhythm"
    if not isinstance(data, dict):
        result.add_error(f"{where}: must be a JSON object")
        return result

    name = data.get("name")
    if name is None or not _is_str(name) or not name:
        result.add_error(f"{where}: 'name' (string) is required")

    short_name = data.get("short_name")
    if short_name is not None:
        if not _is_str(short_name):
            result.add_error(f"{where}: 'short_name' must be a string")
        elif len(short_name) > 2:
            result.add_warning(f"{where}: 'short_name' will be truncated to "
                               f"2 characters")

    steps = data.get("steps_per_bar")
    if _is_int(steps):
        if steps <= 0:
            result.add_error(f"{where}: 'steps_per_bar' must be positive")
        elif steps % 4 != 0:
            result.add_warning(f"{where}: 'steps_per_bar' = {steps} is not a "
                               f"multiple of 4 (4 steps = 1 beat)")
    else:
        result.add_warning(f"{where}: 'steps_per_bar' missing or not an integer "
                           f"(defaults to 16)")

    swing = data.get("swing")
    if swing is not None and not _is_int(swing):
        result.add_error(f"{where}: 'swing' must be an integer")

    tracks = data.get("tracks")
    if not isinstance(tracks, list) or len(tracks) == 0:
        result.add_error(f"{where}: 'tracks' must be a non-empty array (the "
                         f"firmware skips patterns with no tracks)")
        return result

    for i, track in enumerate(tracks):
        t_where = f"{where}.tracks[{i}]"
        if not isinstance(track, dict):
            result.add_error(f"{t_where}: not an object")
            continue
        note = track.get("note")
        if not _is_int(note):
            result.add_error(f"{t_where}: 'note' (integer) is required")
        elif note < DRUM_NOTE_MIN or note > DRUM_NOTE_MAX:
            result.add_warning(f"{t_where}: 'note' out of range "
                               f"[{DRUM_NOTE_MIN}..{DRUM_NOTE_MAX}]")
        pattern = track.get("pattern")
        if not isinstance(pattern, list) or len(pattern) == 0:
            result.add_error(f"{t_where}: 'pattern' must be a non-empty array")
            continue
        for v in pattern:
            if not _is_int(v) or v < 0 or v > 127:
                result.add_warning(f"{t_where}: pattern values must be integers "
                                   f"0..127 (non-integers become 0, out-of-range "
                                   f"values are clamped)")
                break
        if _is_int(steps) and len(pattern) != steps:
            result.add_warning(f"{t_where}: pattern length {len(pattern)} != "
                               f"steps_per_bar {steps}")

    return result


def validate_bass(data: Any, *, filename: str = "") -> ValidationResult:
    result = ValidationResult()
    where = f"bass '{filename}'" if filename else "bass"
    if not isinstance(data, dict):
        result.add_error(f"{where}: must be a JSON object")
        return result

    name = data.get("name")
    if name is None or not _is_str(name) or not name:
        result.add_error(f"{where}: 'name' (string) is required")

    steps = data.get("steps_per_bar")
    if _is_int(steps) and steps % 4 != 0:
        result.add_warning(f"{where}: 'steps_per_bar' = {steps} is not a "
                           f"multiple of 4 (4 steps = 1 beat)")

    step_data = data.get("steps")
    if not isinstance(step_data, list) or len(step_data) == 0:
        result.add_error(f"{where}: 'steps' must be a non-empty array (the "
                         f"firmware skips patterns with no steps)")
        return result

    for v in step_data:
        if not _is_int(v) or v < BASS_DEGREE_REST or v > 3:
            result.add_warning(f"{where}: 'steps' values must be integers "
                               f"-1..3 (non-integers become -1, out-of-range "
                               f"values are clamped)")
            break

    sustain = data.get("sustain_steps")
    if sustain is not None:
        if not isinstance(sustain, list):
            result.add_error(f"{where}: 'sustain_steps' must be an array")
        else:
            if len(sustain) != len(step_data):
                result.add_warning(f"{where}: 'sustain_steps' length "
                                   f"{len(sustain)} != 'steps' length "
                                   f"{len(step_data)}")
            for v in sustain:
                if not _is_int(v) or v < 0 or v > 64:
                    result.add_warning(f"{where}: 'sustain_steps' values must be "
                                       f"integers 0..64 (clamped)")
                    break

    hold = data.get("hold")
    if hold is not None and not _is_bool(hold):
        result.add_error(f"{where}: 'hold' must be a boolean")

    return result


# --- document dispatch ----------------------------------------------------

def validate_document(path: str, data: Any, *, rhythm_names: Optional[list[str]] = None,
                      bass_names: Optional[list[str]] = None) -> ValidationResult:
    """Validate a single parsed JSON document by its on-disk path."""
    base = path.replace("\\", "/").strip("/")
    if base == "config.json":
        return validate_config(data)
    if base == "keymap.json":
        return validate_keymap(data, rhythm_names=rhythm_names, bass_names=bass_names)
    if base.startswith("presets/") or "/presets/" in path:
        return validate_preset_bank(data, rhythm_names=rhythm_names,
                                    bass_names=bass_names)
    if base.startswith("rhythms/") or "/rhythms/" in path:
        return validate_rhythm(data, filename=base.rsplit("/", 1)[-1])
    if base.startswith("bass/") or "/bass/" in path:
        return validate_bass(data, filename=base.rsplit("/", 1)[-1])
    result = ValidationResult()
    result.add_warning(f"'{path}' is not a recognized configuration file")
    return result
