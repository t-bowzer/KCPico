"""Preset browser and editor (10 banks x 8 slots)."""

from __future__ import annotations

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (
    QComboBox,
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QListWidget,
    QListWidgetItem,
    QPushButton,
    QScrollArea,
    QSpinBox,
    QTabWidget,
    QVBoxLayout,
    QWidget,
)

from ..io.storage import ConfigTree
from ..models.enums import (
    ArpMode,
    InversionMode,
    PlayMode,
    ScaleType,
    StrumMode,
    VoicingMode,
)
from ..models.bounds import PARAM_SPECS
from ..models.preset import DRUM_PIECES, PresetSlot
from .widgets import bool_check, enum_combo, int_spin


class PresetsEditor(QWidget):
    changed = Signal()

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self._tree: ConfigTree | None = None
        self._bank = 0
        self._slot = 0
        self._loading = False
        self._build()

    def set_tree(self, tree: ConfigTree) -> None:
        self._tree = tree
        self._refresh_bank_list()
        self._refresh_slot_buttons()
        self._load()

    # -- construction -------------------------------------------------------
    def _build(self) -> None:
        layout = QHBoxLayout(self)

        left = QVBoxLayout()
        left.addWidget(QLabel("Banks"))
        self.bank_list = QListWidget()
        self.bank_list.setFixedWidth(90)
        self.bank_list.currentRowChanged.connect(self._on_bank_changed)
        left.addWidget(self.bank_list)
        layout.addLayout(left)

        right = QVBoxLayout()
        self.name_edit = QLineEdit()
        self.name_edit.setPlaceholderText("Preset name (empty = Default)")
        self.name_edit.textChanged.connect(self._on_name_changed)
        right.addWidget(self.name_edit)

        self.slot_buttons: list[QPushButton] = []
        slot_row = QHBoxLayout()
        for i in range(8):
            btn = QPushButton(f"P{i + 1}")
            btn.setCheckable(True)
            btn.clicked.connect(lambda _checked, s=i: self._on_slot_changed(s))
            self.slot_buttons.append(btn)
            slot_row.addWidget(btn)
        right.addLayout(slot_row)

        self.tabs = QTabWidget()
        self._build_chord_tab()
        self._build_strum_tab()
        self._build_bass_tab()
        self._build_rhythm_tab()
        self._build_drums_tab()
        right.addWidget(self.tabs)
        layout.addLayout(right, 1)

    def _build_chord_tab(self) -> None:
        box, form = self._form_widget()
        self.chord_mode = enum_combo(PARAM_SPECS["chord_mode"].values, PlayMode.HELD)
        self.chord_octave = int_spin(-3, 3, 1, 0)
        self.chord_duration = int_spin(50, 4000, 50, 500)
        self.chord_velocity = int_spin(1, 127, 1, 100)
        self.chord_pan = int_spin(0, 127, 1, 64)
        self.chord_voicing = enum_combo(PARAM_SPECS["chord_voicing"].values,
                                        VoicingMode.ROOT_POSITION)
        self.chord_roll = int_spin(-2000, 2000, 10, 0)
        self.chord_min_notes = int_spin(2, 6, 1, 3)
        self.chord_min_interval = int_spin(0, 12, 1, 0)
        self.chord_inversion = enum_combo(PARAM_SPECS["chord_inversion"].values,
                                          InversionMode.ROOT)
        self.chord_arp = enum_combo(PARAM_SPECS["arp_mode"].values, ArpMode.UP)
        self.chord_channel = int_spin(1, 16, 1, 1)
        rows = [
            ("Play mode", self.chord_mode), ("Octave", self.chord_octave),
            ("Note duration (ms)", self.chord_duration),
            ("Velocity", self.chord_velocity), ("Pan", self.chord_pan),
            ("Voicing", self.chord_voicing), ("Chord roll (ms)", self.chord_roll),
            ("Min notes", self.chord_min_notes),
            ("Min interval", self.chord_min_interval),
            ("Inversion", self.chord_inversion), ("Arp pattern", self.chord_arp),
            ("Channel", self.chord_channel),
        ]
        for label, widget in rows:
            form.addRow(label, widget)
        self.tabs.addTab(box, "Chord")
        self._chord_widgets = [w for _l, w in rows]

    def _build_strum_tab(self) -> None:
        box, form = self._form_widget()
        self.strum_octave = int_spin(-3, 3, 1, 1)
        self.strum_duration = int_spin(50, 4000, 50, 300)
        self.strum_velocity = int_spin(1, 127, 1, 90)
        self.strum_limited = bool_check(False, "Limited keys")
        self.strum_mode = enum_combo(PARAM_SPECS["strum_mode"].values,
                                     StrumMode.FOLLOW_CHORD)
        self.strum_root = int_spin(0, 11, 1, 0)
        self.strum_scale = enum_combo(PARAM_SPECS["strum_scale"].values,
                                      ScaleType.IONIAN)
        self.strum_channel = int_spin(1, 16, 1, 2)
        rows = [
            ("Octave", self.strum_octave), ("Note duration (ms)", self.strum_duration),
            ("Velocity", self.strum_velocity), ("", self.strum_limited),
            ("Mode", self.strum_mode), ("Root pitch class", self.strum_root),
            ("Scale / mode", self.strum_scale), ("Channel", self.strum_channel),
        ]
        for label, widget in rows:
            form.addRow(label, widget)
        self.tabs.addTab(box, "Strum")
        self._strum_widgets = [w for _l, w in rows]

    def _build_bass_tab(self) -> None:
        box, form = self._form_widget()
        self.bass_enabled = bool_check(False, "Enabled")
        self.bass_octave = int_spin(-3, 3, 1, -1)
        self.bass_duration = int_spin(50, 4000, 50, 150)
        self.bass_velocity = int_spin(1, 127, 1, 90)
        self.bass_channel = int_spin(1, 16, 1, 3)
        self.bass_pattern = QComboBox()
        rows = [
            ("", self.bass_enabled), ("Octave", self.bass_octave),
            ("Note duration (ms)", self.bass_duration),
            ("Velocity", self.bass_velocity), ("Channel", self.bass_channel),
            ("Pattern", self.bass_pattern),
        ]
        for label, widget in rows:
            form.addRow(label, widget)
        self.tabs.addTab(box, "Bass")
        self._bass_widgets = [w for _l, w in rows]

    def _build_rhythm_tab(self) -> None:
        box, form = self._form_widget()
        self.rhythm_enabled = bool_check(False, "Enabled")
        self.rhythm_pattern = QComboBox()
        self.rhythm_tempo = int_spin(40, 260, 1, 120)
        self.rhythm_swing = int_spin(-75, 75, 5, 0)
        self.rhythm_muted = bool_check(False, "Muted")
        self.rhythm_channel = int_spin(1, 16, 1, 10)
        rows = [
            ("", self.rhythm_enabled), ("Pattern", self.rhythm_pattern),
            ("Tempo (BPM)", self.rhythm_tempo), ("Swing", self.rhythm_swing),
            ("", self.rhythm_muted), ("Channel", self.rhythm_channel),
        ]
        for label, widget in rows:
            form.addRow(label, widget)
        self.tabs.addTab(box, "Rhythm")
        self._rhythm_widgets = [w for _l, w in rows]

    def _build_drums_tab(self) -> None:
        container = QWidget()
        grid = QGridLayout(container)
        grid.addWidget(QLabel("Piece"), 0, 0)
        grid.addWidget(QLabel("Note"), 0, 1)
        grid.addWidget(QLabel("Velocity"), 0, 2)
        self._drum_note_spins: dict[str, QSpinBox] = {}
        self._drum_vel_spins: dict[str, QSpinBox] = {}
        for row, (key, label, default_note) in enumerate(DRUM_PIECES, start=1):
            note_spin = int_spin(0, 127, 1, default_note)
            vel_spin = int_spin(0, 128, 1, 0)
            grid.addWidget(QLabel(label), row, 0)
            grid.addWidget(note_spin, row, 1)
            grid.addWidget(vel_spin, row, 2)
            self._drum_note_spins[key] = note_spin
            self._drum_vel_spins[key] = vel_spin
        scroll = QScrollArea()
        scroll.setWidget(container)
        scroll.setWidgetResizable(True)
        self.tabs.addTab(scroll, "Drums")

    def _form_widget(self):
        from PySide6.QtWidgets import QFormLayout
        box = QGroupBox()
        form = QFormLayout(box)
        return box, form

    # -- signals ------------------------------------------------------------
    def _wire(self, widget, slot) -> None:
        signal = getattr(widget, "toggled", None) or getattr(
            widget, "valueChanged", None) or getattr(
            widget, "currentIndexChanged", None) or getattr(
            widget, "textChanged", None)
        if signal is not None:
            signal.connect(slot)

    # -- state --------------------------------------------------------------
    def _refresh_bank_list(self) -> None:
        self.bank_list.blockSignals(True)
        self.bank_list.clear()
        for i in range(10):
            self.bank_list.addItem(f"Bank {i + 1}")
        self.bank_list.setCurrentRow(self._bank)
        self.bank_list.blockSignals(False)

    def _refresh_slot_buttons(self) -> None:
        for i, btn in enumerate(self.slot_buttons):
            if self._tree is not None:
                slot = self._tree.banks[self._bank][i]
                btn.setText(f"P{i + 1}\n{slot.name if slot.name != 'Default' else ''}")
            btn.setChecked(i == self._slot)

    def _current_slot(self) -> PresetSlot | None:
        if self._tree is None:
            return None
        return self._tree.banks[self._bank][self._slot]

    def _on_bank_changed(self, row: int) -> None:
        if row < 0:
            return
        self._bank = row
        self._slot = 0
        self._refresh_slot_buttons()
        self._load()

    def _on_slot_changed(self, slot: int) -> None:
        self._slot = slot
        self._refresh_slot_buttons()
        self._load()

    def _on_name_changed(self, text: str) -> None:
        slot = self._current_slot()
        if slot is not None and not self._loading:
            slot.name = text
            self._refresh_slot_buttons()
            self.changed.emit()

    def refresh_pattern_lists(self) -> None:
        if self._tree is None:
            return
        self._set_combo_names(self.rhythm_pattern, self._tree.rhythm_names())
        self._set_combo_names(self.bass_pattern, self._tree.bass_names())

    def _set_combo_names(self, combo: QComboBox, names: list[str]) -> None:
        current = combo.currentText()
        combo.blockSignals(True)
        combo.clear()
        combo.addItems(names)
        idx = combo.findText(current)
        combo.setCurrentIndex(idx if idx >= 0 else 0)
        combo.blockSignals(False)

    # -- load / store -------------------------------------------------------
    def _load(self) -> None:
        slot = self._current_slot()
        if slot is None:
            return
        self._loading = True
        try:
            self.name_edit.setText(slot.name)

            self._set_combo_data(self.chord_mode, int(slot.chord.play_mode))
            self.chord_octave.setValue(slot.chord.octave)
            self.chord_duration.setValue(slot.chord.note_duration_ms)
            self.chord_velocity.setValue(slot.chord.velocity)
            self.chord_pan.setValue(slot.chord.pan)
            self._set_combo_data(self.chord_voicing, int(slot.chord.voicing_mode))
            self.chord_roll.setValue(slot.chord.chord_roll_ms)
            self.chord_min_notes.setValue(slot.chord.min_notes)
            self.chord_min_interval.setValue(slot.chord.min_interval)
            self._set_combo_data(self.chord_inversion, int(slot.chord.inversion))
            self._set_combo_data(self.chord_arp, int(slot.chord.arp_mode))
            self.chord_channel.setValue(slot.chord.channel)

            self.strum_octave.setValue(slot.strum.octave)
            self.strum_duration.setValue(slot.strum.note_duration_ms)
            self.strum_velocity.setValue(slot.strum.velocity)
            self.strum_limited.setChecked(slot.strum.limited_keys)
            self._set_combo_data(self.strum_mode, int(slot.strum.mode))
            self.strum_root.setValue(slot.strum.root_pc)
            self._set_combo_data(self.strum_scale, int(slot.strum.scale_type))
            self.strum_channel.setValue(slot.strum.channel)

            self.bass_enabled.setChecked(slot.bass.enabled)
            self.bass_octave.setValue(slot.bass.octave)
            self.bass_duration.setValue(slot.bass.note_duration_ms)
            self.bass_velocity.setValue(slot.bass.velocity)
            self.bass_channel.setValue(slot.bass.channel)
            self._set_combo_text(self.bass_pattern, slot.bass.pattern)

            self.rhythm_enabled.setChecked(slot.rhythm.enabled)
            self._set_combo_text(self.rhythm_pattern, slot.rhythm.pattern)
            self.rhythm_tempo.setValue(slot.rhythm.tempo)
            self.rhythm_swing.setValue(slot.rhythm.swing)
            self.rhythm_muted.setChecked(slot.rhythm.muted)
            self.rhythm_channel.setValue(slot.rhythm.channel)

            for key in self._drum_note_spins:
                self._drum_note_spins[key].setValue(slot.rhythm.drums.notes[key])
                self._drum_vel_spins[key].setValue(slot.rhythm.drums.velocities[key])
        finally:
            self._loading = False

    def _set_combo_data(self, combo: QComboBox, value: int) -> None:
        idx = combo.findData(value)
        if idx >= 0:
            combo.setCurrentIndex(idx)

    def _set_combo_text(self, combo: QComboBox, text: str) -> None:
        idx = combo.findText(text)
        if idx >= 0:
            combo.setCurrentIndex(idx)
        elif combo.count() > 0:
            combo.setCurrentIndex(0)

    def store_current(self) -> None:
        slot = self._current_slot()
        if slot is None or self._loading:
            return
        slot.name = self.name_edit.text()

        slot.chord.play_mode = PlayMode(self.chord_mode.currentData())
        slot.chord.octave = self.chord_octave.value()
        slot.chord.note_duration_ms = self.chord_duration.value()
        slot.chord.velocity = self.chord_velocity.value()
        slot.chord.pan = self.chord_pan.value()
        slot.chord.voicing_mode = VoicingMode(self.chord_voicing.currentData())
        slot.chord.chord_roll_ms = self.chord_roll.value()
        slot.chord.min_notes = self.chord_min_notes.value()
        slot.chord.min_interval = self.chord_min_interval.value()
        slot.chord.inversion = InversionMode(self.chord_inversion.currentData())
        slot.chord.arp_mode = ArpMode(self.chord_arp.currentData())
        slot.chord.channel = self.chord_channel.value()

        slot.strum.octave = self.strum_octave.value()
        slot.strum.note_duration_ms = self.strum_duration.value()
        slot.strum.velocity = self.strum_velocity.value()
        slot.strum.limited_keys = self.strum_limited.isChecked()
        slot.strum.mode = StrumMode(self.strum_mode.currentData())
        slot.strum.root_pc = self.strum_root.value()
        slot.strum.scale_type = ScaleType(self.strum_scale.currentData())
        slot.strum.channel = self.strum_channel.value()

        slot.bass.enabled = self.bass_enabled.isChecked()
        slot.bass.octave = self.bass_octave.value()
        slot.bass.note_duration_ms = self.bass_duration.value()
        slot.bass.velocity = self.bass_velocity.value()
        slot.bass.channel = self.bass_channel.value()
        slot.bass.pattern = self.bass_pattern.currentText()

        slot.rhythm.enabled = self.rhythm_enabled.isChecked()
        slot.rhythm.pattern = self.rhythm_pattern.currentText()
        slot.rhythm.tempo = self.rhythm_tempo.value()
        slot.rhythm.swing = self.rhythm_swing.value()
        slot.rhythm.muted = self.rhythm_muted.isChecked()
        slot.rhythm.channel = self.rhythm_channel.value()

        for key in self._drum_note_spins:
            slot.rhythm.drums.notes[key] = self._drum_note_spins[key].value()
            slot.rhythm.drums.velocities[key] = self._drum_vel_spins[key].value()

    def connect_signals(self) -> None:
        for widget in self._chord_widgets + self._strum_widgets + \
                self._bass_widgets + self._rhythm_widgets:
            self._wire(widget, self._on_field_changed)
        for key in self._drum_note_spins:
            self._wire(self._drum_note_spins[key], self._on_field_changed)
            self._wire(self._drum_vel_spins[key], self._on_field_changed)

    def _on_field_changed(self, *_args) -> None:
        if self._loading:
            return
        self.store_current()
        self._refresh_slot_buttons()
        self.changed.emit()
