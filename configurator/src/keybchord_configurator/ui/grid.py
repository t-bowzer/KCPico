"""A reusable clickable step grid for pattern editing.

Displays a grid of ``rows`` x ``columns`` integer cells. Each cell's value is
cycled through a caller-supplied ``palette`` on left-click (and cycled backwards
on right-click). Beat markers are drawn every ``RHYTHM_STEPS_PER_BEAT`` (4)
columns.
"""

from __future__ import annotations

from typing import Callable, Optional

from PySide6.QtCore import Qt, Signal
from PySide6.QtGui import QColor, QFont, QPainter, QPen
from PySide6.QtWidgets import QWidget

BEATS_PER_GROUP = 4
CELL_W = 26
CELL_H = 20
ROW_LABEL_W = 90


class StepGrid(QWidget):
    cellChanged = Signal(int, int, int)  # row, column, value

    def __init__(self, parent: Optional[QWidget] = None) -> None:
        super().__init__(parent)
        self._values: list[list[int]] = []
        self._row_labels: list[str] = []
        self._palette: list[int] = [0, 1]
        self._value_text: Callable[[int], str] = lambda v: str(v) if v else ""
        self._value_color: Callable[[int], QColor] = lambda v: QColor(
            "#2d7dd2" if v else "#2a2a2a"
        )
        self._cursor: int = -1
        self._click_handler = None
        self.setMinimumSize(200, 120)

    # -- configuration ------------------------------------------------------
    def set_palette(self, palette: list[int]) -> None:
        self._palette = list(palette)

    def set_click_handler(self, fn) -> None:
        """Override the default click behavior.

        ``fn(row, col, button)`` receives the cell and the ``Qt.MouseButton``.
        When set, left/right clicks no longer cycle the palette.
        """
        self._click_handler = fn

    def set_value_text(self, fn: Callable[[int], str]) -> None:
        self._value_text = fn

    def set_value_color(self, fn: Callable[[int], QColor]) -> None:
        self._value_color = fn

    def set_row_labels(self, labels: list[str]) -> None:
        self._row_labels = list(labels)

    # -- data ---------------------------------------------------------------
    def set_data(self, rows: list[list[int]]) -> None:
        self._values = [list(r) for r in rows]
        self._normalize_rows()
        self._resize()
        self.update()

    def _normalize_rows(self) -> None:
        cols = self._columns()
        for row in self._values:
            while len(row) < cols:
                row.append(0)

    def value(self, row: int, col: int) -> int:
        if 0 <= row < len(self._values) and 0 <= col < len(self._values[row]):
            return self._values[row][col]
        return 0

    def set_value(self, row: int, col: int, value: int) -> None:
        if 0 <= row < len(self._values) and 0 <= col < len(self._values[row]):
            self._values[row][col] = value
            self.update()

    def rows(self) -> int:
        return len(self._values)

    def columns(self) -> int:
        return self._columns()

    def _columns(self) -> int:
        return max((len(r) for r in self._values), default=0)

    def set_cursor(self, step: int) -> None:
        self._cursor = step
        self.update()

    def clear_cursor(self) -> None:
        self._cursor = -1
        self.update()

    # -- sizing -------------------------------------------------------------
    def _resize(self) -> None:
        w = ROW_LABEL_W + self._columns() * CELL_W + 2
        h = self.rows() * CELL_H + 2
        self.setMinimumSize(max(200, w), max(80, h))
        self.resize(max(200, w), max(80, h))

    def sizeHint(self):
        from PySide6.QtCore import QSize
        return QSize(ROW_LABEL_W + self._columns() * CELL_W + 2,
                     self.rows() * CELL_H + 2)

    # -- painting -----------------------------------------------------------
    def paintEvent(self, event) -> None:
        painter = QPainter(self)
        painter.fillRect(self.rect(), QColor("#1e1e1e"))

        cols = self._columns()
        if cols == 0:
            painter.end()
            return

        beat_fill = QColor("#26262b")
        downbeat_fill = QColor("#2c2c33")
        border = QColor("#3a3a3f")

        # Beat-group background shading.
        for col in range(cols):
            x = ROW_LABEL_W + col * CELL_W
            group = col // BEATS_PER_GROUP
            color = downbeat_fill if group % 2 == 0 else beat_fill
            painter.fillRect(x, 0, CELL_W, self.rows() * CELL_H, color)

        # Cells.
        cell_font = QFont(painter.font())
        cell_font.setPointSize(8)
        painter.setFont(cell_font)
        for r in range(self.rows()):
            for c in range(cols):
                x = ROW_LABEL_W + c * CELL_W
                y = r * CELL_H
                value = self.value(r, c)
                painter.fillRect(x + 1, y + 1, CELL_W - 2, CELL_H - 2,
                                 self._value_color(value))
                text = self._value_text(value)
                if text:
                    painter.setPen(QColor("#ffffff"))
                    painter.drawText(x, y, CELL_W, CELL_H, Qt.AlignCenter, text)

        # Row labels.
        painter.setPen(QColor("#d0d0d0"))
        for r in range(self.rows()):
            label = self._row_labels[r] if r < len(self._row_labels) else str(r)
            painter.drawText(2, r * CELL_H, ROW_LABEL_W - 4, CELL_H,
                             Qt.AlignLeft | Qt.AlignVCenter, label)

        # Beat markers (vertical lines every group, downbeat heavier).
        for c in range(0, cols + 1):
            x = ROW_LABEL_W + c * CELL_W
            pen = QPen(border)
            pen.setWidth(2 if c % BEATS_PER_GROUP == 0 else 1)
            painter.setPen(pen)
            painter.drawLine(x, 0, x, self.rows() * CELL_H)

        # Playback cursor.
        if 0 <= self._cursor < cols:
            x = ROW_LABEL_W + self._cursor * CELL_W
            painter.setPen(QPen(QColor("#ffb000"), 2))
            painter.drawRect(x + 1, 1, CELL_W - 2, self.rows() * CELL_H - 2)

        painter.end()

    # -- interaction --------------------------------------------------------
    def mousePressEvent(self, event) -> None:
        if not self._values:
            return
        x = event.position().x() - ROW_LABEL_W
        y = event.position().y()
        col = int(x) // CELL_W
        row = int(y) // CELL_H
        if not (0 <= row < self.rows() and 0 <= col < self._columns()):
            return
        if self._click_handler is not None:
            self._click_handler(row, col, event.button())
            return
        if not self._palette:
            return
        if event.button() == Qt.LeftButton:
            self._cycle(row, col, +1)
        elif event.button() == Qt.RightButton:
            self._cycle(row, col, -1)

    def _cycle(self, row: int, col: int, direction: int) -> None:
        value = self.value(row, col)
        try:
            idx = self._palette.index(value)
        except ValueError:
            idx = 0
        idx = (idx + direction) % len(self._palette)
        new_value = self._palette[idx]
        self.set_value(row, col, new_value)
        self.cellChanged.emit(row, col, new_value)
