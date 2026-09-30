"""Shared Qt widget helpers."""

from __future__ import annotations

from typing import Optional

from PySide6.QtWidgets import (
    QCheckBox,
    QComboBox,
    QSpinBox,
)


def int_spin(minimum: int, maximum: int, step: int, value: int) -> QSpinBox:
    box = QSpinBox()
    box.setRange(minimum, maximum)
    box.setSingleStep(step)
    box.setValue(value)
    return box


def enum_combo(labels: dict[str, int], current: int,
               placeholder: Optional[str] = None) -> QComboBox:
    combo = QComboBox()
    for label, value in labels.items():
        combo.addItem(label, value)
    idx = combo.findData(current)
    if idx >= 0:
        combo.setCurrentIndex(idx)
    if placeholder is not None:
        combo.setPlaceholderText(placeholder)
    return combo


def bool_check(value: bool, label: str = "") -> QCheckBox:
    check = QCheckBox(label)
    check.setChecked(value)
    return check
