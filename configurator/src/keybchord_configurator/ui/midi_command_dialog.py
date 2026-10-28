"""Structured editor for a raw MIDI macro command."""

from __future__ import annotations

from typing import Optional

from PySide6.QtWidgets import (
    QComboBox,
    QDialog,
    QDialogButtonBox,
    QFormLayout,
    QLabel,
    QLineEdit,
    QSpinBox,
    QVBoxLayout,
    QWidget,
)

from ..models.macro import MacroCommand
from ..models.midi import (
    MAX_MIDI_DELAY_MS,
    MidiMessageSpec,
    decode_midi_bytes,
    format_hex_bytes,
    parse_hex_bytes,
)

_KIND_LABELS: tuple[tuple[str, str], ...] = (
    ("Note On", "note_on"),
    ("Note Off", "note_off"),
    ("Control Change", "cc"),
    ("Program Change", "program_change"),
    ("Pitch Bend", "pitch_bend"),
    ("Channel Pressure", "channel_pressure"),
    ("Poly Pressure", "poly_pressure"),
    ("SysEx", "sysex"),
    ("Raw MIDI bytes", "raw"),
)


class MidiCommandDialog(QDialog):
    def __init__(
        self,
        parent: Optional[QWidget] = None,
        *,
        command: Optional[MacroCommand] = None,
    ) -> None:
        super().__init__(parent)
        self.setWindowTitle("Add MIDI command" if command is None
                            else "Edit MIDI command")
        self.setMinimumWidth(440)
        self._build()
        if command is not None and command.bytes:
            self._populate(decode_midi_bytes(command.bytes), command.delay_ms)
        self._sync_fields()

    def _build(self) -> None:
        layout = QVBoxLayout(self)
        form = QFormLayout()

        self.kind_combo = QComboBox()
        for label, kind in _KIND_LABELS:
            self.kind_combo.addItem(label, kind)
        self.kind_combo.currentIndexChanged.connect(self._sync_fields)

        self.channel_spin = QSpinBox()
        self.channel_spin.setRange(1, 16)
        self.channel_spin.setValue(1)

        self.number_spin = QSpinBox()
        self.number_spin.setRange(0, 127)

        self.value_spin = QSpinBox()
        self.value_spin.setRange(0, 127)

        self.bend_spin = QSpinBox()
        self.bend_spin.setRange(-8192, 8191)

        self.bytes_edit = QLineEdit()
        self.bytes_edit.setPlaceholderText("F0 41 10 42 12 ... F7")
        self.bytes_edit.textChanged.connect(self._update_preview)

        self.delay_spin = QSpinBox()
        self.delay_spin.setRange(0, MAX_MIDI_DELAY_MS)
        self.delay_spin.setSingleStep(10)
        self.delay_spin.setSuffix(" ms")

        form.addRow("Message", self.kind_combo)
        form.addRow("Channel", self.channel_spin)
        form.addRow("Data 1", self.number_spin)
        form.addRow("Data 2", self.value_spin)
        form.addRow("Bend", self.bend_spin)
        form.addRow("Bytes", self.bytes_edit)
        form.addRow("Delay", self.delay_spin)
        layout.addLayout(form)

        self._channel_label = form.labelForField(self.channel_spin)
        self._number_label = form.labelForField(self.number_spin)
        self._value_label = form.labelForField(self.value_spin)
        self._bend_label = form.labelForField(self.bend_spin)
        self._bytes_label = form.labelForField(self.bytes_edit)

        self.preview = QLabel("")
        self.preview.setWordWrap(True)
        layout.addWidget(self.preview)

        self.buttons = QDialogButtonBox(
            QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        self.buttons.accepted.connect(self.accept)
        self.buttons.rejected.connect(self.reject)
        layout.addWidget(self.buttons)

        for widget in (self.channel_spin, self.number_spin, self.value_spin,
                       self.bend_spin, self.delay_spin):
            widget.valueChanged.connect(self._update_preview)

    def _kind(self) -> str:
        return self.kind_combo.currentData()

    def _set_visible(self, widget: QWidget, label: Optional[QLabel],
                     visible: bool) -> None:
        widget.setVisible(visible)
        if label is not None:
            label.setVisible(visible)

    def _sync_fields(self) -> None:
        kind = self._kind()
        is_channel = kind not in ("sysex", "raw")
        has_number = kind in (
            "note_on", "note_off", "cc", "program_change", "poly_pressure")
        has_value = kind in (
            "note_on", "note_off", "cc", "channel_pressure", "poly_pressure")
        has_bend = kind == "pitch_bend"
        has_bytes = kind in ("sysex", "raw")

        self._set_visible(self.channel_spin, self._channel_label, is_channel)
        self._set_visible(self.number_spin, self._number_label, has_number)
        self._set_visible(self.value_spin, self._value_label, has_value)
        self._set_visible(self.bend_spin, self._bend_label, has_bend)
        self._set_visible(self.bytes_edit, self._bytes_label, has_bytes)

        if kind == "cc":
            self._number_label.setText("Controller")
            self._value_label.setText("Value")
        elif kind in ("note_on", "note_off", "poly_pressure"):
            self._number_label.setText("Note")
            self._value_label.setText(
                "Velocity" if kind != "poly_pressure" else "Pressure")
        elif kind == "program_change":
            self._number_label.setText("Program")
        elif kind == "channel_pressure":
            self._value_label.setText("Pressure")
        self._update_preview()

    def _spec(self) -> MidiMessageSpec:
        kind = self._kind()
        raw_bytes: list[int] = []
        if kind in ("sysex", "raw"):
            try:
                raw_bytes = parse_hex_bytes(self.bytes_edit.text())
            except ValueError:
                raw_bytes = []
        return MidiMessageSpec(
            kind=kind,
            channel=self.channel_spin.value(),
            number=self.number_spin.value(),
            value=self.value_spin.value(),
            bend=self.bend_spin.value(),
            bytes=raw_bytes,
            delay_ms=self.delay_spin.value(),
        )

    def _update_preview(self, *_args) -> None:
        spec = self._spec()
        encoded = spec.encoded()
        ok = bool(encoded)
        if self._kind() in ("sysex", "raw"):
            try:
                parse_hex_bytes(self.bytes_edit.text())
            except ValueError:
                ok = False
        if ok:
            self.preview.setText(
                f"{format_hex_bytes(encoded)}\n{spec.summary()}")
            self.preview.setStyleSheet("")
        else:
            self.preview.setText("Enter valid hexadecimal MIDI bytes.")
            self.preview.setStyleSheet("color: #e05353;")
        self.buttons.button(QDialogButtonBox.Ok).setEnabled(ok)

    def _populate(self, spec: MidiMessageSpec, delay_ms: int) -> None:
        idx = self.kind_combo.findData(spec.kind)
        if idx >= 0:
            self.kind_combo.setCurrentIndex(idx)
        self.channel_spin.setValue(spec.channel)
        self.number_spin.setValue(spec.number)
        self.value_spin.setValue(spec.value)
        self.bend_spin.setValue(spec.bend)
        self.bytes_edit.setText(format_hex_bytes(spec.bytes))
        self.delay_spin.setValue(delay_ms)

    def result_command(self) -> MacroCommand:
        spec = self._spec()
        return MacroCommand(type="midi", bytes=spec.encoded(),
                            delay_ms=self.delay_spin.value())
