"""Interactive ANSI 104-key diagram for the keymap editor."""

from __future__ import annotations

from typing import Iterable, Optional

from PySide6.QtCore import QPointF, QRectF, QSize, Qt, Signal
from PySide6.QtGui import (
    QColor,
    QFont,
    QFontMetrics,
    QImage,
    QPainter,
    QPen,
)
from PySide6.QtWidgets import (
    QMenu,
    QSizePolicy,
    QToolTip,
    QWidget,
)

from .keymap_visual import (
    ALL_KEYS,
    CHORD_QUALITY_COLORS,
    CHORD_QUALITY_LABELS,
    HARDCODED_COMBOS,
    LAYER_ALL,
    LAYER_NONE,
    LAYER_ORDER,
    LAYOUT_HEIGHT,
    LAYOUT_WIDTH,
    MODIFIER_COLORS,
    KeySpec,
    binding_index,
    combo_label,
    describe_key,
    format_action,
    is_macro_key,
    macro_command_label,
    macro_key_cell_text,
)

_BACKGROUND = "#1e1e1e"
_BOUND_FILL = "#24435f"
_BOUND_BORDER = "#4f8fc0"
_BOUND_TEXT = "#f2f2f2"
_UNBOUND_FILL = "#2a2a2a"
_UNBOUND_BORDER = "#3a3a3f"
_UNBOUND_TEXT = "#bdbdbd"
_STRUM_FILL = "#273130"
_MACRO_FILL = "#2f2938"
_MACRO_BORDER = "#9b7bc4"
_MACRO_TEXT = "#d9c9ee"
_RESERVED_FILL = "#232323"
_OTHER_FILL = "#242424"
_FIXED_BORDER = "#343434"
_FIXED_TEXT = "#909090"
_SELECTED = "#ffb000"
_HOVER = "#c8c8c8"


