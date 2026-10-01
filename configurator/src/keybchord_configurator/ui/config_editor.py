"""Global settings (``config.json``) editor."""

from __future__ import annotations

from PySide6.QtCore import Signal
from PySide6.QtWidgets import (
    QFormLayout,
    QGroupBox,
    QLineEdit,
    QVBoxLayout,
    QWidget,
)

from ..models.config import AppConfig
from ..models.enums import LedTarget, LED_TARGET_BY_JSON, LED_TARGET_JSON
from .widgets import bool_check, enum_combo, int_spin


class ConfigEditor(QWidget):
    changed = Signal()

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self._build()

    def _build(self) -> None:
        root = QVBoxLayout(self)

        midi_group = QGroupBox("MIDI")
        midi_form = QFormLayout(midi_group)
        self.din_enabled = bool_check(True, "DIN MIDI enabled")
        self.usb_midi_enabled = bool_check(True, "USB MIDI enabled")
        self.clock_enabled = bool_check(False, "MIDI clock out")
        midi_form.addRow(self.din_enabled)
        midi_form.addRow(self.usb_midi_enabled)
        midi_form.addRow(self.clock_enabled)
        root.addWidget(midi_group)

        chord_group = QGroupBox("Chord")
        chord_form = QFormLayout(chord_group)
        self.base_root_midi = int_spin(0, 127, 1, 60)
        self.note_range_low = int_spin(0, 127, 1, 48)
        self.note_range_high = int_spin(0, 127, 1, 84)
        chord_form.addRow("Base root MIDI", self.base_root_midi)
        chord_form.addRow("Note range low", self.note_range_low)
        chord_form.addRow("Note range high", self.note_range_high)
        root.addWidget(chord_group)

        display_group = QGroupBox("Display timeouts (ms)")
        display_form = QFormLayout(display_group)
        self.display_revert_ms = int_spin(250, 5000, 50, 1500)
        self.display_prompt_ms = int_spin(1000, 30000, 100, 5000)
        self.display_cursor_ms = int_spin(500, 30000, 100, 5000)
        self.display_menu_ms = int_spin(500, 30000, 100, 10000)
        display_form.addRow("Revert", self.display_revert_ms)
        display_form.addRow("Prompt", self.display_prompt_ms)
        display_form.addRow("Cursor", self.display_cursor_ms)
        display_form.addRow("Menu", self.display_menu_ms)
        root.addWidget(display_group)

        led_group = QGroupBox("LED")
        led_form = QFormLayout(led_group)
        self.bpm_indicator = bool_check(True, "BPM indicator")
        self.led_target = enum_combo(
            {"All": LedTarget.ALL, "Caps Lock": LedTarget.CAPS_LOCK,
             "Num Lock": LedTarget.NUM_LOCK, "Scroll Lock": LedTarget.SCROLL_LOCK},
            LedTarget.NUM_LOCK,
        )
        self.led_flash_ms = int_spin(5, 500, 5, 40)
        led_form.addRow(self.bpm_indicator)
        led_form.addRow("LED", self.led_target)
        led_form.addRow("Flash (ms)", self.led_flash_ms)
        root.addWidget(led_group)

        misc_group = QGroupBox("Startup & logging")
        misc_form = QFormLayout(misc_group)
        self.startup_preset = QLineEdit("B1:P1")
        self.debug_log = bool_check(True, "Debug log")
        self.midi_monitor = bool_check(True, "MIDI monitor")
        misc_form.addRow("Startup preset", self.startup_preset)
        misc_form.addRow(self.debug_log)
        misc_form.addRow(self.midi_monitor)
        root.addWidget(misc_group)
        root.addStretch(1)

        for widget in self._widgets():
            self._connect(widget)

    def _widgets(self):
        return [
            self.din_enabled, self.usb_midi_enabled, self.clock_enabled,
            self.base_root_midi,
            self.note_range_low, self.note_range_high, self.display_revert_ms,
            self.display_prompt_ms, self.display_cursor_ms, self.display_menu_ms,
            self.bpm_indicator, self.led_target, self.led_flash_ms,
            self.startup_preset, self.debug_log, self.midi_monitor,
        ]

    def _connect(self, widget) -> None:
        if isinstance(widget, QLineEdit):
            widget.textChanged.connect(lambda *_a: self.changed.emit())
        else:
            signal = getattr(widget, "toggled", None) or getattr(
                widget, "valueChanged", None) or getattr(widget, "currentIndexChanged", None)
            if signal is not None:
                signal.connect(lambda *_a: self.changed.emit())

    def load(self, cfg: AppConfig) -> None:
        self.din_enabled.setChecked(cfg.din_enabled)
        self.usb_midi_enabled.setChecked(cfg.usb_midi_enabled)
        self.clock_enabled.setChecked(cfg.midi_clock_enabled)
        self.base_root_midi.setValue(cfg.base_root_midi)
        self.note_range_low.setValue(cfg.note_range_low)
        self.note_range_high.setValue(cfg.note_range_high)
        self.display_revert_ms.setValue(cfg.display_revert_ms)
        self.display_prompt_ms.setValue(cfg.display_prompt_ms)
        self.display_cursor_ms.setValue(cfg.cursor_timeout_ms)
        self.display_menu_ms.setValue(cfg.menu_timeout_ms)
        self.bpm_indicator.setChecked(cfg.bpm_indicator)
        idx = self.led_target.findData(int(cfg.led_indicator))
        if idx >= 0:
            self.led_target.setCurrentIndex(idx)
        self.led_flash_ms.setValue(cfg.led_flash_ms)
        self.startup_preset.setText(cfg.startup_preset)
        self.debug_log.setChecked(cfg.debug_log_enabled)
        self.midi_monitor.setChecked(cfg.midi_monitor_enabled)

    def store(self, cfg: AppConfig) -> None:
        cfg.din_enabled = self.din_enabled.isChecked()
        cfg.usb_midi_enabled = self.usb_midi_enabled.isChecked()
        cfg.midi_clock_enabled = self.clock_enabled.isChecked()
        cfg.base_root_midi = self.base_root_midi.value()
        cfg.note_range_low = self.note_range_low.value()
        cfg.note_range_high = self.note_range_high.value()
        cfg.display_revert_ms = self.display_revert_ms.value()
        cfg.display_prompt_ms = self.display_prompt_ms.value()
        cfg.cursor_timeout_ms = self.display_cursor_ms.value()
        cfg.menu_timeout_ms = self.display_menu_ms.value()
        cfg.bpm_indicator = self.bpm_indicator.isChecked()
        cfg.led_indicator = LedTarget(self.led_target.currentData())
        cfg.led_flash_ms = self.led_flash_ms.value()
        cfg.startup_preset = self.startup_preset.text().strip() or "B1:P1"
        cfg.debug_log_enabled = self.debug_log.isChecked()
        cfg.midi_monitor_enabled = self.midi_monitor.isChecked()
