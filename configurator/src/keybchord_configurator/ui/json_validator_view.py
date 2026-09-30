"""JSON validator view (validates a folder or the in-memory document)."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Optional

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (
    QFileDialog,
    QHBoxLayout,
    QHeaderView,
    QLabel,
    QPlainTextEdit,
    QPushButton,
    QTableWidget,
    QTableWidgetItem,
    QVBoxLayout,
    QWidget,
)

from ..io.storage import (
    BASS_DIR,
    CONFIG_FILE,
    KEYMAP_FILE,
    PRESETS_DIR,
    RHYTHMS_DIR,
    ConfigTree,
    load_tree,
)
from ..models.bass import BassPattern
from ..models.rhythm import RhythmPattern
from ..validation.validator import (
    validate_bass,
    validate_config,
    validate_keymap,
    validate_preset_bank,
    validate_rhythm,
)


def _collect_rhythm_names(directory: Path) -> list[str]:
    names = []
    for path in sorted((directory / RHYTHMS_DIR).glob("*.json")):
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except (json.JSONDecodeError, OSError):
            continue
        pattern = RhythmPattern.from_dict(data)
        names.append(pattern.name or path.stem)
    return names


def _collect_bass_names(directory: Path) -> list[str]:
    names = []
    for path in sorted((directory / BASS_DIR).glob("*.json")):
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except (json.JSONDecodeError, OSError):
            continue
        pattern = BassPattern.from_dict(data)
        names.append(pattern.name or path.stem)
    return names


def validate_directory(directory: str | Path) -> list[tuple[str, object]]:
    """Validate all config files in a directory.

    Returns a list of ``(label, ValidationResult)``.
    """
    root = Path(directory)
    results: list[tuple[str, object]] = []

    def add(label: str, path: Path, validator, **kwargs) -> None:
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as exc:
            from ..validation.errors import ValidationResult
            result = ValidationResult(errors=[f"invalid JSON: {exc.msg}"])
            results.append((label, result))
            return
        except OSError as exc:
            from ..validation.errors import ValidationResult
            result = ValidationResult(errors=[str(exc)])
            results.append((label, result))
            return
        results.append((label, validator(data, **kwargs)))

    rhythm_names = _collect_rhythm_names(root) or None
    bass_names = _collect_bass_names(root) or None

    add(CONFIG_FILE, root / CONFIG_FILE, validate_config)
    add(KEYMAP_FILE, root / KEYMAP_FILE, validate_keymap,
        rhythm_names=rhythm_names, bass_names=bass_names)

    presets_dir = root / PRESETS_DIR
    if presets_dir.exists():
        for path in sorted(presets_dir.glob("bank*.json")):
            add(f"{PRESETS_DIR}/{path.name}", path, validate_preset_bank,
                rhythm_names=rhythm_names, bass_names=bass_names)

    for subdir, validator in ((RHYTHMS_DIR, validate_rhythm), (BASS_DIR, validate_bass)):
        directory_path = root / subdir
        if not directory_path.exists():
            continue
        for path in sorted(directory_path.glob("*.json")):
            add(f"{subdir}/{path.name}", path, validator, filename=path.name)

    return results


def validate_tree(tree: ConfigTree) -> list[tuple[str, object]]:
    """Validate the in-memory tree. Returns ``(label, ValidationResult)``."""
    results: list[tuple[str, object]] = []
    rhythm_names = tree.rhythm_names()
    bass_names = tree.bass_names()
    results.append((CONFIG_FILE, validate_config(tree.config.to_dict())))
    results.append((KEYMAP_FILE, validate_keymap(
        tree.keymap.to_dict(), rhythm_names=rhythm_names, bass_names=bass_names)))
    for bank in range(10):
        results.append((f"{PRESETS_DIR}/bank{bank + 1}.json",
                        validate_preset_bank(
                            [s.to_dict() for s in tree.banks[bank]],
                            rhythm_names=rhythm_names, bass_names=bass_names)))
    for filename, pattern in tree.rhythms:
        results.append((f"{RHYTHMS_DIR}/{filename}",
                        validate_rhythm(pattern.to_dict(), filename=filename)))
    for filename, pattern in tree.bass:
        results.append((f"{BASS_DIR}/{filename}",
                        validate_bass(pattern.to_dict(), filename=filename)))
    return results


class ValidatorView(QWidget):
    validate_current_requested = Signal()

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self._results: list[tuple[str, object]] = []
        self._build()

    def _build(self) -> None:
        layout = QVBoxLayout(self)

        top = QHBoxLayout()
        self.folder_btn = QPushButton("Validate folder...")
        self.folder_btn.clicked.connect(self._choose_folder)
        self.current_btn = QPushButton("Validate current document")
        self.current_btn.clicked.connect(self.validate_current_requested.emit)
        top.addWidget(self.folder_btn)
        top.addWidget(self.current_btn)
        top.addStretch(1)
        layout.addLayout(top)

        self.table = QTableWidget(0, 3)
        self.table.setHorizontalHeaderLabels(["File", "Errors", "Warnings"])
        self.table.horizontalHeader().setSectionResizeMode(0, QHeaderView.Stretch)
        self.table.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeToContents)
        self.table.horizontalHeader().setSectionResizeMode(2, QHeaderView.ResizeToContents)
        self.table.setEditTriggers(QTableWidget.NoEditTriggers)
        self.table.setSelectionBehavior(QTableWidget.SelectRows)
        self.table.currentCellChanged.connect(self._on_select)
        layout.addWidget(self.table, 1)

        self.details = QPlainTextEdit()
        self.details.setReadOnly(True)
        layout.addWidget(self.details)

    def _choose_folder(self) -> None:
        directory = QFileDialog.getExistingDirectory(self, "Choose configuration folder")
        if not directory:
            return
        self.show_results(validate_directory(directory))

    def show_results(self, results: list[tuple[str, object]]) -> None:
        self._results = results
        self.table.setRowCount(len(results))
        for row, (label, result) in enumerate(results):
            file_item = QTableWidgetItem(label)
            file_item.setFlags(file_item.flags() & ~Qt.ItemIsEditable)
            self.table.setItem(row, 0, file_item)
            err_item = QTableWidgetItem(str(len(result.errors)))
            err_item.setFlags(err_item.flags() & ~Qt.ItemIsEditable)
            if result.errors:
                err_item.setForeground(Qt.red)
            self.table.setItem(row, 1, err_item)
            warn_item = QTableWidgetItem(str(len(result.warnings)))
            warn_item.setFlags(warn_item.flags() & ~Qt.ItemIsEditable)
            if result.warnings:
                warn_item.setForeground(Qt.darkYellow)
            self.table.setItem(row, 2, warn_item)
        self.details.clear()

    def _on_select(self, row: int, _col: int, _prev_row: int, _prev_col: int) -> None:
        if not (0 <= row < len(self._results)):
            self.details.clear()
            return
        _label, result = self._results[row]
        lines = []
        for e in result.errors:
            lines.append("ERROR: " + e)
        for w in result.warnings:
            lines.append("WARNING: " + w)
        self.details.setPlainText("\n".join(lines) or "OK")
