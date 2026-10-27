"""Editor for per-preset top-row macro keys."""

from __future__ import annotations

from typing import Any, Optional

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (
    QComboBox,
    QDialog,
    QDialogButtonBox,
    QFormLayout,
    QHBoxLayout,
    QHeaderView,
    QLabel,
    QLineEdit,
    QListWidget,
    QPushButton,
    QTableWidget,
    QTableWidgetItem,
    QVBoxLayout,
    QWidget,
)

from ..models.bounds import PARAM_SPECS
from ..models.enums import NumberKeyMode
from ..models.keymap import BOOL_PARAMS, KEYMAP_PARAM_NAMES
from ..models.macro import (
    MACRO_COMMAND_TYPES,
    MAX_MACRO_COMMANDS_PER_KEY,
    MAX_MACRO_COMMANDS_PER_PRESET,
    MAX_MACRO_NAME_LEN,
    NUMBER_KEY_LABELS,
    MacroCommand,
    MacroConfig,
)
from ..models.preset import DRUM_PIECES


def macro_param_names() -> list[str]:
    names = list(KEYMAP_PARAM_NAMES)
    names.extend(key for key in PARAM_SPECS if key.startswith("drum_"))
    return names


def _parse_value(text: str) -> Any:
    text = text.strip()
    if text == "":
        return None
    if text.lstrip("+-").isdigit():
        return int(text)
    return text


class MacroCommandDialog(QDialog):
    def __init__(
        self,
        parent: Optional[QWidget] = None,
        *,
        command: Optional[MacroCommand] = None,
        rhythm_names: Optional[list[str]] = None,
        bass_names: Optional[list[str]] = None,
        chord_pattern_names: Optional[list[str]] = None,
    ) -> None:
        super().__init__(parent)
        self.setWindowTitle("Add macro command" if command is None else "Edit macro command")
        self.rhythm_names = rhythm_names or []
        self.bass_names = bass_names or []
        self.chord_pattern_names = chord_pattern_names or []
        self._build()
        if command is not None:
            self._populate(command)

    def _build(self) -> None:
        layout = QVBoxLayout(self)
        form = QFormLayout()

        self.type_combo = QComboBox()
        self.type_combo.addItems(list(MACRO_COMMAND_TYPES))
        self.type_combo.currentTextChanged.connect(self._sync_fields)

        self.param_combo = QComboBox()
        self.param_combo.addItems(macro_param_names())
        self.param_combo.currentTextChanged.connect(self._sync_value_options)

        self.value_combo = QComboBox()
        self.value_combo.setEditable(True)
        self.a_combo = QComboBox()
        self.a_combo.setEditable(True)
        self.b_combo = QComboBox()
        self.b_combo.setEditable(True)

        self.piece_combo = QComboBox()
        for key, label, _default in DRUM_PIECES:
            self.piece_combo.addItem(label, key)
        self.mute_combo = QComboBox()
        self.mute_combo.addItem("Mute on", "on")
        self.mute_combo.addItem("Mute off", "off")

        form.addRow("Command", self.type_combo)
        form.addRow("Parameter", self.param_combo)
        form.addRow("Value", self.value_combo)
        form.addRow("A", self.a_combo)
        form.addRow("B", self.b_combo)
        form.addRow("Drum piece", self.piece_combo)
        form.addRow("Mute", self.mute_combo)
        layout.addLayout(form)

        self._param_label = form.labelForField(self.param_combo)
        self._value_label = form.labelForField(self.value_combo)
        self._a_label = form.labelForField(self.a_combo)
        self._b_label = form.labelForField(self.b_combo)
        self._piece_label = form.labelForField(self.piece_combo)
        self._mute_label = form.labelForField(self.mute_combo)

        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

        self._sync_fields()

    def _set_visible(self, widget: QWidget, label: Optional[QLabel], visible: bool) -> None:
        widget.setVisible(visible)
        if label is not None:
            label.setVisible(visible)

    def _sync_fields(self) -> None:
        typ = self.type_combo.currentText()
        is_param = typ != "drum_mute"
        is_value = typ in ("set", "toggle")
        self._set_visible(self.param_combo, self._param_label, is_param)
        self._set_visible(self.value_combo, self._value_label, is_value)
        self._set_visible(self.a_combo, self._a_label, typ == "toggle")
        self._set_visible(self.b_combo, self._b_label, typ == "toggle")
        self._set_visible(self.piece_combo, self._piece_label, typ == "drum_mute")
        self._set_visible(self.mute_combo, self._mute_label, typ == "drum_mute")
        if is_param:
            self._sync_value_options()

    def _value_options(self, param: str) -> list[str]:
        if param in BOOL_PARAMS:
            return ["on", "off"]
        spec = PARAM_SPECS.get(param)
        if spec is not None:
            if spec.kind in ("bool",):
                return ["on", "off"]
            if spec.kind == "enum" and spec.values:
                return list(spec.values.keys())
            if spec.kind == "pattern":
                if param == "rhythm_pattern":
                    return list(self.rhythm_names)
                if param == "bass_pattern":
                    return list(self.bass_names)
                if param == "chord_pattern":
                    return list(self.chord_pattern_names)
        return []

    def _sync_value_options(self) -> None:
        param = self.param_combo.currentText()
        for combo in (self.value_combo, self.a_combo, self.b_combo):
            combo.blockSignals(True)
            current = combo.currentText()
            combo.clear()
            combo.addItems(self._value_options(param))
            if current:
                combo.setCurrentText(current)
            combo.blockSignals(False)

    def _populate(self, command: MacroCommand) -> None:
        idx = self.type_combo.findText(command.type)
        if idx >= 0:
            self.type_combo.setCurrentIndex(idx)
        if command.param:
            idx = self.param_combo.findText(command.param)
            if idx >= 0:
                self.param_combo.setCurrentIndex(idx)
        self._sync_value_options()
        if command.value is not None:
            self.value_combo.setCurrentText(str(command.value))
        if command.a is not None:
            self.a_combo.setCurrentText(str(command.a))
        if command.b is not None:
            self.b_combo.setCurrentText(str(command.b))
        if command.piece:
            idx = self.piece_combo.findData(command.piece)
            if idx >= 0:
                self.piece_combo.setCurrentIndex(idx)
        if command.value in (True, "on", 1):
            self.mute_combo.setCurrentIndex(0)
        elif command.value in (False, "off", 0):
            self.mute_combo.setCurrentIndex(1)
        self._sync_fields()

    def result_command(self) -> MacroCommand:
        typ = self.type_combo.currentText()
        cmd = MacroCommand(type=typ)
        if typ == "drum_mute":
            cmd.piece = self.piece_combo.currentData()
            cmd.value = self.mute_combo.currentData()
        else:
            cmd.param = self.param_combo.currentText()
            if typ == "set":
                cmd.value = _parse_value(self.value_combo.currentText())
            elif typ == "toggle":
                cmd.a = _parse_value(self.a_combo.currentText())
                cmd.b = _parse_value(self.b_combo.currentText())
        return cmd


