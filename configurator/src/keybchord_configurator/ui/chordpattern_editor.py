"""Chord pattern editor with a degree-bitmask grid + sustain grid."""

from __future__ import annotations

from PySide6.QtCore import Signal
from PySide6.QtGui import QColor
from PySide6.QtWidgets import (
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
from ..models.chordpattern import CHORD_PATTERN_FILES, ChordPattern
from ..models.defaults import default_chord_pattern_template
from .grid import StepGrid

BIT_PALETTE = list(range(16))
SUSTAIN_PALETTE = [0, 2, 4, 8, 16]

_BIT_SHORT = {
    0: "", 1: "R", 2: "3", 4: "5", 8: "7",
    3: "R3", 5: "R5", 6: "35", 7: "R35",
    9: "R7", 10: "37", 11: "R37", 12: "57",
    13: "R57", 14: "357", 15: "R357",
}
_BIT_COLORS = {
    0: QColor("#2a2a2a"),
    1: QColor("#2d7dd2"),
    2: QColor("#3fa34d"),
    4: QColor("#7a4fbf"),
    8: QColor("#b07a2d"),
}


def _color_for(mask: int) -> QColor:
    if mask == 0:
        return _BIT_COLORS[0]
    if mask & 0x08:
        return _BIT_COLORS[8]
    if mask & 0x04:
        return _BIT_COLORS[4]
    if mask & 0x02:
        return _BIT_COLORS[2]
    return _BIT_COLORS[1]


class ChordPatternEditor(QWidget):
    changed = Signal()

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self._tree: ConfigTree | None = None
        self._index: int = -1
        self._loading = False
        self._build()

    def set_tree(self, tree: ConfigTree) -> None:
        self._tree = tree
        self._index = 0 if tree.chordpatterns else -1
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
        meta_form.addRow("Name", self.name_edit)
        meta_form.addRow("Steps per bar", self.steps_spin)
        right.addWidget(meta)

        steps_group = QGroupBox(
            "Steps (click to cycle degree bits: root / 3rd / 5th / 7th, and combos)")
        steps_layout = QVBoxLayout(steps_group)
        self.steps_grid = StepGrid()
        self.steps_grid.set_palette(BIT_PALETTE)
        self.steps_grid.set_value_text(lambda v: _BIT_SHORT.get(v, ""))
        self.steps_grid.set_value_color(_color_for)
        self.steps_grid.cellChanged.connect(self._on_steps_grid_changed)
        steps_layout.addWidget(self.steps_grid)
        right.addWidget(steps_group)

        sustain_group = QGroupBox("Sustain (steps held; click to cycle)")
        sustain_layout = QVBoxLayout(sustain_group)
        self.sustain_grid = StepGrid()
        self.sustain_grid.set_palette(SUSTAIN_PALETTE)
        self.sustain_grid.set_value_text(lambda v: str(v) if v else "")
        self.sustain_grid.set_value_color(
            lambda v: QColor("#3fa34d" if v else "#2a2a2a"))
        self.sustain_grid.cellChanged.connect(self._on_sustain_grid_changed)
        sustain_layout.addWidget(self.sustain_grid)
        right.addWidget(sustain_group)

        layout.addLayout(right, 1)

        self.name_edit.textChanged.connect(self._on_meta_changed)
        self.steps_spin.valueChanged.connect(self._on_steps_changed)

    # -- helpers ------------------------------------------------------------
    def _pattern(self) -> ChordPattern | None:
        if self._tree is None or not (0 <= self._index < len(self._tree.chordpatterns)):
            return None
        return self._tree.chordpatterns[self._index][1]

    def _filename(self) -> str:
        if self._tree is None or not (0 <= self._index < len(self._tree.chordpatterns)):
            return ""
        return self._tree.chordpatterns[self._index][0]

    def refresh(self) -> None:
        self._loading = True
        self.pattern_list.blockSignals(True)
        self.pattern_list.clear()
        if self._tree is not None:
            for filename, pattern in self._tree.chordpatterns:
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

            steps = list(pattern.steps)
            if len(steps) < pattern.steps_per_bar:
                steps.extend([0] * (pattern.steps_per_bar - len(steps)))
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
        for widget in (self.name_edit, self.steps_spin, self.steps_grid,
                       self.sustain_grid, self.dup_btn, self.del_btn):
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
        self._rename_list_item()
        self.changed.emit()

    def _on_steps_changed(self, value: int) -> None:
        pattern = self._pattern()
        if pattern is None or self._loading:
            return
        pattern.steps_per_bar = value
        while len(pattern.steps) < value:
            pattern.steps.append(0)
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
        pattern = ChordPattern.from_dict(default_chord_pattern_template())
        pattern.name = self._unique_name("New Pattern")
        self._tree.chordpatterns.append((filename, pattern))
        self._index = len(self._tree.chordpatterns) - 1
        self.refresh()
        self.changed.emit()

    def _duplicate_pattern(self) -> None:
        pattern = self._pattern()
        if pattern is None or self._tree is None:
            return
        filename = self._unique_user_filename("user")
        copy = ChordPattern.from_dict(pattern.to_dict())
        copy.name = self._unique_name(pattern.name + " Copy")
        self._tree.chordpatterns.append((filename, copy))
        self._index = len(self._tree.chordpatterns) - 1
        self.refresh()
        self.changed.emit()

    def _delete_pattern(self) -> None:
        if self._tree is None or self._index < 0:
            return
        if self._filename() in CHORD_PATTERN_FILES:
            QMessageBox.information(self, "Built-in pattern",
                                    "Built-in patterns cannot be deleted.")
            return
        del self._tree.chordpatterns[self._index]
        self._index = min(self._index, len(self._tree.chordpatterns) - 1)
        self.refresh()
        self.changed.emit()

    def _unique_user_filename(self, base: str) -> str:
        used = {f for f, _p in self._tree.chordpatterns}
        i = 1
        while True:
            name = f"{base}{i}.json"
            if name not in used and name not in CHORD_PATTERN_FILES:
                return name
            i += 1

    def _unique_name(self, base: str) -> str:
        used = {p.name for _f, p in self._tree.chordpatterns}
        if base not in used:
            return base
        i = 2
        while f"{base} {i}" in used:
            i += 1
        return f"{base} {i}"
