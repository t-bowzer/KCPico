"""Full per-preset chord sequencer editor (M14).

One row per bar on a sixteenth-note grid. Left-click a step to pick a chord
(root / type / add9 / add11 / add13); right-click clears it. The lower panel
edits the recorded chord-settings snapshot and the per-setting Live checkboxes.
"""

from __future__ import annotations

from typing import Optional

from PySide6.QtCore import Qt, Signal
from PySide6.QtGui import QColor
from PySide6.QtWidgets import (
    QCheckBox,
    QComboBox,
    QDialog,
    QDialogButtonBox,
    QFormLayout,
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QListWidget,
    QMessageBox,
    QPushButton,
    QScrollArea,
    QSpinBox,
    QVBoxLayout,
    QWidget,
)

from ..io.storage import ConfigTree
from ..models.bounds import (
    CHORD_MIN_INTERVAL_MAX,
    CHORD_MIN_INTERVAL_MIN,
    CHORD_MIN_NOTES_MAX,
    CHORD_MIN_NOTES_MIN,
    CHORD_ROLL_MAX,
    CHORD_ROLL_MIN,
    MAX_SEQUENCE_EVENTS,
    NOTE_DURATION_MAX,
    NOTE_DURATION_MIN,
    OCTAVE_MAX,
    OCTAVE_MIN,
    PAN_MAX,
    PAN_MIN,
    SEQUENCE_BARS_CHOICES,
    SEQUENCE_MAX_STEPS_PER_BAR,
    SEQUENCE_MIN_STEPS_PER_BAR,
    SEQUENCE_SLOTS,
    SEQUENCE_SETTING_COUNT,
    VELOCITY_MAX,
    VELOCITY_MIN,
)
from ..models.enums import ArpMode, ChordType, InversionMode, VoicingMode
from ..models.sequence import (
    LIVE_SETTING_KEYS,
    LIVE_SETTING_LABELS,
    ROOT_NAMES,
    RecordedSettings,
    Sequence,
    SequenceEvent,
    SequencerLibrary,
    chord_label,
)
from .grid import StepGrid
from .widgets import enum_combo, int_spin

CHORD_TYPE_LABELS: tuple[tuple[str, ChordType], ...] = (
    ("Major", ChordType.MAJOR),
    ("Minor", ChordType.MINOR),
    ("Dominant 7", ChordType.DOM7),
    ("Major 7", ChordType.MAJ7),
    ("Minor 7", ChordType.MIN7),
    ("Diminished", ChordType.DIM),
    ("Diminished 7", ChordType.DIM7),
    ("Augmented", ChordType.AUG),
    ("Sus4", ChordType.SUS4),
    ("Sus2", ChordType.SUS2),
    ("Minor 7b5", ChordType.MIN7B5),
)

VOICING_LABELS = {
    "Root position": int(VoicingMode.ROOT_POSITION),
    "Smart": int(VoicingMode.SMART),
    "Down": int(VoicingMode.DOWN),
    "Up": int(VoicingMode.UP),
}
INVERSION_LABELS = {
    "Root": int(InversionMode.ROOT),
    "First": int(InversionMode.FIRST),
    "Second": int(InversionMode.SECOND),
    "Third": int(InversionMode.THIRD),
}
ARP_LABELS = {
    "Up": int(ArpMode.UP),
    "Down": int(ArpMode.DOWN),
    "Up-Down": int(ArpMode.UP_DOWN),
    "Alternating": int(ArpMode.ALTERNATING),
    "Random": int(ArpMode.RANDOM),
}


