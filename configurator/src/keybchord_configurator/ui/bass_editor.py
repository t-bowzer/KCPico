"""Bass pattern editor with a degree grid + sustain grid."""

from __future__ import annotations

from PySide6.QtCore import QTimer, Signal
from PySide6.QtGui import QColor
from PySide6.QtWidgets import (
    QCheckBox,
    QFormLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QListWidget,
    QMessageBox,
    QPushButton,
    QSpinBox,
    QVBoxLayout,
    QWidget,
)

from ..io.storage import ConfigTree
from ..models.bass import (
    BASS_DEGREE_REST,
    BASS_FILES,
    BASS_DEGREE_LABELS,
    BassPattern,
)
from ..models.defaults import default_bass_template
from .grid import StepGrid

DEGREE_PALETTE = [BASS_DEGREE_REST, 0, 1, 2, 3]
SUSTAIN_PALETTE = [0, 2, 4, 8, 16]

_DEGREE_SHORT = {BASS_DEGREE_REST: "", 0: "R", 1: "3", 2: "5", 3: "7"}
_DEGREE_COLORS = {
    BASS_DEGREE_REST: QColor("#2a2a2a"),
    0: QColor("#2d7dd2"),
    1: QColor("#3fa34d"),
    2: QColor("#7a4fbf"),
    3: QColor("#b07a2d"),
}