class KeyboardDiagram(QWidget):
    bindingSelected = Signal(int)
    bindingEditRequested = Signal(int)
    unboundKeyActivated = Signal(str, object)
    macroKeySelected = Signal(str)
    macroKeyEditRequested = Signal(str)

    def __init__(self, parent: Optional[QWidget] = None) -> None:
        super().__init__(parent)
        self._bindings: list = []
        self._by_key: dict[str, list[int]] = {}
        self._by_combo: dict[tuple[Optional[str], str], int] = {}
        self._rhythm_names: list[str] = []
        self._bass_names: list[str] = []
        self._chord_pattern_names: list[str] = []
        self._macro_config = None
        self._macro_preset_label = ""
        self._selected_macro_key = ""
        self._layer = LAYER_ALL
        self._selected = -1
        self._hover_key = ""
        self._unit = 1.0
        self._origin = QPointF(0.0, 0.0)
        self.setMouseTracking(True)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.setMinimumSize(680, 250)

    def set_keymap(self, keymap, *, rhythm_names: Iterable[str] = (),
                   bass_names: Iterable[str] = (),
                   chord_pattern_names: Iterable[str] = ()) -> None:
        self._bindings = list(keymap.bindings) if keymap is not None else []
        self._by_key, self._by_combo = binding_index(self._bindings)
        self._rhythm_names = list(rhythm_names)
        self._bass_names = list(bass_names)
        self._chord_pattern_names = list(chord_pattern_names)
        self._selected = -1
        self.update()

    def set_macro_context(self, macro_config, preset_label: str = "") -> None:
        self._macro_config = macro_config
        self._macro_preset_label = preset_label
        if not is_macro_key(self._selected_macro_key, macro_config):
            self._selected_macro_key = ""
        self.update()

    def set_selected_macro_key(self, label: str) -> None:
        self._selected_macro_key = label
        self.update()

    def _macro_key_active(self, spec: KeySpec) -> bool:
        return (self._layer in (LAYER_ALL, LAYER_NONE)
                and is_macro_key(spec.name, self._macro_config))

    def set_layer(self, layer: str) -> None:
        self._layer = layer if layer in LAYER_ORDER else LAYER_ALL
        self.update()

    def layer(self) -> str:
        return self._layer

    def set_selected_binding(self, index: int) -> None:
        self._selected = index
        self.update()

    def selected_binding(self) -> int:
        return self._selected

    def sizeHint(self) -> QSize:
        return QSize(1000, 320)

    def minimumSizeHint(self) -> QSize:
        return QSize(680, 250)

    def export_png(self, path: str, scale: int = 2) -> None:
        width = max(1200, self.width()) * scale
        height = max(300, int(width * self.height() / max(1, self.width())))
        image = QImage(width, height, QImage.Format_ARGB32_Premultiplied)
        image.fill(QColor(_BACKGROUND))
        painter = QPainter(image)
        painter.setRenderHint(QPainter.Antialiasing, True)
        painter.setRenderHint(QPainter.TextAntialiasing, True)
        saved_unit = self._unit
        saved_origin = QPointF(self._origin)
        saved_selected = self._selected
        saved_hover = self._hover_key
        self._selected = -1
        self._hover_key = ""
        try:
            self._paint(painter, QRectF(0.0, 0.0, float(width), float(height)))
        finally:
            painter.end()
            self._unit = saved_unit
            self._origin = saved_origin
            self._selected = saved_selected
            self._hover_key = saved_hover
        if not image.save(path, "PNG"):
            raise OSError(f"Could not write {path}")

    def export_svg(self, path: str) -> None:
        from PySide6.QtSvg import QSvgGenerator

        width = max(1600, self.width() * 2)
        height = max(400, int(width * self.height() / max(1, self.width())))
        generator = QSvgGenerator()
        generator.setFileName(path)
        generator.setSize(QSize(width, height))
        generator.setViewBox(QRectF(0.0, 0.0, float(width), float(height)))
        generator.setTitle("KeybChord keymap")
        painter = QPainter()
        if not painter.begin(generator):
            raise OSError(f"Could not write {path}")
        painter.setRenderHint(QPainter.Antialiasing, True)
        painter.setRenderHint(QPainter.TextAntialiasing, True)
        saved_unit = self._unit
        saved_origin = QPointF(self._origin)
        saved_selected = self._selected
        saved_hover = self._hover_key
        self._selected = -1
        self._hover_key = ""
        try:
            self._paint(painter, QRectF(0.0, 0.0, float(width), float(height)))
        finally:
            painter.end()
            self._unit = saved_unit
            self._origin = saved_origin
            self._selected = saved_selected
            self._hover_key = saved_hover

    def paintEvent(self, event) -> None:
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing, True)
        painter.setRenderHint(QPainter.TextAntialiasing, True)
        self._paint(painter, QRectF(self.rect()))
        painter.end()

    def _paint(self, painter: QPainter, rect: QRectF) -> None:
        painter.fillRect(rect, QColor(_BACKGROUND))
        if rect.width() <= 2 or rect.height() <= 2:
            return

        margin = max(8.0, rect.width() * 0.01)
        legend_height = max(16.0, min(30.0, rect.height() * 0.08))
        outer = rect.adjusted(margin, margin, -margin, -margin)
        legend_rect = QRectF(outer.left(), outer.top(), outer.width(), legend_height)
        keyboard_rect = QRectF(
            outer.left(),
            outer.top() + legend_height + 4.0,
            outer.width(),
            max(1.0, outer.height() - legend_height - 4.0),
        )

        width_units = max(1.0, LAYOUT_WIDTH)
        height_units = max(1.0, LAYOUT_HEIGHT)
        self._unit = min(
            keyboard_rect.width() / width_units,
            keyboard_rect.height() / height_units,
        )
        drawn_width = width_units * self._unit
        drawn_height = height_units * self._unit
        self._origin = QPointF(
            keyboard_rect.center().x() - drawn_width / 2.0,
            keyboard_rect.center().y() - drawn_height / 2.0,
        )

        self._draw_legend(painter, legend_rect)

        for spec in ALL_KEYS:
            self._draw_key(painter, spec)

    def _draw_legend(self, painter: QPainter, rect: QRectF) -> None:
        font = self._font(max(8, int(self._unit * 0.20)))
        painter.setFont(font)
        metrics = QFontMetrics(font)
        square = max(7.0, self._unit * 0.16)
        x = rect.left()
        y = rect.center().y()
        items = [
            ("Configurable", _BOUND_BORDER),
            ("Ctrl", MODIFIER_COLORS["Ctrl"]),
            ("Alt", MODIFIER_COLORS["Alt"]),
            ("Super", MODIFIER_COLORS["Super"]),
            ("Fixed", _FIXED_TEXT),
        ]
        if is_macro_key("1", self._macro_config):
            items.append(("Macro", _MACRO_BORDER))
        painter.setPen(Qt.NoPen)
        for label, color in items:
            painter.setBrush(QColor(color))
            painter.drawRoundedRect(
                QRectF(x, y - square / 2.0, square, square), 1.5, 1.5
            )
            x += square + 4.0
            painter.setPen(QColor("#c8c8c8"))
            painter.drawText(
                QRectF(x, rect.top(), metrics.horizontalAdvance(label) + 2, rect.height()),
                Qt.AlignLeft | Qt.AlignVCenter,
                label,
            )
            x += metrics.horizontalAdvance(label) + 14.0
            painter.setPen(Qt.NoPen)
        painter.setPen(QColor("#8a8a8a"))
        right_text = f"Layer: {self._layer}"
        if is_macro_key("1", self._macro_config) and self._macro_preset_label:
            right_text += f"  |  Macros: {self._macro_preset_label}"
        painter.drawText(
            rect, Qt.AlignRight | Qt.AlignVCenter, right_text
        )

    def _font(self, pixel_size: int, *, bold: bool = False) -> QFont:
        font = QFont(self.font())
        font.setPixelSize(max(5, pixel_size))
        font.setBold(bold)
        return font

    def _display_binding(self, key: str) -> int:
        indices = self._by_key.get(key, [])
        if not indices:
            return -1
        if self._layer == LAYER_ALL:
            base = self._by_combo.get((None, key))
            return base if base is not None else indices[0]
        modifier = None if self._layer == LAYER_NONE else self._layer
        return self._by_combo.get((modifier, key), -1)

    def _chip_modifiers(self, key: str) -> list[str]:
        result: list[str] = []
        for index in self._by_key.get(key, []):
            modifier = self._bindings[index].modifier
            if modifier is None:
                continue
            if self._layer == LAYER_ALL or modifier == self._layer:
                if modifier not in result:
                    result.append(modifier)
        return result

    def _hardcoded_text(self, key: str) -> str:
        if self._layer != "Super":
            return ""
        for (modifier, combo_key), description in HARDCODED_COMBOS.items():
            if modifier == "Super" and combo_key == key:
                return description.replace("Load preset ", "Load P")
        return ""

    def _draw_key(self, painter: QPainter, spec: KeySpec) -> None:
        unit = self._unit
        rect = QRectF(
            self._origin.x() + spec.x * unit,
            self._origin.y() + spec.y * unit,
            spec.w * unit,
            spec.h * unit,
        ).adjusted(unit * 0.035, unit * 0.035, -unit * 0.035, -unit * 0.035)

        index = self._display_binding(spec.name) if spec.kind == "bindable" else -1
        hardcoded = self._hardcoded_text(spec.name)
        suppressed = self._fixed_suppressed(spec)
        is_macro = self._macro_key_active(spec)
        fill, border, text_color = self._key_colors(spec, index)
        if is_macro:
            fill, border, text_color = _MACRO_FILL, _MACRO_BORDER, _MACRO_TEXT

        if spec.name == self._hover_key:
            border = _HOVER
        if self._selected >= 0 and self._selected < len(self._bindings):
            if self._bindings[self._selected].key == spec.name:
                border = _SELECTED
        if is_macro and spec.name == self._selected_macro_key:
            border = _SELECTED

        painter.setPen(QPen(QColor(border), max(1.0, unit * 0.025)))
        painter.setBrush(QColor(fill))
        radius = max(2.0, unit * 0.10)
        painter.drawRoundedRect(rect, radius, radius)

        show_key_label = (spec.kind in ("bindable", "chord", "strum")
                          or bool(hardcoded) or is_macro)
        if show_key_label:
            key_font = self._font(max(5, int(unit * 0.16)))
            painter.setFont(key_font)
            painter.setPen(QColor("#7f7f7f"))
            painter.drawText(
                rect.adjusted(unit * 0.08, unit * 0.03, -unit * 0.08, 0),
                Qt.AlignLeft | Qt.AlignTop,
                spec.label,
            )

        chips = self._chip_modifiers(spec.name) if spec.kind == "bindable" else []
        chip_height = unit * 0.20 if chips else 0.0
        text_rect = QRectF(
            rect.left() + unit * 0.06,
            rect.top() + unit * (0.22 if show_key_label else 0.08),
            rect.width() - unit * 0.12,
            max(unit * 0.20, rect.height() - unit * (0.28 if show_key_label else 0.14) - chip_height),
        )

        if suppressed:
            self._draw_wrapped_text(
                painter, text_rect, "Off", text_color,
                max(6, int(unit * 0.20)),
            )
        elif is_macro:
            self._draw_wrapped_text(
                painter, text_rect,
                macro_key_cell_text(self._macro_config, spec.name), text_color,
                max(6, int(unit * 0.21)),
            )
        elif spec.kind == "chord":
            self._draw_chord_text(painter, text_rect, spec)
        elif index >= 0:
            label = self._action_label(index)
            self._draw_wrapped_text(
                painter, text_rect, label.short, text_color,
                max(6, int(unit * 0.23)),
            )
        elif hardcoded:
            self._draw_wrapped_text(
                painter, text_rect, hardcoded, "#b98fd0",
                max(6, int(unit * 0.22)),
            )
        elif spec.kind == "strum":
            self._draw_wrapped_text(
                painter, text_rect, "Strum", text_color,
                max(6, int(unit * 0.21)),
            )
        else:
            self._draw_wrapped_text(
                painter, text_rect, spec.label, text_color,
                max(6, int(unit * 0.23)),
            )

        if chips:
            self._draw_chips(painter, rect, chips)

    def _fixed_suppressed(self, spec: KeySpec) -> bool:
        return (self._layer == "Super" and spec.kind in ("chord", "strum")
                and not self._hardcoded_text(spec.name))

    def _key_colors(self, spec: KeySpec, index: int) -> tuple[str, str, str]:
        if self._fixed_suppressed(spec):
            return _RESERVED_FILL, _FIXED_BORDER, "#737373"
        if spec.kind == "bindable":
            if index >= 0:
                return _BOUND_FILL, _BOUND_BORDER, _BOUND_TEXT
            return _UNBOUND_FILL, _UNBOUND_BORDER, _UNBOUND_TEXT
        if spec.kind == "chord":
            fill = "#2b2b31"
            if spec.chord_quality is not None:
                fill = CHORD_QUALITY_COLORS.get(spec.chord_quality, fill)
            return fill, _FIXED_BORDER, "#9d9d9d"
        if spec.kind == "strum":
            return _STRUM_FILL, _FIXED_BORDER, "#9a9a9a"
        if spec.kind == "reserved":
            return _RESERVED_FILL, _FIXED_BORDER, "#858585"
        return _OTHER_FILL, _FIXED_BORDER, "#707070"

    def _draw_chord_text(self, painter: QPainter, rect: QRectF,
                         spec: KeySpec) -> None:
        unit = self._unit
        quality = ""
        if spec.chord_quality is not None:
            quality = CHORD_QUALITY_LABELS.get(spec.chord_quality, "")
        if quality:
            quality_height = unit * 0.17
            root_rect = QRectF(
                rect.left(), rect.top(), rect.width(), rect.height() - quality_height
            )
            quality_rect = QRectF(
                rect.left(), rect.bottom() - quality_height, rect.width(), quality_height
            )
        else:
            root_rect = rect
            quality_rect = QRectF()
        self._draw_wrapped_text(
            painter, root_rect, spec.chord_root or spec.label, "#a7a7a7",
            max(6, int(unit * 0.23)), bold=True, max_lines=1,
        )
        if quality:
            self._draw_wrapped_text(
                painter, quality_rect, quality, "#777777",
                max(5, int(unit * 0.15)), max_lines=1,
            )

    def _draw_chips(self, painter: QPainter, rect: QRectF,
                    modifiers: list[str]) -> None:
        unit = self._unit
        chip_width = max(8.0, unit * 0.25)
        chip_height = max(7.0, unit * 0.19)
        gap = max(1.0, unit * 0.035)
        margin = max(2.0, unit * 0.05)
        total_width = len(modifiers) * chip_width + (len(modifiers) - 1) * gap
        x = rect.right() - total_width - margin
        y = rect.bottom() - chip_height - margin
        font = self._font(max(5, int(unit * 0.14)), bold=True)
        painter.setFont(font)
        for modifier in modifiers:
            color = MODIFIER_COLORS.get(modifier, "#666666")
            painter.setPen(Qt.NoPen)
            painter.setBrush(QColor(color))
            painter.drawRoundedRect(
                QRectF(x, y, chip_width, chip_height), 2.0, 2.0
            )
            painter.setPen(QColor("#ffffff"))
            painter.drawText(
                QRectF(x, y, chip_width, chip_height),
                Qt.AlignCenter,
                modifier[0],
            )
            x += chip_width + gap

    def _draw_wrapped_text(self, painter: QPainter, rect: QRectF, text: str,
                           color: str, pixel_size: int, *,
                           bold: bool = False, max_lines: int = 2) -> None:
        if not text or rect.width() <= 1 or rect.height() <= 1:
            return
        width = int(rect.width())
        words = text.split() or [text]
        size = pixel_size
        min_size = max(5, int(pixel_size * 0.65))
        while size > min_size:
            font = self._font(size, bold=bold)
            metrics = QFontMetrics(font)
            longest = max(metrics.horizontalAdvance(word) for word in words)
            if longest <= width:
                break
            size -= 1
        font = self._font(size, bold=bold)
        painter.setFont(font)
        metrics = QFontMetrics(font)
        lines = self._wrap_lines(text, metrics, width, max_lines)
        if not lines:
            return
        line_height = metrics.height()
        total_height = line_height * len(lines)
        y = rect.center().y() - total_height / 2.0
        painter.setPen(QColor(color))
        for line in lines:
            painter.drawText(
                QRectF(rect.left(), y, rect.width(), line_height),
                Qt.AlignHCenter | Qt.AlignVCenter,
                line,
            )
            y += line_height

    @staticmethod
    def _wrap_lines(text: str, metrics: QFontMetrics, width: int,
                    max_lines: int) -> list[str]:
        if width <= 0:
            return []
        if metrics.horizontalAdvance(text) <= width:
            return [text]
        words = text.split()
        if not words:
            return []
        lines: list[str] = []
        current = ""
        consumed = 0
        for word in words:
            candidate = word if not current else f"{current} {word}"
            if metrics.horizontalAdvance(candidate) <= width or not current:
                current = candidate
                consumed += 1
            else:
                lines.append(current)
                current = word
                consumed += 1
                if len(lines) >= max_lines:
                    break
        if current and len(lines) < max_lines:
            lines.append(current)
        if not lines:
            return []
        clamped = [
            metrics.elidedText(line, Qt.ElideRight, width)
            if metrics.horizontalAdvance(line) > width else line
            for line in lines
        ]
        if consumed < len(words):
            clamped[-1] = metrics.elidedText(
                clamped[-1] + "\u2026", Qt.ElideRight, width
            )
        return clamped

    def _action_label(self, index: int):
        binding = self._bindings[index]
        return format_action(
            binding.action,
            rhythm_names=self._rhythm_names,
            bass_names=self._bass_names,
            chord_pattern_names=self._chord_pattern_names,
        )

    def _tooltip_for(self, spec: KeySpec) -> str:
        if self._macro_key_active(spec):
            commands = (self._macro_config.keys.get(spec.name, [])
                        if self._macro_config is not None else [])
            where = f" for {self._macro_preset_label}" if self._macro_preset_label else ""
            name = (self._macro_config.name_for(spec.name)
                    if self._macro_config is not None else spec.name)
            lines = [
                f"{spec.label} ({spec.name})",
                f"Per-preset macro{where}: {name} ({len(commands)} commands)",
            ]
            if commands:
                for command in commands:
                    lines.append(
                        "- " + macro_command_label(
                            command,
                            rhythm_names=self._rhythm_names,
                            bass_names=self._bass_names,
                            chord_pattern_names=self._chord_pattern_names,
                        ).detail
                    )
            else:
                lines.append("Empty macro")
            lines.append("Double-click to edit this macro key")
            return "\n".join(lines)

        lines = [f"{spec.label} ({spec.name})", describe_key(spec)]
        if self._fixed_suppressed(spec):
            lines.append("Suppressed while Super is held")
        for index in self._by_key.get(spec.name, []):
            binding = self._bindings[index]
            label = self._action_label(index)
            lines.append(f"{combo_label(binding.modifier, binding.key)}: {label.detail}")
        for (modifier, key), description in HARDCODED_COMBOS.items():
            if key == spec.name:
                lines.append(f"{combo_label(modifier, key)}: {description} (fixed)")
        if spec.kind == "bindable" and not self._by_key.get(spec.name):
            lines.append("Unbound")
        return "\n".join(lines)

    def _key_at(self, position) -> Optional[KeySpec]:
        if self._unit <= 0:
            return None
        x = (position.x() - self._origin.x()) / self._unit
        y = (position.y() - self._origin.y()) / self._unit
        for spec in reversed(ALL_KEYS):
            if spec.x <= x <= spec.x + spec.w and spec.y <= y <= spec.y + spec.h:
                return spec
        return None

    def _binding_for_click(self, key: str) -> int:
        indices = self._by_key.get(key, [])
        if not indices:
            return -1
        if self._layer != LAYER_ALL:
            return self._display_binding(key)
        base = self._by_combo.get((None, key))
        return base if base is not None else indices[0]

    def _choose_binding(self, key: str, global_pos) -> int:
        indices = self._by_key.get(key, [])
        if len(indices) <= 1:
            return indices[0] if indices else -1
        menu = QMenu(self)
        action_to_index: dict = {}
        for index in indices:
            binding = self._bindings[index]
            label = self._action_label(index)
            text = f"{combo_label(binding.modifier, binding.key)}  \u2014  {label.short}"
            action = menu.addAction(text)
            action_to_index[action] = index
        chosen = menu.exec(global_pos)
        return action_to_index.get(chosen, -1)

    def mousePressEvent(self, event) -> None:
        if event.button() != Qt.LeftButton:
            return
        spec = self._key_at(event.position())
        if spec is None:
            return
        if self._macro_key_active(spec):
            self._selected_macro_key = spec.name
            self.set_selected_binding(-1)
            self.macroKeySelected.emit(spec.name)
            self.update()
            return
        if spec.kind != "bindable":
            return
        index = self._binding_for_click(spec.name)
        self.set_selected_binding(index)
        self.bindingSelected.emit(index)

    def mouseDoubleClickEvent(self, event) -> None:
        if event.button() != Qt.LeftButton:
            return
        spec = self._key_at(event.position())
        if spec is None:
            return
        if self._macro_key_active(spec):
            self._selected_macro_key = spec.name
            self.set_selected_binding(-1)
            self.macroKeySelected.emit(spec.name)
            self.macroKeyEditRequested.emit(spec.name)
            self.update()
            return
        if spec.kind != "bindable":
            return
        indices = self._by_key.get(spec.name, [])
        index = self._binding_for_click(spec.name)
        if index < 0:
            modifier = None if self._layer in (LAYER_ALL, LAYER_NONE) else self._layer
            self.unboundKeyActivated.emit(spec.name, modifier)
            return
        if self._layer == LAYER_ALL and len(indices) > 1:
            index = self._choose_binding(
                spec.name, event.globalPosition().toPoint()
            )
        if index >= 0:
            self.set_selected_binding(index)
            self.bindingSelected.emit(index)
            self.bindingEditRequested.emit(index)

    def mouseMoveEvent(self, event) -> None:
        spec = self._key_at(event.position())
        if spec is None:
            if self._hover_key:
                self._hover_key = ""
                self.unsetCursor()
                QToolTip.hideText()
                self.update()
            return
        if spec.name != self._hover_key:
            self._hover_key = spec.name
            self.setCursor(
                Qt.PointingHandCursor
                if spec.kind == "bindable" or self._macro_key_active(spec)
                else Qt.ArrowCursor
            )
            QToolTip.showText(
                event.globalPosition().toPoint(), self._tooltip_for(spec), self
            )
            self.update()

    def leaveEvent(self, event) -> None:
        self._hover_key = ""
        self.unsetCursor()
        QToolTip.hideText()
        self.update()
        super().leaveEvent(event)