class MacroEditor(QWidget):
    changed = Signal()
    keySelected = Signal(str)

    def __init__(self, parent: Optional[QWidget] = None) -> None:
        super().__init__(parent)
        self._config: Optional[MacroConfig] = None
        self._loading = False
        self._rhythm_names: list[str] = []
        self._bass_names: list[str] = []
        self._chord_pattern_names: list[str] = []
        self._build()

    def _build(self) -> None:
        root = QVBoxLayout(self)

        mode_row = QHBoxLayout()
        mode_row.addWidget(QLabel("Top-row number keys:"))
        self.mode_combo = QComboBox()
        self.mode_combo.addItem("Strum", int(NumberKeyMode.STRUM))
        self.mode_combo.addItem("Macro keys", int(NumberKeyMode.MACRO))
        self.mode_combo.currentIndexChanged.connect(self._on_mode_changed)
        mode_row.addWidget(self.mode_combo)
        mode_row.addStretch(1)
        self.count_label = QLabel("")
        mode_row.addWidget(self.count_label)
        root.addLayout(mode_row)

        body = QHBoxLayout()
        self.key_list = QListWidget()
        self.key_list.setFixedWidth(170)
        self.key_list.currentRowChanged.connect(self._on_key_changed)
        body.addWidget(self.key_list)

        right = QVBoxLayout()
        name_row = QHBoxLayout()
        name_row.addWidget(QLabel("Macro name:"))
        self.name_edit = QLineEdit()
        self.name_edit.setMaxLength(MAX_MACRO_NAME_LEN)
        self.name_edit.textEdited.connect(self._on_name_changed)
        self.name_edit.editingFinished.connect(self._normalize_name)
        name_row.addWidget(self.name_edit, 1)
        right.addLayout(name_row)

        self.table = QTableWidget(0, 4)
        self.table.setHorizontalHeaderLabels(["Command", "Parameter", "Value", "Drum piece"])
        self.table.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeToContents)
        self.table.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeToContents)
        self.table.horizontalHeader().setSectionResizeMode(2, QHeaderView.Stretch)
        self.table.horizontalHeader().setSectionResizeMode(3, QHeaderView.ResizeToContents)
        self.table.setEditTriggers(QTableWidget.NoEditTriggers)
        self.table.setSelectionBehavior(QTableWidget.SelectRows)
        self.table.setSelectionMode(QTableWidget.SingleSelection)
        self.table.doubleClicked.connect(lambda _index: self._edit_command())
        right.addWidget(self.table, 1)

        buttons = QHBoxLayout()
        self.add_btn = QPushButton("Add")
        self.add_btn.clicked.connect(self._add_command)
        self.edit_btn = QPushButton("Edit")
        self.edit_btn.clicked.connect(self._edit_command)
        self.remove_btn = QPushButton("Remove")
        self.remove_btn.clicked.connect(self._remove_command)
        self.up_btn = QPushButton("Move Up")
        self.up_btn.clicked.connect(lambda: self._move_command(-1))
        self.down_btn = QPushButton("Move Down")
        self.down_btn.clicked.connect(lambda: self._move_command(+1))
        for btn in (self.add_btn, self.edit_btn, self.remove_btn,
                    self.up_btn, self.down_btn):
            buttons.addWidget(btn)
        buttons.addStretch(1)
        right.addLayout(buttons)

        self.status_label = QLabel("")
        self.status_label.setWordWrap(True)
        right.addWidget(self.status_label)
        body.addLayout(right, 1)
        root.addLayout(body, 1)

        self._refresh_key_list()

    def set_names(self, rhythm_names: list[str], bass_names: list[str],
                  chord_pattern_names: list[str]) -> None:
        self._rhythm_names = list(rhythm_names)
        self._bass_names = list(bass_names)
        self._chord_pattern_names = list(chord_pattern_names)

    def set_config(self, config: MacroConfig,
                   rhythm_names: list[str] | None = None,
                   bass_names: list[str] | None = None,
                   chord_pattern_names: list[str] | None = None) -> None:
        if rhythm_names is not None:
            self._rhythm_names = list(rhythm_names)
        if bass_names is not None:
            self._bass_names = list(bass_names)
        if chord_pattern_names is not None:
            self._chord_pattern_names = list(chord_pattern_names)

        self._loading = True
        try:
            self._config = config
            idx = self.mode_combo.findData(int(config.number_keys))
            self.mode_combo.setCurrentIndex(idx if idx >= 0 else 0)
            self._refresh_key_list()
            if self.key_list.currentRow() < 0:
                self.key_list.setCurrentRow(0)
            self._refresh_table()
        finally:
            self._loading = False

    def select_key(self, label: str) -> None:
        if label not in NUMBER_KEY_LABELS:
            return
        self.key_list.setCurrentRow(NUMBER_KEY_LABELS.index(label))
        self._refresh_table()
        self.table.setFocus()

    def edit_selected_command(self) -> None:
        if self._selected_row() < 0:
            commands = self._current_commands()
            if commands:
                self.table.selectRow(0)
            else:
                self._add_command()
                return
        self._edit_command()

    def _on_name_changed(self, text: str) -> None:
        if self._loading or self._config is None:
            return
        label = self._current_label()
        if label is None:
            return
        self._config.names[label] = text.strip()[:MAX_MACRO_NAME_LEN]
        row = self.key_list.currentRow()
        if row >= 0:
            count = len(self._config.keys.get(label, []))
            name = self._config.name_for(label)
            self.key_list.item(row).setText(f"{label}  {name} ({count})")
        self.changed.emit()

    def _normalize_name(self) -> None:
        if self._loading or self._config is None:
            return
        label = self._current_label()
        if label is None:
            return
        if not self._config.names.get(label):
            self.name_edit.setText(self._config.name_for(label))

    def _current_label(self) -> Optional[str]:
        row = self.key_list.currentRow()
        if row < 0 or row >= len(NUMBER_KEY_LABELS):
            return None
        return NUMBER_KEY_LABELS[row]

    def _current_commands(self) -> list[MacroCommand]:
        label = self._current_label()
        if self._config is None or label is None:
            return []
        return self._config.keys.setdefault(label, [])

    def _on_mode_changed(self, _index: int) -> None:
        if self._loading or self._config is None:
            return
        self._config.number_keys = NumberKeyMode(self.mode_combo.currentData())
        self.changed.emit()

    def _on_key_changed(self, _row: int) -> None:
        if not self._loading:
            self._refresh_table()
            label = self._current_label()
            if label is not None:
                self.keySelected.emit(label)

    def _refresh_key_list(self) -> None:
        current = self.key_list.currentRow()
        self.key_list.blockSignals(True)
        self.key_list.clear()
        for label in NUMBER_KEY_LABELS:
            count = len(self._config.keys.get(label, [])) if self._config else 0
            name = self._config.name_for(label) if self._config else f"M{label}"
            self.key_list.addItem(f"{label}  {name} ({count})")
        self.key_list.blockSignals(False)
        if 0 <= current < len(NUMBER_KEY_LABELS):
            self.key_list.setCurrentRow(current)

    def _refresh_table(self) -> None:
        label = self._current_label()
        self.name_edit.blockSignals(True)
        self.name_edit.setText(
            self._config.name_for(label)
            if self._config is not None and label is not None else "")
        self.name_edit.setEnabled(
            self._config is not None and label is not None)
        self.name_edit.blockSignals(False)

        commands = self._current_commands()
        self.table.setRowCount(len(commands))
        for row, cmd in enumerate(commands):
            values = [
                cmd.type,
                cmd.param or "",
                self._command_value_text(cmd),
                cmd.piece or "",
            ]
            for col, text in enumerate(values):
                item = QTableWidgetItem(text)
                item.setFlags(item.flags() & ~Qt.ItemIsEditable)
                self.table.setItem(row, col, item)
        self._update_status()

    @staticmethod
    def _command_value_text(cmd: MacroCommand) -> str:
        if cmd.type == "set":
            return str(cmd.value)
        if cmd.type == "toggle":
            return f"{cmd.a} <-> {cmd.b}"
        if cmd.type == "drum_mute":
            return "on" if cmd.value in (True, "on", 1) else "off"
        return ""

    def _update_status(self) -> None:
        total = self._config.total_commands() if self._config else 0
        label = self._current_label() or "?"
        per_key = len(self._config.keys.get(label, [])) if self._config else 0
        text = (f"{total}/{MAX_MACRO_COMMANDS_PER_PRESET} commands used; "
                f"key {label}: {per_key}/{MAX_MACRO_COMMANDS_PER_KEY}")
        if total > MAX_MACRO_COMMANDS_PER_PRESET or \
                per_key > MAX_MACRO_COMMANDS_PER_KEY:
            self.status_label.setStyleSheet("color: #e05353;")
            text += "  (over limit)"
        else:
            self.status_label.setStyleSheet("")
        self.status_label.setText(text)
        self.add_btn.setEnabled(
            self._config is not None and self._config.can_add(label))

    def _selected_row(self) -> int:
        rows = self.table.selectionModel().selectedRows()
        return rows[0].row() if rows else -1

    def _add_command(self) -> None:
        label = self._current_label()
        if self._config is None or label is None or not self._config.can_add(label):
            return
        dialog = MacroCommandDialog(
            self,
            rhythm_names=self._rhythm_names,
            bass_names=self._bass_names,
            chord_pattern_names=self._chord_pattern_names,
        )
        if dialog.exec() == QDialog.Accepted:
            self._config.keys.setdefault(label, []).append(dialog.result_command())
            self._refresh_key_list()
            self._refresh_table()
            self.changed.emit()

    def _edit_command(self) -> None:
        label = self._current_label()
        row = self._selected_row()
        if self._config is None or label is None or row < 0:
            return
        commands = self._config.keys.get(label, [])
        if row >= len(commands):
            return
        dialog = MacroCommandDialog(
            self,
            command=commands[row],
            rhythm_names=self._rhythm_names,
            bass_names=self._bass_names,
            chord_pattern_names=self._chord_pattern_names,
        )
        if dialog.exec() == QDialog.Accepted:
            commands[row] = dialog.result_command()
            self._refresh_table()
            self.changed.emit()

    def _remove_command(self) -> None:
        label = self._current_label()
        row = self._selected_row()
        if self._config is None or label is None or row < 0:
            return
        commands = self._config.keys.get(label, [])
        if row >= len(commands):
            return
        del commands[row]
        self._refresh_key_list()
        self._refresh_table()
        self.changed.emit()

    def _move_command(self, delta: int) -> None:
        label = self._current_label()
        row = self._selected_row()
        if self._config is None or label is None or row < 0:
            return
        commands = self._config.keys.get(label, [])
        target = row + delta
        if target < 0 or target >= len(commands):
            return
        commands[row], commands[target] = commands[target], commands[row]
        self._refresh_table()
        self.table.selectRow(target)
        self.changed.emit()