class BassEditor(QWidget):
    changed = Signal()

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self._tree: ConfigTree | None = None
        self._index: int = -1
        self._loading = False
        self._preview_step = 0
        self._preview_timer = QTimer(self)
        self._preview_timer.timeout.connect(self._advance_preview)
        self._build()

    def set_tree(self, tree: ConfigTree) -> None:
        self._tree = tree
        self._index = 0 if tree.bass else -1
        self.refresh()

    # -- construction -------------------------------------------------------
    def _build(self) -> None:
        layout = QHBoxLayout(self)

        left = QVBoxLayout()
        left.addWidget(QLabel("Patterns"))
        self.pattern_list = QListWidget()
        self.pattern_list.currentRowChanged.connect(self._on_select)
        left.addWidget(self.pattern_list, 1)

        btn_row = QHBoxLayout()
        self.add_btn = QPushButton("New")
        self.add_btn.clicked.connect(self._add_pattern)
        self.dup_btn = QPushButton("Duplicate")
        self.dup_btn.clicked.connect(self._duplicate_pattern)
        self.del_btn = QPushButton("Delete")
        self.del_btn.clicked.connect(self._delete_pattern)
        btn_row.addWidget(self.add_btn)
        btn_row.addWidget(self.dup_btn)
        btn_row.addWidget(self.del_btn)
        left.addLayout(btn_row)
        layout.addLayout(left)

        right = QVBoxLayout()

        meta = QGroupBox("Pattern")
        meta_form = QFormLayout(meta)
        self.name_edit = QLineEdit()
        self.steps_spin = QSpinBox()
        self.steps_spin.setRange(4, 64)
        self.steps_spin.setSingleStep(4)
        self.hold_check = QCheckBox("Hold (not beat-driven)")
        meta_form.addRow("Name", self.name_edit)
        meta_form.addRow("Steps per bar", self.steps_spin)
        meta_form.addRow(self.hold_check)
        right.addWidget(meta)

        steps_group = QGroupBox("Steps (left-click to cycle: rest / root / 3rd / 5th / 7th)")
        steps_layout = QVBoxLayout(steps_group)
        self.steps_grid = StepGrid()
        self.steps_grid.set_palette(DEGREE_PALETTE)
        self.steps_grid.set_value_text(lambda v: _DEGREE_SHORT.get(v, ""))
        self.steps_grid.set_value_color(lambda v: _DEGREE_COLORS.get(v, QColor("#2a2a2a")))
        self.steps_grid.cellChanged.connect(self._on_steps_grid_changed)
        steps_layout.addWidget(self.steps_grid)
        right.addWidget(steps_group)

        sustain_group = QGroupBox("Sustain (steps held; left-click to cycle)")
        sustain_layout = QVBoxLayout(sustain_group)
        self.sustain_grid = StepGrid()
        self.sustain_grid.set_palette(SUSTAIN_PALETTE)
        self.sustain_grid.set_value_text(lambda v: str(v) if v else "")
        self.sustain_grid.set_value_color(
            lambda v: QColor("#3fa34d" if v else "#2a2a2a"))
        self.sustain_grid.cellChanged.connect(self._on_sustain_grid_changed)
        sustain_layout.addWidget(self.sustain_grid)
        right.addWidget(sustain_group)

        preview_row = QHBoxLayout()
        preview_row.addWidget(QLabel("Tempo"))
        self.tempo_spin = QSpinBox()
        self.tempo_spin.setRange(40, 260)
        self.tempo_spin.setValue(120)
        preview_row.addWidget(self.tempo_spin)
        preview_row.addWidget(QLabel("Root"))
        self.root_spin = QSpinBox()
        self.root_spin.setRange(24, 96)
        self.root_spin.setValue(48)
        preview_row.addWidget(self.root_spin)
        self.preview_btn = QPushButton("Preview")
        self.preview_btn.clicked.connect(self._toggle_preview)
        preview_row.addWidget(self.preview_btn)
        preview_row.addStretch(1)
        right.addLayout(preview_row)

        layout.addLayout(right, 1)

        self.name_edit.textChanged.connect(self._on_meta_changed)
        self.steps_spin.valueChanged.connect(self._on_steps_changed)
        self.hold_check.toggled.connect(self._on_meta_changed)

    # -- helpers ------------------------------------------------------------
    def _pattern(self) -> BassPattern | None:
        if self._tree is None or not (0 <= self._index < len(self._tree.bass)):
            return None
        return self._tree.bass[self._index][1]

    def _filename(self) -> str:
        if self._tree is None or not (0 <= self._index < len(self._tree.bass)):
            return ""
        return self._tree.bass[self._index][0]

    def refresh(self) -> None:
        self._loading = True
        self.pattern_list.blockSignals(True)
        self.pattern_list.clear()
        if self._tree is not None:
            for filename, pattern in self._tree.bass:
                self.pattern_list.addItem(pattern.name or filename)
        self.pattern_list.setCurrentRow(self._index)
        self.pattern_list.blockSignals(False)
        self._loading = False
        self._load_pattern()

    def _load_pattern(self) -> None:
        pattern = self._pattern()
        if pattern is None:
            self._set_enabled(False)
            return
        self._set_enabled(True)
        self._loading = True
        try:
            self.name_edit.setText(pattern.name)
            self.steps_spin.setValue(pattern.steps_per_bar)
            self.hold_check.setChecked(pattern.hold)

            steps = list(pattern.steps)
            if len(steps) < pattern.steps_per_bar:
                steps.extend([BASS_DEGREE_REST] * (pattern.steps_per_bar - len(steps)))
            sustain = list(pattern.sustain_steps)
            if len(sustain) < pattern.steps_per_bar:
                sustain.extend([0] * (pattern.steps_per_bar - len(sustain)))

            self.steps_grid.set_row_labels(["Degree"])
            self.steps_grid.set_data([steps])
            self.sustain_grid.set_row_labels(["Sustain"])
            self.sustain_grid.set_data([sustain])
        finally:
            self._loading = False

    def _set_enabled(self, enabled: bool) -> None:
        for widget in (self.name_edit, self.steps_spin, self.hold_check,
                       self.steps_grid, self.sustain_grid, self.preview_btn,
                       self.dup_btn, self.del_btn):
            widget.setEnabled(enabled)

    # -- signal handlers ----------------------------------------------------
    def _on_select(self, row: int) -> None:
        if self._loading or row < 0:
            return
        self._index = row
        self._load_pattern()

    def _on_meta_changed(self, *_args) -> None:
        pattern = self._pattern()
        if pattern is None or self._loading:
            return
        pattern.name = self.name_edit.text()
        pattern.hold = self.hold_check.isChecked()
        self._rename_list_item()
        self.changed.emit()

    def _on_steps_changed(self, value: int) -> None:
        pattern = self._pattern()
        if pattern is None or self._loading:
            return
        pattern.steps_per_bar = value
        while len(pattern.steps) < value:
            pattern.steps.append(BASS_DEGREE_REST)
        if len(pattern.steps) > value:
            del pattern.steps[value:]
        while len(pattern.sustain_steps) < value:
            pattern.sustain_steps.append(0)
        if len(pattern.sustain_steps) > value:
            del pattern.sustain_steps[value:]
        self._load_pattern()
        self.changed.emit()

    def _on_steps_grid_changed(self, _row: int, col: int, value: int) -> None:
        pattern = self._pattern()
        if pattern is None:
            return
        if 0 <= col < len(pattern.steps):
            pattern.steps[col] = value
        self.changed.emit()

    def _on_sustain_grid_changed(self, _row: int, col: int, value: int) -> None:
        pattern = self._pattern()
        if pattern is None:
            return
        while len(pattern.sustain_steps) <= col:
            pattern.sustain_steps.append(0)
        pattern.sustain_steps[col] = value
        self.changed.emit()

    def _rename_list_item(self) -> None:
        pattern = self._pattern()
        if pattern is not None and 0 <= self._index < self.pattern_list.count():
            item = self.pattern_list.item(self._index)
            if item is not None:
                item.setText(pattern.name or self._filename())

    # -- list operations ----------------------------------------------------
    def _add_pattern(self) -> None:
        if self._tree is None:
            return
        filename = self._unique_user_filename("user")
        pattern = BassPattern.from_dict(default_bass_template())
        pattern.name = self._unique_name("New Bass")
        self._tree.bass.append((filename, pattern))
        self._index = len(self._tree.bass) - 1
        self.refresh()
        self.changed.emit()

    def _duplicate_pattern(self) -> None:
        pattern = self._pattern()
        if pattern is None or self._tree is None:
            return
        filename = self._unique_user_filename("user")
        copy = BassPattern.from_dict(pattern.to_dict())
        copy.name = self._unique_name(pattern.name + " Copy")
        self._tree.bass.append((filename, copy))
        self._index = len(self._tree.bass) - 1
        self.refresh()
        self.changed.emit()

    def _delete_pattern(self) -> None:
        if self._tree is None or self._index < 0:
            return
        if self._filename() in BASS_FILES:
            QMessageBox.information(self, "Built-in pattern",
                                    "Built-in patterns cannot be deleted.")
            return
        del self._tree.bass[self._index]
        self._index = min(self._index, len(self._tree.bass) - 1)
        self.refresh()
        self.changed.emit()

    def _unique_user_filename(self, base: str) -> str:
        used = {f for f, _p in self._tree.bass}
        i = 1
        while True:
            name = f"{base}{i}.json"
            if name not in used and name not in BASS_FILES:
                return name
            i += 1

    def _unique_name(self, base: str) -> str:
        used = {p.name for _f, p in self._tree.bass}
        if base not in used:
            return base
        i = 2
        while f"{base} {i}" in used:
            i += 1
        return f"{base} {i}"

    # -- preview (stretch) --------------------------------------------------
    def _toggle_preview(self) -> None:
        if self._preview_timer.isActive():
            self._stop_preview()
            return
        from ..preview import synth
        from ..preview.ensure import ensure_available
        pattern = self._pattern()
        if pattern is None:
            return
        if not ensure_available(self):
            return
        tempo = self.tempo_spin.value()
        bar = synth.synthesize_bass_bar(pattern, tempo, self.root_spin.value())
        if not synth.play(bar, block=False):
            return
        step_ms = int(60000.0 / (tempo * 4))
        self._preview_step = 0
        self._preview_timer.start(step_ms)
        self.preview_btn.setText("Stop")

    def _advance_preview(self) -> None:
        pattern = self._pattern()
        if pattern is None:
            self._stop_preview()
            return
        self.steps_grid.set_cursor(self._preview_step)
        self.sustain_grid.set_cursor(self._preview_step)
        self._preview_step += 1
        if self._preview_step >= max(pattern.steps_per_bar, 1):
            self._stop_preview()

    def _stop_preview(self) -> None:
        from ..preview import synth
        synth.stop()
        self._preview_timer.stop()
        self.steps_grid.clear_cursor()
        self.sustain_grid.clear_cursor()
        self.preview_btn.setText("Preview")
