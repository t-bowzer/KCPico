"""Backup / restore view."""

from __future__ import annotations

from PySide6.QtCore import Signal
from PySide6.QtWidgets import (
    QHBoxLayout,
    QLabel,
    QPushButton,
    QVBoxLayout,
    QWidget,
)


class BackupView(QWidget):
    backup_requested = Signal()
    restore_requested = Signal()

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self._build()

    def _build(self) -> None:
        layout = QVBoxLayout(self)
        layout.addWidget(QLabel(
            "Back up the whole configuration to a .zip, or restore one."))

        row = QHBoxLayout()
        self.backup_btn = QPushButton("Back up configuration...")
        self.backup_btn.clicked.connect(self.backup_requested.emit)
        self.restore_btn = QPushButton("Restore from backup...")
        self.restore_btn.clicked.connect(self.restore_requested.emit)
        row.addWidget(self.backup_btn)
        row.addWidget(self.restore_btn)
        row.addStretch(1)
        layout.addLayout(row)

        self.status_label = QLabel("")
        self.status_label.setWordWrap(True)
        layout.addWidget(self.status_label)
        layout.addStretch(1)

    def show_status(self, text: str) -> None:
        self.status_label.setText(text)