class ChordPickerDialog(QDialog):
    def __init__(self, event: Optional[SequenceEvent] = None, parent=None) -> None:
        super().__init__(parent)
        self.setWindowTitle("Sequence chord")
        event = event or SequenceEvent()

        self.root_combo = QComboBox()
        for i, name in enumerate(ROOT_NAMES):
            self.root_combo.addItem(name, i)
        self.root_combo.setCurrentIndex(event.root_pc)

        self.type_combo = QComboBox()
        for label, typ in CHORD_TYPE_LABELS:
            self.type_combo.addItem(label, int(typ))
        idx = self.type_combo.findData(int(event.chord_type))
        if idx >= 0:
            self.type_combo.setCurrentIndex(idx)

        self.add9 = QCheckBox("add9")
        self.add11 = QCheckBox("add11")
        self.add13 = QCheckBox("add13")
        self.add9.setChecked(event.add9)
        self.add11.setChecked(event.add11)
        self.add13.setChecked(event.add13)

        self.preview = QLabel()
        self.preview.setStyleSheet("font-weight: bold;")

        form = QFormLayout()
        form.addRow("Root", self.root_combo)
        form.addRow("Type", self.type_combo)
        form.addRow("", self.add9)
        form.addRow("", self.add11)
        form.addRow("", self.add13)
        form.addRow("Chord", self.preview)

        buttons = QDialogButtonBox(
            QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)

        layout = QVBoxLayout(self)
        layout.addLayout(form)
        layout.addWidget(buttons)

        for widget in (self.root_combo, self.type_combo):
            widget.currentIndexChanged.connect(self._update_preview)
        for box in (self.add9, self.add11, self.add13):
            box.toggled.connect(self._update_preview)
        self._update_preview()

    def selected_event(self) -> SequenceEvent:
        root = int(self.root_combo.currentData())
        typ = ChordType(int(self.type_combo.currentData()))
        return SequenceEvent(
            root_pc=root,
            chord_type=typ,
            add9=self.add9.isChecked(),
            add11=self.add11.isChecked(),
            add13=self.add13.isChecked(),
        )

    def _update_preview(self, *_args) -> None:
        self.preview.setText(self.selected_event().label)


