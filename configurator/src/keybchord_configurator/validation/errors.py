"""Validation results."""

from __future__ import annotations

from dataclasses import dataclass, field


@dataclass
class ValidationResult:
    """A collection of validation messages.

    ``errors`` are problems the firmware would reject or skip; ``warnings`` are
    things the firmware would silently clamp, ignore, or fall back from.
    """

    errors: list[str] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)

    def ok(self) -> bool:
        return not self.errors

    def merge(self, other: "ValidationResult") -> "ValidationResult":
        self.errors.extend(other.errors)
        self.warnings.extend(other.warnings)
        return self

    def add_error(self, message: str) -> None:
        self.errors.append(message)

    def add_warning(self, message: str) -> None:
        self.warnings.append(message)
