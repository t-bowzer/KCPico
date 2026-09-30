"""Rhythm pattern editor with a step grid."""

from __future__ import annotations

from PySide6.QtCore import Qt, QTimer, Signal
from PySide6.QtGui import QColor
from PySide6.QtWidgets import (
    QComboBox,
    QDialog,
    QDialogButtonBox,
    QFormLayout,
    QGroupBox,
    QHBoxLayout,
    QHeaderView,
    QLabel,
    QLineEdit,
    QListWidget,
    QMessageBox,
    QPushButton,
    QSpinBox,
    QTableWidget,
    QTableWidgetItem,
    QVBoxLayout,
    QWidget,
)

from ..io.storage import ConfigTree
from ..models.bounds import SWING_MAX, SWING_MIN
from ..models.defaults import default_rhythm_template
from ..models.rhythm import RHYTHM_FILES, RhythmPattern, RhythmTrack
from .grid import StepGrid

VELOCITY_PALETTE_BASE = [0, 1, 2, 3, 4, 8, 16]


class RhythmEditor(QWidget):
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
        self._index = 0 if tree.rhythms else -1
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
        self.short_name_edit = QLineEdit()
        self.steps_spin = QSpinBox()
        self.steps_spin.setRange(4, 64)
        self.steps_spin.setSingleStep(4)
        self.swing_spin = QSpinBox()
        self.swing_spin.setRange(SWING_MIN, SWING_MAX)
        self.swing_spin.setSingleStep(5)
        meta_form.addRow("Name", self.name_edit)
        meta_form.addRow("Short name", self.short_name_edit)
        meta_form.addRow("Steps per bar", self.steps_spin)
        meta_form.addRow("Swing", self.swing_spin)
        right.addWidget(meta)

        tracks_group = QGroupBox("Tracks")
        tracks_layout = QVBoxLayout(tracks_group)
        self.track_table = QTableWidget(0, 2)
        self.track_table.setHorizontalHeaderLabels(["Voice", "GM note"])
        self.track_table.horizontalHeader().setSectionResizeMode(0, QHeaderView.Stretch)
        self.track_table.itemChanged.connect(self._on_track_item_changed)
        tracks_layout.addWidget(self.track_table)

        track_btn_row = QHBoxLayout()
        self.add_track_btn = QPushButton("Add track")
        self.add_track_btn.clicked.connect(self._add_track)
        self.del_track_btn = QPushButton("Remove track")
        self.del_track_btn.clicked.connect(self._remove_track)
        track_btn_row.addWidget(self.add_track_btn)
        track_btn_row.addWidget(self.del_track_btn)
        track_btn_row.addStretch(1)
        tracks_layout.addLayout(track_btn_row)
        right.addWidget(tracks_group)

        grid_group = QGroupBox("Grid (left-click to place/clear a note, right-click to edit velocity)")
        grid_layout = QVBoxLayout(grid_group)
        grid_row = QHBoxLayout()
        grid_row.addWidget(QLabel("Place velocity"))
        self.draw_vel_spin = QSpinBox()
        self.draw_vel_spin.setRange(1, 127)
        self.draw_vel_spin.setValue(1)
        grid_row.addWidget(self.draw_vel_spin)
        grid_row.addWidget(QLabel("1 = default velocity (100)"))
        grid_row.addStretch(1)
        grid_layout.addLayout(grid_row)

        self.grid = StepGrid()
        self.grid.set_click_handler(self._on_grid_click)
        grid_layout.addWidget(self.grid)
        right.addWidget(grid_group, 1)

        preview_row = QHBoxLayout()
        preview_row.addWidget(QLabel("Tempo"))
        self.tempo_spin = QSpinBox()
        self.tempo_spin.setRange(40, 260)
        self.tempo_spin.setValue(120)
        preview_row.addWidget(self.tempo_spin)
        self.preview_btn = QPushButton("Preview")
        self.preview_btn.clicked.connect(self._toggle_preview)
        preview_row.addWidget(self.preview_btn)
        preview_row.addStretch(1)
        right.addLayout(preview_row)

        layout.addLayout(right, 1)

        self._wire_meta()
        self._apply_grid_style()

    def _wire_meta(self) -> None:
        self.name_edit.textChanged.connect(self._on_meta_changed)
        self.short_name_edit.textChanged.connect(self._on_meta_changed)
        self.steps_spin.valueChanged.connect(self._on_steps_changed)
        self.swing_spin.valueChanged.connect(self._on_meta_changed)

    # -- helpers ------------------------------------------------------------
    def _pattern(self) -> RhythmPattern | None:
        if self._tree is None or not (0 <= self._index < len(self._tree.rhythms)):
            return None
        return self._tree.rhythms[self._index][1]

    def _filename(self) -> str:
        if self._tree is None or not (0 <= self._index < len(self._tree.rhythms)):
            return ""
        return self._tree.rhythms[self._index][0]

    def refresh(self) -> None:
        self._loading = True
        self.pattern_list.blockSignals(True)
        self.pattern_list.clear()
        if self._tree is not None:
            for filename, pattern in self._tree.rhythms:
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
            self.short_name_edit.setText(pattern.short_name)
            self.steps_spin.setValue(pattern.steps_per_bar)
            self.swing_spin.setValue(pattern.swing)

            self.track_table.blockSignals(True)
            self.track_table.setRowCount(len(pattern.tracks))
            for i, track in enumerate(pattern.tracks):
                name_item = QTableWidgetItem(track.name)
                note_item = QTableWidgetItem()
                note_item.setData(0x0100, track.note)  # Qt.EditRole
                note_item.setText(str(track.note))
                self.track_table.setItem(i, 0, name_item)
                self.track_table.setItem(i, 1, note_item)
            self.track_table.blockSignals(False)

            self._render_grid()
        finally:
            self._loading = False

    def _set_enabled(self, enabled: bool) -> None:
        for widget in (self.name_edit, self.short_name_edit, self.steps_spin,
                       self.swing_spin, self.track_table, self.add_track_btn,
                       self.del_track_btn, self.grid, self.draw_vel_spin,
                       self.preview_btn, self.dup_btn, self.del_btn):
            widget.setEnabled(enabled)

    def _render_grid(self) -> None:
        pattern = self._pattern()
        if pattern is None:
            return
        rows = [list(t.pattern) for t in pattern.tracks]
        self.grid.set_row_labels([t.name or str(i) for i, t in enumerate(pattern.tracks)])
        self.grid.set_data(rows)

    def _apply_grid_style(self) -> None:
        self.grid.set_value_text(
            lambda val: "" if val == 0 else str(val))
        self.grid.set_value_color(
            lambda val: QColor("#2d7dd2" if val == 1 else
                               ("#7a4fbf" if val > 1 else "#2a2a2a")))

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
        pattern.short_name = self.short_name_edit.text()
        pattern.swing = self.swing_spin.value()
        self._rename_list_item()
        self.changed.emit()

    def _on_steps_changed(self, value: int) -> None:
        pattern = self._pattern()
        if pattern is None or self._loading:
            return
        pattern.steps_per_bar = value
        for track in pattern.tracks:
            while len(track.pattern) < value:
                track.pattern.append(0)
            if len(track.pattern) > value:
                del track.pattern[value:]
        self._render_grid()
        self.changed.emit()

    def _on_grid_click(self, row: int, col: int, button) -> None:
        if button == Qt.LeftButton:
            current = self.grid.value(row, col)
            self._set_note_velocity(
                row, col, 0 if current != 0 else self.draw_vel_spin.value())
        elif button == Qt.RightButton:
            self._edit_velocity(row, col)

    def _set_note_velocity(self, row: int, col: int, value: int) -> None:
        pattern = self._pattern()
        if pattern is None:
            return
        if 0 <= row < len(pattern.tracks) and 0 <= col < len(pattern.tracks[row].pattern):
            pattern.tracks[row].pattern[col] = value
        self.grid.set_value(row, col, value)
        self.changed.emit()

    def _edit_velocity(self, row: int, col: int) -> None:
        pattern = self._pattern()
        if pattern is None or not (0 <= row < len(pattern.tracks)):
            return
        current = self.grid.value(row, col)

        dialog = QDialog(self)
        dialog.setWindowTitle("Edit note velocity")
        form = QFormLayout(dialog)

        spin = QSpinBox()
        spin.setRange(0, 127)
        spin.setValue(self.draw_vel_spin.value() if current == 0 else current)
        form.addRow("Velocity", spin)

        hint = QLabel("0 = clear note, 1 = default velocity (100), 2\u2013127 = literal")
        hint.setWordWrap(True)
        form.addRow(hint)

        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.accepted.connect(dialog.accept)
        buttons.rejected.connect(dialog.reject)
        form.addRow(buttons)

        if dialog.exec() == QDialog.Accepted:
            self._set_note_velocity(row, col, spin.value())

    def _on_track_item_changed(self, item: QTableWidgetItem) -> None:
        pattern = self._pattern()
        if pattern is None or self._loading:
            return
        row = item.row()
        if not (0 <= row < len(pattern.tracks)):
            return
        if item.column() == 0:
            pattern.tracks[row].name = item.text()
            self.grid.set_row_labels(
                [t.name or str(i) for i, t in enumerate(pattern.tracks)])
        elif item.column() == 1:
            try:
                pattern.tracks[row].note = max(0, min(127, int(item.text())))
            except ValueError:
                pass
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
        pattern = RhythmPattern.from_dict(default_rhythm_template())
        pattern.name = self._unique_name("New Rhythm")
        self._tree.rhythms.append((filename, pattern))
        self._index = len(self._tree.rhythms) - 1
        self.refresh()
        self.changed.emit()

    def _duplicate_pattern(self) -> None:
        pattern = self._pattern()
        if pattern is None or self._tree is None:
            return
        filename = self._unique_user_filename("user")
        copy = RhythmPattern.from_dict(pattern.to_dict())
        copy.name = self._unique_name(pattern.name + " Copy")
        self._tree.rhythms.append((filename, copy))
        self._index = len(self._tree.rhythms) - 1
        self.refresh()
        self.changed.emit()

    def _delete_pattern(self) -> None:
        if self._tree is None or self._index < 0:
            return
        if self._filename() in RHYTHM_FILES:
            QMessageBox.information(self, "Built-in pattern",
                                    "Built-in patterns cannot be deleted.")
            return
        del self._tree.rhythms[self._index]
        self._index = min(self._index, len(self._tree.rhythms) - 1)
        self.refresh()
        self.changed.emit()

    def _add_track(self) -> None:
        pattern = self._pattern()
        if pattern is None:
            return
        pattern.tracks.append(RhythmTrack(note=42, name="track",
                                          pattern=[0] * pattern.steps_per_bar))
        self._load_pattern()
        self.changed.emit()

    def _remove_track(self) -> None:
        pattern = self._pattern()
        row = self.track_table.currentRow()
        if pattern is None or not (0 <= row < len(pattern.tracks)):
            return
        del pattern.tracks[row]
        self._load_pattern()
        self.changed.emit()

    def _unique_user_filename(self, base: str) -> str:
        used = {f for f, _p in self._tree.rhythms}
        i = 1
        while True:
            name = f"{base}{i}.json"
            if name not in used and name not in RHYTHM_FILES:
                return name
            i += 1

    def _unique_name(self, base: str) -> str:
        used = {p.name for _f, p in self._tree.rhythms}
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
        bar = synth.synthesize_rhythm_bar(pattern, tempo)
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
        self.grid.set_cursor(self._preview_step)
        self._preview_step += 1
        if self._preview_step >= max(pattern.steps_per_bar, 1):
            self._stop_preview()

    def _stop_preview(self) -> None:
        from ..preview import synth
        synth.stop()
        self._preview_timer.stop()
        self.grid.clear_cursor()
        self.preview_btn.setText("Preview")
