"""Global ``config.json`` model.

Mirrors ``keybchord/lib/core/config.h`` / ``config.cpp``: the same field names,
defaults, and integer clamping. ``from_dict`` is lenient (like the firmware's
``AppConfig::load``); the strict rules live in the validator.
"""

from __future__ import annotations

from typing import Any

from .bounds import (
    BASE_ROOT_MIDI_MAX,
    BASE_ROOT_MIDI_MIN,
    DISPLAY_CURSOR_MS_MAX,
    DISPLAY_CURSOR_MS_MIN,
    DISPLAY_MENU_MS_MAX,
    DISPLAY_MENU_MS_MIN,
    DISPLAY_PROMPT_MS_MAX,
    DISPLAY_PROMPT_MS_MIN,
    DISPLAY_REVERT_MS_MAX,
    DISPLAY_REVERT_MS_MIN,
    LED_FLASH_MS_MAX,
    LED_FLASH_MS_MIN,
    NOTE_RANGE_MAX,
    NOTE_RANGE_MIN,
    clamp,
)
from .enums import LedTarget, parse_led_target, LED_TARGET_JSON


def _clamp_int(value, minimum, maximum, fallback):
    try:
        return clamp(int(value), minimum, maximum)
    except (TypeError, ValueError):
        return fallback


def _as_bool(value, fallback):
    if isinstance(value, bool):
        return value
    return fallback


class AppConfig:
    """Global settings (``/config.json``)."""

    def __init__(self) -> None:
        self.din_enabled: bool = True
        self.midi_clock_enabled: bool = False
        self.base_root_midi: int = 60
        self.note_range_low: int = 48
        self.note_range_high: int = 84
        self.display_revert_ms: int = 1500
        self.display_prompt_ms: int = 5000
        self.cursor_timeout_ms: int = 5000
        self.menu_timeout_ms: int = 10000
        self.bpm_indicator: bool = True
        self.led_indicator: LedTarget = LedTarget.NUM_LOCK
        self.led_flash_ms: int = 40
        self.startup_preset: str = "B1:P1"
        self.debug_log_enabled: bool = True
        self.midi_monitor_enabled: bool = True

    @classmethod
    def defaults(cls) -> "AppConfig":
        return cls()

    @classmethod
    def from_dict(cls, data: Any) -> "AppConfig":
        cfg = cls()
        if not isinstance(data, dict):
            return cfg

        midi = data.get("midi")
        if isinstance(midi, dict):
            cfg.din_enabled = _as_bool(midi.get("din_enabled"), cfg.din_enabled)
            cfg.midi_clock_enabled = _as_bool(
                midi.get("clock_enabled"), cfg.midi_clock_enabled
            )

        chord = data.get("chord")
        if isinstance(chord, dict):
            cfg.base_root_midi = _clamp_int(
                chord.get("base_root_midi"), BASE_ROOT_MIDI_MIN, BASE_ROOT_MIDI_MAX,
                cfg.base_root_midi,
            )
            note_range = chord.get("note_range")
            if isinstance(note_range, list) and len(note_range) >= 2:
                cfg.note_range_low = _clamp_int(
                    note_range[0], NOTE_RANGE_MIN, NOTE_RANGE_MAX, cfg.note_range_low
                )
                cfg.note_range_high = _clamp_int(
                    note_range[1], NOTE_RANGE_MIN, NOTE_RANGE_MAX, cfg.note_range_high
                )
                # Never allow an inverted range (NFR-9).
                if cfg.note_range_low > cfg.note_range_high:
                    cfg.note_range_low, cfg.note_range_high = (
                        cfg.note_range_high,
                        cfg.note_range_low,
                    )

        display = data.get("display")
        if isinstance(display, dict):
            cfg.display_revert_ms = _clamp_int(
                display.get("revert_timeout_ms"),
                DISPLAY_REVERT_MS_MIN, DISPLAY_REVERT_MS_MAX, cfg.display_revert_ms,
            )
            cfg.display_prompt_ms = _clamp_int(
                display.get("prompt_timeout_ms"),
                DISPLAY_PROMPT_MS_MIN, DISPLAY_PROMPT_MS_MAX, cfg.display_prompt_ms,
            )
            cfg.cursor_timeout_ms = _clamp_int(
                display.get("cursor_timeout_ms"),
                DISPLAY_CURSOR_MS_MIN, DISPLAY_CURSOR_MS_MAX, cfg.cursor_timeout_ms,
            )
            cfg.menu_timeout_ms = _clamp_int(
                display.get("menu_timeout_ms"),
                DISPLAY_MENU_MS_MIN, DISPLAY_MENU_MS_MAX, cfg.menu_timeout_ms,
            )

        led = data.get("led")
        if isinstance(led, dict):
            cfg.bpm_indicator = _as_bool(led.get("bpm_indicator"), cfg.bpm_indicator)
            cfg.led_indicator = parse_led_target(led.get("led"))
            cfg.led_flash_ms = _clamp_int(
                led.get("flash_ms"), LED_FLASH_MS_MIN, LED_FLASH_MS_MAX, cfg.led_flash_ms
            )

        startup = data.get("startup_preset")
        if isinstance(startup, str):
            cfg.startup_preset = startup

        logging = data.get("logging")
        if isinstance(logging, dict):
            cfg.debug_log_enabled = _as_bool(
                logging.get("debug_log"), cfg.debug_log_enabled
            )
            cfg.midi_monitor_enabled = _as_bool(
                logging.get("midi_monitor"), cfg.midi_monitor_enabled
            )

        return cfg

    def to_dict(self) -> dict:
        return {
            "midi": {
                "din_enabled": self.din_enabled,
                "clock_enabled": self.midi_clock_enabled,
            },
            "chord": {
                "base_root_midi": self.base_root_midi,
                "note_range": [self.note_range_low, self.note_range_high],
            },
            "display": {
                "revert_timeout_ms": self.display_revert_ms,
                "prompt_timeout_ms": self.display_prompt_ms,
                "cursor_timeout_ms": self.cursor_timeout_ms,
                "menu_timeout_ms": self.menu_timeout_ms,
            },
            "led": {
                "bpm_indicator": self.bpm_indicator,
                "led": LED_TARGET_JSON[self.led_indicator],
                "flash_ms": self.led_flash_ms,
            },
            "startup_preset": self.startup_preset,
            "logging": {
                "debug_log": self.debug_log_enabled,
                "midi_monitor": self.midi_monitor_enabled,
            },
        }
