"""Keymap editor (configurable key bindings)."""

from __future__ import annotations

from typing import Optional

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (
    QComboBox,
    QDialog,
    QDialogButtonBox,
    QFileDialog,
    QFormLayout,
    QHBoxLayout,
    QHeaderView,
    QLabel,
    QMessageBox,
    QPushButton,
    QSpinBox,
    QSplitter,
    QTableWidget,
    QTableWidgetItem,
    QVBoxLayout,
    QWidget,
)

from ..io.storage import ConfigTree
from ..models.enums import EDIT_MENU_BY_JSON
from ..models.keymap import (
    ACTION_TYPES,
    BINDABLE_KEYS,
    BOOL_PARAMS,
    KEYMAP_PARAM_NAMES,
    MODIFIERS,
    PARAM_VALUE_TABLE,
    KeyAction,
    KeyBinding,
    default_keymap,
)
from ..validation.validator import validate_keymap
from .keyboard_diagram import KeyboardDiagram
from .keymap_visual import LAYER_ORDER


class BindingDialog(QDialog):
    def __init__(self, parent=None, *, binding: Optional[KeyBinding] = None,
                 rhythm_names: Optional[list[str]] = None,
                 bass_names: Optional[list[str]] = None,
                 initial_key: Optional[str] = None,
                 initial_modifier: Optional[str] = None) -> None:
        super().__init__(parent)
        self.setWindowTitle("Add binding" if binding is None else "Edit binding")
        self.rhythm_names = rhythm_names or []
        self.bass_names = bass_names or []
        self._build()

        if binding is not None:
            self.modifier_combo.setCurrentIndex(
                MODIFIERS.index(binding.modifier) + 1 if binding.modifier else 0)
            idx = self.key_combo.findText(binding.key)
            if idx >= 0:
                self.key_combo.setCurrentIndex(idx)
            self.type_combo.setCurrentIndex(
                ACTION_TYPES.index(binding.action.type)
                if binding.action.type in ACTION_TYPES else 0)
            self._sync_action_fields()
            self._populate_from_action(binding.action)
        else:
            if initial_modifier:
                idx = self.modifier_combo.findData(initial_modifier)
                if idx >= 0:
                    self.modifier_combo.setCurrentIndex(idx)
            if initial_key:
                idx = self.key_combo.findText(initial_key)
                if idx >= 0:
                    self.key_combo.setCurrentIndex(idx)

    def _build(self) -> None:
        layout = QVBoxLayout(self)
        form = QFormLayout()

        self.modifier_combo = QComboBox()
        self.modifier_combo.addItem("(none)", None)
        for m in MODIFIERS:
            self.modifier_combo.addItem(m, m)
        self.key_combo = QComboBox()
        self.key_combo.addItems(list(BINDABLE_KEYS))
        self.type_combo = QComboBox()
        self.type_combo.addItems(list(ACTION_TYPES))
        self.type_combo.currentIndexChanged.connect(self._sync_action_fields)

        self.param_combo = QComboBox()
        self.param_combo.addItems(list(KEYMAP_PARAM_NAMES))
        self.param_combo.currentIndexChanged.connect(self._sync_value_options)

        self.menu_combo = QComboBox()
        self.menu_combo.addItems(list(EDIT_MENU_BY_JSON.keys()))

        self.value_edit = QComboBox()
        self.value_edit.setEditable(True)
        self.value_edit.addItems([])

        self.a_edit = QComboBox()
        self.a_edit.setEditable(True)
        self.b_edit = QComboBox()
        self.b_edit.setEditable(True)

        self.index_spin = QSpinBox()
        self.index_spin.setRange(0, 11)
        self.slot_spin = QSpinBox()
        self.slot_spin.setRange(0, 7)

        self.ext_combo = QComboBox()
        self.ext_combo.addItem("add9", 9)
        self.ext_combo.addItem("add11", 11)
        self.ext_combo.addItem("add13", 13)

        form.addRow("Modifier", self.modifier_combo)
        form.addRow("Key", self.key_combo)
        form.addRow("Action", self.type_combo)
        form.addRow("Parameter", self.param_combo)
        form.addRow("Menu", self.menu_combo)
        form.addRow("Value", self.value_edit)
        form.addRow("A", self.a_edit)
        form.addRow("B", self.b_edit)
        form.addRow("Extension", self.ext_combo)
        form.addRow("Drum index", self.index_spin)
        form.addRow("Slot", self.slot_spin)
        layout.addLayout(form)

        self._param_label = form.labelForField(self.param_combo)
        self._menu_label = form.labelForField(self.menu_combo)
        self._value_label = form.labelForField(self.value_edit)
        self._a_label = form.labelForField(self.a_edit)
        self._b_label = form.labelForField(self.b_edit)
        self._ext_label = form.labelForField(self.ext_combo)
        self._index_label = form.labelForField(self.index_spin)
        self._slot_label = form.labelForField(self.slot_spin)

        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

        self._sync_action_fields()

    def _set_visible(self, widget, label, visible: bool) -> None:
        widget.setVisible(visible)
        if label is not None:
            label.setVisible(visible)

    def _sync_action_fields(self) -> None:
        action_type = self.type_combo.currentText()
        is_open_menu = action_type == "open_menu"
        is_param = action_type in ("cycle", "set", "inc", "dec", "toggle")
        is_set = action_type == "set"
        is_toggle = action_type == "toggle"
        is_drum = action_type == "drum_mute"
        is_load = action_type == "preset_load"
        is_ext = action_type == "ext"

        self._set_visible(self.param_combo, self._param_label, is_param)
        self._set_visible(self.menu_combo, self._menu_label, is_open_menu)
        self._set_visible(self.value_edit, self._value_label, is_set)
        self._set_visible(self.a_edit, self._a_label, is_toggle)
        self._set_visible(self.b_edit, self._b_label, is_toggle)
        self._set_visible(self.ext_combo, self._ext_label, is_ext)
        self._set_visible(self.index_spin, self._index_label, is_drum)
        self._set_visible(self.slot_spin, self._slot_label, is_load)

        if is_param:
            self._sync_value_options()

    def _value_options(self, param: str) -> list[str]:
        if param in BOOL_PARAMS:
            return ["on", "off"]
        table = PARAM_VALUE_TABLE.get(param)
        if table:
            return list(table.keys())
        if param == "rhythm_pattern":
            return list(self.rhythm_names)
        if param == "bass_pattern":
            return list(self.bass_names)
        return []

    def _sync_value_options(self) -> None:
        param = self.param_combo.currentText()
        for combo in (self.value_edit, self.a_edit, self.b_edit):
            combo.blockSignals(True)
            combo.clear()
            combo.addItems(self._value_options(param))
            combo.blockSignals(False)

    def _populate_from_action(self, action: KeyAction) -> None:
        if action.type in ("cycle", "set", "inc", "dec", "toggle"):
            idx = self.param_combo.findText(action.param or "")
            if idx >= 0:
                self.param_combo.setCurrentIndex(idx)
            self._sync_value_options()
        if action.type == "open_menu" and action.menu:
            idx = self.menu_combo.findText(action.menu)
            if idx >= 0:
                self.menu_combo.setCurrentIndex(idx)
        if action.type == "set" and action.value is not None:
            self.value_edit.setCurrentText(str(action.value))
        if action.type == "toggle":
            if action.a is not None:
                self.a_edit.setCurrentText(str(action.a))
            if action.b is not None:
                self.b_edit.setCurrentText(str(action.b))
        if action.type == "drum_mute" and action.index is not None:
            self.index_spin.setValue(action.index)
        if action.type == "preset_load" and action.slot is not None:
            self.slot_spin.setValue(action.slot)
        if action.type == "ext":
            idx = self.ext_combo.findData(action.value)
            if idx >= 0:
                self.ext_combo.setCurrentIndex(idx)

    def result_binding(self) -> KeyBinding:
        modifier = self.modifier_combo.currentData()
        key = self.key_combo.currentText()
        action_type = self.type_combo.currentText()
        action = KeyAction(type=action_type)

        if action_type in ("cycle", "set", "inc", "dec", "toggle"):
            action.param = self.param_combo.currentText()
        if action_type == "open_menu":
            action.menu = self.menu_combo.currentText()
        if action_type == "set":
            action.value = self._parse_value(self.value_edit.currentText())
        if action_type == "toggle":
            action.a = self._parse_value(self.a_edit.currentText())
            action.b = self._parse_value(self.b_edit.currentText())
        if action_type == "ext":
            action.value = self.ext_combo.currentData()
        if action_type == "drum_mute":
            action.index = self.index_spin.value()
        if action_type == "preset_load":
            action.slot = self.slot_spin.value()

        return KeyBinding(modifier=modifier, key=key, action=action)

    @staticmethod
    def _parse_value(text: str):
        text = text.strip()
        if text == "":
            return None
        if text.lstrip("+-").isdigit():
            return int(text)
        return text