class SequenceEditor(QWidget):
    changed = Signal()

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self._tree: Optional[ConfigTree] = None
        self._bank = 0
        self._slot = 0
        self._seq_index = 0
        self._loading = False
        self._setting_widgets: dict[str, QWidget] = {}
        self._live_checks: list[QCheckBox] = []
        self._build()

    # -- construction -------------------------------------------------------
    def _build(self) -> None:
        layout = QHBoxLayout(self)

        left = QVBoxLayout()
        left.addWidget(QLabel("Sequence slots"))
        self.slot_list = QListWidget()
        self.slot_list.setFixedWidth(150)
        self.slot_list.currentRowChanged.connect(self._on_slot_changed)
        left.addWidget(self.slot_list, 1)
        self.clear_btn = QPushButton("Clear slot")
        self.clear_btn.clicked.connect(self._clear_slot)
        left.addWidget(self.clear_btn)
        layout.addLayout(left)

        right = QVBoxLayout()
        top = QHBoxLayout()
        self.location = QLabel("B1:P1")
        self.location.setStyleSheet("font-weight: bold;")
        top.addWidget(self.location)
        top.addSpacing(12)
        top.addWidget(QLabel("Bars"))
        self.bars_combo = QComboBox()
        for bars in SEQUENCE_BARS_CHOICES:
            self.bars_combo.addItem(str(bars), bars)
        self.bars_combo.currentIndexChanged.connect(self._on_bars_changed)
        top.addWidget(self.bars_combo)
        top.addWidget(QLabel("Steps/bar"))
        self.steps_spin = QSpinBox()
        self.steps_spin.setRange(SEQUENCE_MIN_STEPS_PER_BAR,
                                 SEQUENCE_MAX_STEPS_PER_BAR)
        self.steps_spin.setSingleStep(4)
        self.steps_spin.valueChanged.connect(self._on_steps_changed)
        top.addWidget(self.steps_spin)
        self.events_label = QLabel("0 events")
        top.addWidget(self.events_label)
        top.addStretch(1)
        self.capture_btn = QPushButton("Capture settings from preset")
        self.capture_btn.clicked.connect(self._capture_from_preset)
        top.addWidget(self.capture_btn)
        right.addLayout(top)

        hint = QLabel(
            "Left-click a step to pick a chord; right-click clears it. "
            "Playback uses held chords; Live = use the preset's current value.")
        hint.setWordWrap(True)
        right.addWidget(hint)

        self.grid = StepGrid()
        self.grid.set_click_handler(self._on_grid_click)
        grid_scroll = QScrollArea()
        grid_scroll.setWidgetResizable(True)
        grid_scroll.setWidget(self.grid)
        grid_scroll.setMinimumHeight(180)
        right.addWidget(grid_scroll, 2)

        settings_box = QGroupBox("Recorded chord settings")
        settings_layout = QGridLayout(settings_box)
        settings_layout.addWidget(QLabel("Setting"), 0, 0)
        settings_layout.addWidget(QLabel("Recorded value"), 0, 1)
        settings_layout.addWidget(QLabel("Live"), 0, 2)
        self._build_setting_rows(settings_layout)
        settings_scroll = QScrollArea()
        settings_scroll.setWidgetResizable(True)
        settings_scroll.setWidget(settings_box)
        right.addWidget(settings_scroll, 2)

        layout.addLayout(right, 1)

    def _build_setting_rows(self, layout: QGridLayout) -> None:
        widgets: dict[str, QWidget] = {}
        widgets["octave"] = int_spin(OCTAVE_MIN, OCTAVE_MAX, 1, 0)
        widgets["note_duration_ms"] = int_spin(
            NOTE_DURATION_MIN, NOTE_DURATION_MAX, 50, 500)
        widgets["velocity"] = int_spin(VELOCITY_MIN, VELOCITY_MAX, 1, 100)
        widgets["pan"] = int_spin(PAN_MIN, PAN_MAX, 1, 64)
        widgets["voicing_mode"] = enum_combo(
            VOICING_LABELS, int(VoicingMode.ROOT_POSITION))
        widgets["chord_roll_ms"] = int_spin(
            CHORD_ROLL_MIN, CHORD_ROLL_MAX, 10, 0)
        widgets["min_notes"] = int_spin(
            CHORD_MIN_NOTES_MIN, CHORD_MIN_NOTES_MAX, 1, 3)
        widgets["min_interval"] = int_spin(
            CHORD_MIN_INTERVAL_MIN, CHORD_MIN_INTERVAL_MAX, 1, 0)
        widgets["inversion"] = enum_combo(
            INVERSION_LABELS, int(InversionMode.ROOT))
        widgets["arp_mode"] = enum_combo(ARP_LABELS, int(ArpMode.UP))
        widgets["pattern"] = QComboBox()

        for row, key in enumerate(LIVE_SETTING_KEYS, start=1):
            label = LIVE_SETTING_LABELS[row - 1]
            layout.addWidget(QLabel(label), row, 0)
            widget = widgets[key]
            layout.addWidget(widget, row, 1)
            live = QCheckBox()
            live.toggled.connect(self._on_settings_changed)
            self._live_checks.append(live)
            layout.addWidget(live, row, 2)
            self._setting_widgets[key] = widget
            self._wire_setting(widget)

    def _wire_setting(self, widget: QWidget) -> None:
        if isinstance(widget, QComboBox):
            widget.currentIndexChanged.connect(self._on_settings_changed)
        elif isinstance(widget, QSpinBox):
            widget.valueChanged.connect(self._on_settings_changed)

    # -- context ------------------------------------------------------------
    def set_tree(self, tree: ConfigTree) -> None:
        self._tree = tree
        self.set_pattern_names(tree.chord_pattern_names())
        self.refresh()

    def set_preset(self, bank: int, slot: int) -> None:
        if self._tree is not None:
            bank = max(0, min(len(self._tree.banks) - 1, bank))
        self._bank = bank
        self._slot = max(0, min(7, slot))
        self.refresh()

    def set_pattern_names(self, names: list[str]) -> None:
        combo = self._setting_widgets.get("pattern")
        if not isinstance(combo, QComboBox):
            return
        current = combo.currentIndex()
        combo.blockSignals(True)
        combo.clear()
        combo.addItems(names)
        if 0 <= current < combo.count():
            combo.setCurrentIndex(current)
        combo.blockSignals(False)

    # -- library/sequence ---------------------------------------------------
    def _library(self) -> SequencerLibrary:
        if self._tree is None:
            return SequencerLibrary()
        key = (self._bank, self._slot)
        lib = self._tree.sequences.get(key)
        if lib is None:
            lib = SequencerLibrary()
            self._tree.sequences[key] = lib
        return lib

    def _sequence(self) -> Optional[Sequence]:
        if self._tree is None:
            return None
        return self._library().slot(self._seq_index)

    # -- refresh ------------------------------------------------------------
    def refresh(self) -> None:
        if self._tree is None:
            return
        self.location.setText(f"B{self._bank + 1}:P{self._slot + 1}")
        lib = self._library()
        self.slot_list.blockSignals(True)
        self.slot_list.clear()
        for i, seq in enumerate(lib.slots):
            self.slot_list.addItem(f"{i + 1:>2}   {len(seq.events)} events")
        self.slot_list.setCurrentRow(self._seq_index)
        self.slot_list.blockSignals(False)
        self._load_sequence()

    def _load_sequence(self) -> None:
        seq = self._sequence()
        if seq is None:
            return
        self._loading = True
        try:
            idx = self.bars_combo.findData(seq.bars)
            self.bars_combo.setCurrentIndex(idx if idx >= 0 else 0)
            self.steps_spin.setValue(seq.steps_per_bar)
            self._refresh_grid(seq)
            self._load_settings(seq)
            self.events_label.setText(f"{len(seq.events)} events")
            self._update_slot_item(seq)
        finally:
            self._loading = False

    def _refresh_grid(self, seq: Sequence) -> None:
        cols = seq.steps_per_bar
        data = [[-1] * cols for _ in range(seq.bars)]
        for event in seq.events:
            row, col = divmod(event.step, cols)
            if 0 <= row < seq.bars and 0 <= col < cols:
                data[row][col] = event.chord_id
        self.grid.set_row_labels([f"Bar {i + 1}" for i in range(seq.bars)])
        self.grid.set_data(data)
        self.grid.set_value_text(lambda v: chord_label(v) if v >= 0 else "")
        self.grid.set_value_color(self._color_for)
        self.grid.update()

    @staticmethod
    def _color_for(value: int) -> QColor:
        if value < 0:
            return QColor("#2a2a2a")
        root = (value // 128) % 12
        return QColor.fromHsv((root * 30) % 360, 150, 185)

    def _load_settings(self, seq: Sequence) -> None:
        rec = seq.recorded
        self._set_spin("octave", rec.octave)
        self._set_spin("note_duration_ms", rec.note_duration_ms)
        self._set_spin("velocity", rec.velocity)
        self._set_spin("pan", rec.pan)
        self._set_combo("voicing_mode", int(rec.voicing_mode))
        self._set_spin("chord_roll_ms", rec.chord_roll_ms)
        self._set_spin("min_notes", rec.min_notes)
        self._set_spin("min_interval", rec.min_interval)
        self._set_combo("inversion", int(rec.inversion))
        self._set_combo("arp_mode", int(rec.arp_mode))
        self._set_combo("pattern", rec.pattern)
        for i, check in enumerate(self._live_checks):
            check.blockSignals(True)
            check.setChecked(seq.is_live(i))
            check.blockSignals(False)

    def _set_spin(self, key: str, value: int) -> None:
        widget = self._setting_widgets.get(key)
        if isinstance(widget, QSpinBox):
            widget.blockSignals(True)
            widget.setValue(value)
            widget.blockSignals(False)

    def _set_combo(self, key: str, value: int) -> None:
        widget = self._setting_widgets.get(key)
        if isinstance(widget, QComboBox):
            idx = widget.findData(value)
            if idx < 0 and key == "pattern":
                idx = value if value < widget.count() else 0
            widget.blockSignals(True)
            if idx >= 0:
                widget.setCurrentIndex(idx)
            widget.blockSignals(False)

    def _update_slot_item(self, seq: Sequence) -> None:
        item = self.slot_list.item(self._seq_index)
        if item is not None:
            item.setText(f"{self._seq_index + 1:>2}   {len(seq.events)} events")

    # -- edits --------------------------------------------------------------
    def _on_slot_changed(self, row: int) -> None:
        if self._loading or row < 0:
            return
        self._seq_index = row
        self._load_sequence()

    def _on_bars_changed(self, *_args) -> None:
        if self._loading:
            return
        seq = self._sequence()
        if seq is None:
            return
        seq.bars = int(self.bars_combo.currentData())
        loop = seq.loop_steps
        seq.events = [e for e in seq.events if e.step < loop]
        self._after_change(seq)

    def _on_steps_changed(self, value: int) -> None:
        if self._loading:
            return
        seq = self._sequence()
        if seq is None:
            return
        seq.steps_per_bar = value
        loop = seq.loop_steps
        seq.events = [e for e in seq.events if e.step < loop]
        self._after_change(seq)

    def _on_settings_changed(self, *_args) -> None:
        if self._loading:
            return
        seq = self._sequence()
        if seq is None:
            return
        seq.recorded = self._read_settings()
        for i, check in enumerate(self._live_checks):
            seq.set_live(i, check.isChecked())
        self.changed.emit()

    def _read_settings(self) -> RecordedSettings:
        rec = RecordedSettings()
        for key, widget in self._setting_widgets.items():
            if isinstance(widget, QSpinBox):
                setattr(rec, key, widget.value())
            elif isinstance(widget, QComboBox):
                if key == "pattern":
                    setattr(rec, key, max(0, widget.currentIndex()))
                else:
                    setattr(rec, key, widget.currentData())
        # Enum values come back as ints; normalize to the enum types.
        rec.voicing_mode = VoicingMode(int(rec.voicing_mode))
        rec.inversion = InversionMode(int(rec.inversion))
        rec.arp_mode = ArpMode(int(rec.arp_mode))
        return rec

    def _after_change(self, seq: Sequence) -> None:
        self._refresh_grid(seq)
        self.events_label.setText(f"{len(seq.events)} events")
        self._update_slot_item(seq)
        self.changed.emit()

    def _on_grid_click(self, row: int, col: int, button) -> None:
        seq = self._sequence()
        if seq is None:
            return
        step = row * seq.steps_per_bar + col
        if button == Qt.RightButton:
            seq.remove_event(step)
            self._after_change(seq)
            return
        if button != Qt.LeftButton:
            return

        dialog = ChordPickerDialog(seq.event_at(step), self)
        if dialog.exec() != QDialog.Accepted:
            return
        event = dialog.selected_event()
        event.step = step
        if not seq.set_event(step, event):
            QMessageBox.warning(
                self, "Sequence full",
                f"This sequence already has {MAX_SEQUENCE_EVENTS} events. "
                "Remove a step before adding another.")
            return
        self._after_change(seq)

    def _capture_from_preset(self) -> None:
        if self._tree is None:
            return
        seq = self._sequence()
        if seq is None:
            return
        if not (0 <= self._bank < len(self._tree.banks)):
            return
        slot = max(0, min(7, self._slot))
        chord = self._tree.banks[self._bank][slot].chord
        names = self._tree.chord_pattern_names()
        try:
            pattern = names.index(chord.pattern)
        except ValueError:
            pattern = 0
        seq.recorded = RecordedSettings(
            octave=chord.octave,
            note_duration_ms=chord.note_duration_ms,
            velocity=chord.velocity,
            pan=chord.pan,
            voicing_mode=chord.voicing_mode,
            chord_roll_ms=chord.chord_roll_ms,
            min_notes=chord.min_notes,
            min_interval=chord.min_interval,
            inversion=chord.inversion,
            arp_mode=chord.arp_mode,
            pattern=pattern,
        )
        self._load_settings(seq)
        self.changed.emit()

    def _clear_slot(self) -> None:
        seq = self._sequence()
        if seq is None:
            return
        if QMessageBox.question(
                self, "Clear sequence",
                f"Clear sequence slot {self._seq_index + 1}?",
                QMessageBox.Yes | QMessageBox.No,
                QMessageBox.No) != QMessageBox.Yes:
            return
        self._library().slots[self._seq_index] = Sequence.defaults()
        self.refresh()
        self.changed.emit()

    def store_current(self) -> None:
        if self._loading:
            return
        seq = self._sequence()
        if seq is None:
            return
        seq.bars = int(self.bars_combo.currentData())
        seq.steps_per_bar = self.steps_spin.value()
        seq.recorded = self._read_settings()
        for i, check in enumerate(self._live_checks):
            seq.set_live(i, check.isChecked())
        loop = seq.loop_steps
        seq.events = [e for e in seq.events if e.step < loop]
