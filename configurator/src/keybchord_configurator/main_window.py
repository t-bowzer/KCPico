"""Main application window."""

from __future__ import annotations

import tempfile
from pathlib import Path

from PySide6.QtCore import Qt
from PySide6.QtGui import QAction
from PySide6.QtWidgets import (
    QFileDialog,
    QMainWindow,
    QMessageBox,
    QTabWidget,
)

from .io import backup as backup_io
from .io import device as device_io
from .io.storage import ConfigTree, default_tree, load_tree, save_tree
from .ui.backup_view import BackupView
from .ui.bass_editor import BassEditor
from .ui.busy import run_with_progress
from .ui.config_editor import ConfigEditor
from .ui.json_validator_view import ValidatorView, validate_directory, validate_tree
from .ui.keymap_editor import KeymapEditor
from .ui.presets_editor import PresetsEditor
from .ui.rhythm_editor import RhythmEditor


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("KeybChord Configurator")
        self.resize(1100, 760)
        self._tree: ConfigTree = default_tree()
        self._dirty = False
        self._last_directory: str | None = None
        self._build_ui()
        self._build_actions()
        self._refresh_all()
        self._update_title()

    # -- construction -------------------------------------------------------
    def _build_ui(self) -> None:
        self.tabs = QTabWidget()
        self.presets_editor = PresetsEditor()
        self.rhythm_editor = RhythmEditor()
        self.bass_editor = BassEditor()
        self.keymap_editor = KeymapEditor()
        self.config_editor = ConfigEditor()
        self.validator_view = ValidatorView()
        self.backup_view = BackupView()

        self.tabs.addTab(self.presets_editor, "Presets")
        self.tabs.addTab(self.rhythm_editor, "Rhythms")
        self.tabs.addTab(self.bass_editor, "Bass")
        self.tabs.addTab(self.keymap_editor, "Keymap")
        self.tabs.addTab(self.config_editor, "Settings")
        self.tabs.addTab(self.validator_view, "Validate")
        self.tabs.addTab(self.backup_view, "Backup")
        self.setCentralWidget(self.tabs)

        for editor in (self.presets_editor, self.rhythm_editor, self.bass_editor,
                       self.keymap_editor, self.config_editor):
            editor.changed.connect(self._mark_dirty)

        self.presets_editor.connect_signals()
        self.validator_view.validate_current_requested.connect(self._validate_current)
        self.backup_view.backup_requested.connect(self._backup)
        self.backup_view.restore_requested.connect(self._restore)

    def _build_actions(self) -> None:
        file_menu = self.menuBar().addMenu("&File")

        def add_action(menu, text, shortcut, handler):
            action = QAction(text, self)
            if shortcut:
                action.setShortcut(shortcut)
            action.triggered.connect(handler)
            menu.addAction(action)
            return action

        add_action(file_menu, "&New", "Ctrl+N", self._new_document)
        add_action(file_menu, "&Open device...", "Ctrl+O", self._open_device)
        add_action(file_menu, "Open &folder...", "Ctrl+Shift+O", self._open_folder)
        file_menu.addSeparator()
        add_action(file_menu, "&Save to device...", "Ctrl+S", self._save_to_device)
        add_action(file_menu, "Save to &folder...", "Ctrl+Shift+S", self._save_to_folder)
        add_action(file_menu, "&Eject device", None, self._eject_device)
        file_menu.addSeparator()
        add_action(file_menu, "&Back up...", None, self._backup)
        add_action(file_menu, "&Restore...", None, self._restore)
        file_menu.addSeparator()
        add_action(file_menu, "&Quit", "Ctrl+Q", self.close)

        tools_menu = self.menuBar().addMenu("&Tools")
        add_action(tools_menu, "&Validate current document", "F5", self._validate_current)
        add_action(tools_menu, "Validate &folder...", None, self._validate_folder)

    # -- tree management ----------------------------------------------------
    def _refresh_all(self) -> None:
        self.config_editor.load(self._tree.config)
        self.presets_editor.set_tree(self._tree)
        self.rhythm_editor.set_tree(self._tree)
        self.bass_editor.set_tree(self._tree)
        self.keymap_editor.set_tree(self._tree)
        self.presets_editor.refresh_pattern_lists()
        self._update_title()

    def _store_all(self) -> None:
        self.config_editor.store(self._tree.config)
        self.presets_editor.store_current()

    def _mark_dirty(self, *_args) -> None:
        self._dirty = True
        self._update_title()

    def _update_title(self) -> None:
        marker = "*" if self._dirty else ""
        self.setWindowTitle(f"KeybChord Configurator{marker}")

    def _set_tree(self, tree: ConfigTree) -> None:
        self._tree = tree
        self._dirty = False
        self._refresh_all()

    def _new_document(self) -> None:
        if not self._confirm_discard():
            return
        self._set_tree(default_tree())

    # -- open ---------------------------------------------------------------
    def _open_device(self) -> None:
        if not self._confirm_discard():
            return
        holder: dict = {}

        def find() -> str:
            holder["path"] = device_io.find_device()
            return holder["path"]

        def load():
            return load_tree(holder["path"]) if holder["path"] else None

        try:
            results = run_with_progress(self, [
                ("Looking for the KeybChord volume...", find),
                ("Loading configuration from the device...", load),
            ])
        except Exception as exc:  # noqa: BLE001
            QMessageBox.warning(self, "Error", f"Could not open device: {exc}")
            return
        path = holder.get("path")
        if path is None:
            QMessageBox.information(
                self, "Device not found",
                "Could not find a mounted 'KeybChord' volume.\n"
                "Power the device on while holding Ctrl, or choose a folder.")
            return
        self._last_directory = path
        self._set_tree(results[1])
        self.statusBar().showMessage(f"Loaded {path}", 5000)

    def _open_folder(self) -> None:
        directory = QFileDialog.getExistingDirectory(self, "Open configuration folder")
        if directory:
            self._load_directory(directory)

    def _load_directory(self, directory: str) -> None:
        if not self._confirm_discard():
            return
        try:
            tree = run_with_progress(self, [
                (f"Loading configuration from {directory}...",
                 lambda: load_tree(directory)),
            ])[0]
        except Exception as exc:  # noqa: BLE001
            QMessageBox.warning(self, "Error", f"Could not load configuration: {exc}")
            return
        self._last_directory = directory
        self._set_tree(tree)
        self.statusBar().showMessage(f"Loaded {directory}", 5000)

    # -- save ---------------------------------------------------------------
    def _save_to_device(self) -> None:
        self._store_all()
        if self._confirm_invalid():
            return
        holder: dict = {}

        def find() -> str:
            holder["path"] = device_io.find_device()
            return holder["path"]

        def do_save():
            if holder["path"] is None:
                return None
            save_tree(self._tree, holder["path"])
            return True

        try:
            run_with_progress(self, [
                ("Looking for the KeybChord volume...", find),
                ("Saving configuration to the device...", do_save),
            ])
        except Exception as exc:  # noqa: BLE001
            QMessageBox.warning(self, "Error", f"Could not save to device: {exc}")
            return
        path = holder.get("path")
        if path is None:
            ret = QMessageBox.question(
                self, "Device not found",
                "No 'KeybChord' volume found. Save to a folder instead?")
            if ret == QMessageBox.Yes:
                self._save_to_folder()
            return
        self._dirty = False
        self._update_title()
        self.statusBar().showMessage(f"Saved to device ({path})", 5000)
        ret = QMessageBox.question(
            self, "Eject device", "Configuration saved. Eject the device now?")
        if ret == QMessageBox.Yes:
            self._eject(path)

    def _save_to_folder(self) -> None:
        self._store_all()
        start = self._last_directory or ""
        directory = QFileDialog.getExistingDirectory(
            self, "Save configuration to folder", start)
        if not directory:
            return
        if self._confirm_invalid():
            return
        try:
            run_with_progress(self, [
                (f"Saving configuration to {directory}...",
                 lambda: save_tree(self._tree, directory)),
            ])
        except Exception as exc:  # noqa: BLE001
            QMessageBox.warning(self, "Error", f"Could not save: {exc}")
            return
        self._last_directory = directory
        self._dirty = False
        self._update_title()
        self.statusBar().showMessage(f"Saved to {directory}", 5000)

    def _confirm_invalid(self) -> bool:
        """Return True if the user wants to abort the save."""
        results = validate_tree(self._tree)
        errors = [(label, r.errors) for label, r in results if r.errors]
        if not errors:
            return False
        lines = []
        for label, errs in errors[:10]:
            for e in errs[:3]:
                lines.append(f"{label}: {e}")
        detail = "\n".join(lines)
        if len(lines) < sum(len(e) for _l, e in errors):
            detail += "\n..."
        ret = QMessageBox.warning(
            self, "Validation errors",
            "The configuration has validation errors:\n\n" + detail +
            "\n\nSave anyway?",
            QMessageBox.Save | QMessageBox.Cancel, QMessageBox.Cancel)
        return ret != QMessageBox.Save

    # -- eject --------------------------------------------------------------
    def _eject_device(self) -> None:
        try:
            path = run_with_progress(self, [
                ("Looking for the KeybChord volume...", device_io.find_device),
            ])[0]
        except Exception as exc:  # noqa: BLE001
            QMessageBox.warning(self, "Error", f"Could not find device: {exc}")
            return
        if path is None:
            QMessageBox.information(self, "Device not found",
                                    "No 'KeybChord' volume is mounted.")
            return
        self._eject(path)

    def _eject(self, path: str) -> None:
        try:
            ok, message = run_with_progress(self, [
                (f"Ejecting {path}...", lambda: device_io.eject(path)),
            ])[0]
        except Exception as exc:  # noqa: BLE001
            QMessageBox.warning(self, "Eject failed", str(exc))
            return
        if ok:
            QMessageBox.information(self, "Ejected", message)
        else:
            QMessageBox.warning(self, "Eject failed", message)

    # -- backup / restore ---------------------------------------------------
    def _backup(self) -> None:
        self._store_all()
        zip_path, _filter = QFileDialog.getSaveFileName(
            self, "Back up configuration", "keybchord-backup.zip", "Zip (*.zip)")
        if not zip_path:
            return
        if not zip_path.lower().endswith(".zip"):
            zip_path += ".zip"

        def do_backup():
            with tempfile.TemporaryDirectory() as td:
                save_tree(self._tree, td)
                return backup_io.backup_to_zip(td, zip_path)

        try:
            archived = run_with_progress(self, [
                ("Backing up configuration...", do_backup),
            ])[0]
        except Exception as exc:  # noqa: BLE001
            QMessageBox.warning(self, "Error", f"Could not back up: {exc}")
            return
        self.backup_view.show_status(
            f"Backed up {len(archived)} files to {zip_path}")
        self.statusBar().showMessage(f"Backed up to {zip_path}", 5000)

    def _restore(self) -> None:
        zip_path, _filter = QFileDialog.getOpenFileName(
            self, "Restore from backup", "", "Zip (*.zip)")
        if not zip_path:
            return
        directory = QFileDialog.getExistingDirectory(
            self, "Restore into folder", self._last_directory or "")
        if not directory:
            return
        try:
            restored = run_with_progress(self, [
                (f"Restoring configuration to {directory}...",
                 lambda: backup_io.restore_from_zip(zip_path, directory)),
            ])[0]
        except Exception as exc:  # noqa: BLE001
            QMessageBox.warning(self, "Error", f"Could not restore: {exc}")
            return
        if not restored:
            QMessageBox.warning(self, "Restore",
                                "No recognized configuration files in the archive.")
            return
        self._load_directory(directory)
        self.backup_view.show_status(
            f"Restored {len(restored)} files into {directory}")
        self.statusBar().showMessage(f"Restored from {zip_path}", 5000)

    # -- validation ---------------------------------------------------------
    def _validate_current(self) -> None:
        self._store_all()
        self.validator_view.show_results(validate_tree(self._tree))

    def _validate_folder(self) -> None:
        directory = QFileDialog.getExistingDirectory(self, "Validate folder")
        if directory:
            self.validator_view.show_results(validate_directory(directory))

    # -- misc ---------------------------------------------------------------
    def _confirm_discard(self) -> bool:
        if not self._dirty:
            return True
        ret = QMessageBox.question(
            self, "Unsaved changes",
            "Discard unsaved changes?",
            QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
        return ret == QMessageBox.Yes

    def closeEvent(self, event) -> None:
        if self._confirm_discard():
            event.accept()
        else:
            event.ignore()