class KeymapEditor(QWidget):
    changed = Signal()

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self._tree: ConfigTree | None = None
        self._build()

    def set_tree(self, tree: ConfigTree) -> None:
        self._tree = tree
        self.refresh()

    def _build(self) -> None:
        layout = QVBoxLayout(self)

        top = QWidget()
        top_layout = QVBoxLayout(top)
        top_layout.setContentsMargins(0, 0, 0, 0)

        toolbar = QHBoxLayout()
        toolbar.addWidget(QLabel("Show:"))
        self.layer_combo = QComboBox()
        self.layer_combo.addItems(list(LAYER_ORDER))
        self.layer_combo.currentTextChanged.connect(self._set_layer)
        toolbar.addWidget(self.layer_combo)
        toolbar.addStretch(1)
        self.export_btn = QPushButton("Export diagram...")
        self.export_btn.clicked.connect(self._export_diagram)
        toolbar.addWidget(self.export_btn)
        top_layout.addLayout(toolbar)

        self.diagram = KeyboardDiagram()
        top_layout.addWidget(self.diagram, 1)

        bottom = QWidget()
        bottom_layout = QVBoxLayout(bottom)
        bottom_layout.setContentsMargins(0, 0, 0, 0)

        self.table = QTableWidget(0, 4)
        self.table.setHorizontalHeaderLabels(["Key", "Action", "Target", "Value"])
        self.table.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeToContents)
        self.table.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeToContents)
        self.table.horizontalHeader().setSectionResizeMode(2, QHeaderView.Stretch)
        self.table.horizontalHeader().setSectionResizeMode(3, QHeaderView.Stretch)
        self.table.setEditTriggers(QTableWidget.NoEditTriggers)
        self.table.setSelectionBehavior(QTableWidget.SelectRows)
        self.table.setSelectionMode(QTableWidget.SingleSelection)
        self.table.doubleClicked.connect(lambda _index: self._edit_binding())
        self.table.itemSelectionChanged.connect(self._sync_diagram_selection)
        bottom_layout.addWidget(self.table, 1)

        btn_row = QHBoxLayout()
        self.add_btn = QPushButton("Add")
        self.add_btn.clicked.connect(self._add_binding)
        self.edit_btn = QPushButton("Edit")
        self.edit_btn.clicked.connect(self._edit_binding)
        self.remove_btn = QPushButton("Remove")
        self.remove_btn.clicked.connect(self._remove_binding)
        self.reset_btn = QPushButton("Reset to default")
        self.reset_btn.clicked.connect(self._reset_default)
        for btn in (self.add_btn, self.edit_btn, self.remove_btn, self.reset_btn):
            btn_row.addWidget(btn)
        btn_row.addStretch(1)
        bottom_layout.addLayout(btn_row)

        self.status_label = QLabel("")
        self.status_label.setWordWrap(True)
        bottom_layout.addWidget(self.status_label)

        splitter = QSplitter(Qt.Vertical)
        splitter.addWidget(top)
        splitter.addWidget(bottom)
        splitter.setChildrenCollapsible(False)
        splitter.setStretchFactor(0, 1)
        splitter.setStretchFactor(1, 1)
        splitter.setSizes([420, 300])
        layout.addWidget(splitter, 1)

        self.diagram.bindingSelected.connect(self._select_binding_row)
        self.diagram.bindingEditRequested.connect(self._edit_binding_index)
        self.diagram.unboundKeyActivated.connect(self._add_binding_for_key)

    def _set_layer(self, layer: str) -> None:
        self.diagram.set_layer(layer)

    def _export_diagram(self) -> None:
        path, selected_filter = QFileDialog.getSaveFileName(
            self, "Export keyboard diagram", "keybchord-keymap.png",
            "PNG image (*.png);;SVG image (*.svg)")
        if not path:
            return
        lower = path.lower()
        if lower.endswith(".png"):
            wants_svg = False
        elif lower.endswith(".svg"):
            wants_svg = True
        else:
            wants_svg = selected_filter.startswith("SVG")
        if wants_svg and not lower.endswith(".svg"):
            path += ".svg"
        elif not wants_svg and not lower.endswith(".png"):
            path += ".png"
        try:
            if wants_svg:
                self.diagram.export_svg(path)
            else:
                self.diagram.export_png(path)
        except Exception as exc:  # noqa: BLE001
            QMessageBox.warning(self, "Export failed", str(exc))

    def refresh(self) -> None:
        if self._tree is None:
            return
        selected = self._selected_row()
        self.table.setRowCount(len(self._tree.keymap.bindings))
        for i, binding in enumerate(self._tree.keymap.bindings):
            combo = (f"{binding.modifier}+" if binding.modifier else "") + binding.key
            self._set_item(i, 0, combo)
            self._set_item(i, 1, binding.action.type)
            self._set_item(i, 2, binding.action.param or binding.action.menu or "")
            self._set_item(i, 3, self._value_text(binding.action))
        self.diagram.set_keymap(
            self._tree.keymap,
            rhythm_names=self._tree.rhythm_names(),
            bass_names=self._tree.bass_names(),
            chord_pattern_names=self._tree.chord_pattern_names(),
        )
        if 0 <= selected < self.table.rowCount():
            self.table.selectRow(selected)
            self.diagram.set_selected_binding(selected)
        else:
            self.table.clearSelection()
            self.diagram.set_selected_binding(-1)
        self._update_status()

    def _set_item(self, row: int, col: int, text: str) -> None:
        item = QTableWidgetItem(text)
        item.setFlags(item.flags() & ~Qt.ItemIsEditable)
        self.table.setItem(row, col, item)

    @staticmethod
    def _value_text(action: KeyAction) -> str:
        if action.type == "set":
            return str(action.value)
        if action.type == "toggle":
            return f"{action.a} \u2192 {action.b}"
        if action.type == "drum_mute":
            return f"index {action.index}"
        if action.type == "preset_load":
            return f"slot {action.slot}"
        return ""

    def _update_status(self) -> None:
        if self._tree is None:
            return
        result = validate_keymap(self._tree.keymap.to_dict(),
                                 rhythm_names=self._tree.rhythm_names(),
                                 bass_names=self._tree.bass_names())
        if result.ok():
            self.status_label.setText("Keymap valid.")
            self.status_label.setStyleSheet("color: #3fa34d;")
        else:
            self.status_label.setText("Problems:\n" + "\n".join(result.errors))
            self.status_label.setStyleSheet("color: #e05353;")

    def _selected_row(self) -> int:
        rows = self.table.selectionModel().selectedRows()
        return rows[0].row() if rows else -1

    def _select_binding_row(self, index: int) -> None:
        if 0 <= index < self.table.rowCount():
            self.table.selectRow(index)
            item = self.table.item(index, 0)
            if item is not None:
                self.table.scrollToItem(item)
            self.diagram.set_selected_binding(index)
        else:
            self.table.clearSelection()
            self.diagram.set_selected_binding(-1)

    def _sync_diagram_selection(self) -> None:
        rows = self.table.selectionModel().selectedRows()
        self.diagram.set_selected_binding(rows[0].row() if rows else -1)

    def _edit_binding_index(self, index: int) -> None:
        if self._tree is None:
            return
        if not 0 <= index < len(self._tree.keymap.bindings):
            return
        self._select_binding_row(index)
        self._edit_binding()

    def _add_binding_for_key(self, key: str, modifier: Optional[str]) -> None:
        if self._tree is None:
            return
        dialog = BindingDialog(
            self,
            rhythm_names=self._tree.rhythm_names(),
            bass_names=self._tree.bass_names(),
            initial_key=key,
            initial_modifier=modifier,
        )
        if dialog.exec() == QDialog.Accepted:
            self._tree.keymap.bindings.append(dialog.result_binding())
            self.refresh()
            self._select_binding_row(len(self._tree.keymap.bindings) - 1)
            self.changed.emit()

    def _add_binding(self) -> None:
        if self._tree is None:
            return
        dialog = BindingDialog(self, rhythm_names=self._tree.rhythm_names(),
                               bass_names=self._tree.bass_names())
        if dialog.exec() == QDialog.Accepted:
            self._tree.keymap.bindings.append(dialog.result_binding())
            self.refresh()
            self._select_binding_row(len(self._tree.keymap.bindings) - 1)
            self.changed.emit()

    def _edit_binding(self) -> None:
        if self._tree is None:
            return
        row = self._selected_row()
        if row < 0:
            return
        dialog = BindingDialog(self, binding=self._tree.keymap.bindings[row],
                               rhythm_names=self._tree.rhythm_names(),
                               bass_names=self._tree.bass_names())
        if dialog.exec() == QDialog.Accepted:
            self._tree.keymap.bindings[row] = dialog.result_binding()
            self.refresh()
            self._select_binding_row(row)
            self.changed.emit()

    def _remove_binding(self) -> None:
        if self._tree is None:
            return
        row = self._selected_row()
        if row < 0:
            return
        del self._tree.keymap.bindings[row]
        self.refresh()
        if self._tree.keymap.bindings:
            self._select_binding_row(min(row, len(self._tree.keymap.bindings) - 1))
        self.changed.emit()

    def _reset_default(self) -> None:
        if self._tree is None:
            return
        self._tree.keymap = default_keymap()
        self.refresh()
        self.changed.emit()
