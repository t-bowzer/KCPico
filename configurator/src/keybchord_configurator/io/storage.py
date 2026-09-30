"""On-disk layout, loading, and saving.

The filesystem layout is (see ``docs/custom-configuration-guide.md``):

* ``config.json`` — global settings (object)
* ``keymap.json`` — key bindings (``{"bindings": [...]}``)
* ``presets/bank1.json`` … ``bank10.json`` — arrays of 8 slots
* ``rhythms/*.json`` — rhythm patterns (12 built-ins + user files)
* ``bass/*.json`` — bass patterns (9 built-ins + user files)
"""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

from ..models.bass import BASS_FILES, BASS_NAMES, BassPattern
from ..models.config import AppConfig
from ..models.defaults import BUILTIN_BASS, BUILTIN_RHYTHMS
from ..models.keymap import KeymapConfig, default_keymap
from ..models.preset import PresetSlot
from ..models.rhythm import RHYTHM_FILES, RHYTHM_NAMES, RhythmPattern

CONFIG_FILE = "config.json"
KEYMAP_FILE = "keymap.json"
PRESETS_DIR = "presets"
RHYTHMS_DIR = "rhythms"
BASS_DIR = "bass"


def _write_json(path: Path, data) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    text = json.dumps(data, indent=2, ensure_ascii=False)
    path.write_text(text, encoding="utf-8")


def read_json_file(path: Path):
    """Read and parse a JSON file; raises on missing/invalid JSON."""
    with open(path, "r", encoding="utf-8") as fh:
        return json.load(fh)


def _load_optional_json(path: Path, fallback):
    if not path.exists():
        return fallback
    try:
        return read_json_file(path)
    except (json.JSONDecodeError, OSError, ValueError):
        return fallback


@dataclass
class ConfigTree:
    """The complete in-memory configuration."""

    config: AppConfig = field(default_factory=AppConfig.defaults)
    keymap: KeymapConfig = field(default_factory=default_keymap)
    banks: list[list[PresetSlot]] = field(default_factory=list)
    rhythms: list[tuple[str, RhythmPattern]] = field(default_factory=list)
    bass: list[tuple[str, BassPattern]] = field(default_factory=list)

    def __post_init__(self) -> None:
        if not self.banks:
            self.banks = [
                [PresetSlot.defaults() for _ in range(8)] for _ in range(10)
            ]

    # --- name lists (for pattern resolution) ------------------------------
    def rhythm_names(self) -> list[str]:
        return [entry_name(filename, pattern.name) for filename, pattern in self.rhythms]

    def bass_names(self) -> list[str]:
        return [entry_name(filename, pattern.name) for filename, pattern in self.bass]

    def rhythm_name(self, pattern_name: str) -> Optional[str]:
        for filename, pattern in self.rhythms:
            if entry_name(filename, pattern.name) == pattern_name:
                return entry_name(filename, pattern.name)
        return None


def entry_name(filename: str, name: str) -> str:
    """Runtime pattern name (authored ``name`` or filename fallback)."""
    if name:
        return name
    return Path(filename).stem


def load_tree(directory: str | Path) -> ConfigTree:
    """Load a configuration directory into a ConfigTree.

    Missing or malformed files fall back to defaults (mirroring the firmware's
    lenient loaders).
    """
    root = Path(directory)
    tree = ConfigTree()

    config_data = _load_optional_json(root / CONFIG_FILE, {})
    tree.config = AppConfig.from_dict(config_data)

    keymap_data = _load_optional_json(root / KEYMAP_FILE, None)
    if isinstance(keymap_data, dict):
        tree.keymap = KeymapConfig.from_dict(keymap_data)
    else:
        tree.keymap = default_keymap()

    tree.banks = load_presets(root)
    tree.rhythms = load_rhythms(root)
    tree.bass = load_bass(root)
    return tree


def default_tree() -> ConfigTree:
    """A fresh, fully-provisioned default configuration (mirrors first boot)."""
    tree = ConfigTree()
    tree.config = AppConfig.defaults()
    tree.keymap = default_keymap()
    tree.banks = [
        [PresetSlot.defaults() for _ in range(8)] for _ in range(10)
    ]
    tree.rhythms = [
        (RHYTHM_FILES[i], RhythmPattern.from_dict(BUILTIN_RHYTHMS[i]))
        for i in range(len(BUILTIN_RHYTHMS))
    ]
    tree.bass = [
        (BASS_FILES[i], BassPattern.from_dict(BUILTIN_BASS[i]))
        for i in range(len(BUILTIN_BASS))
    ]
    return tree


def load_presets(root: Path) -> list[list[PresetSlot]]:
    banks: list[list[PresetSlot]] = []
    presets_dir = root / PRESETS_DIR
    for bank in range(1, 11):
        path = presets_dir / f"bank{bank}.json"
        data = _load_optional_json(path, None)
        slots: list[PresetSlot] = []
        if isinstance(data, list):
            for i in range(8):
                slots.append(PresetSlot.from_dict(data[i] if i < len(data) else {}))
        else:
            slots = [PresetSlot.defaults() for _ in range(8)]
        banks.append(slots)
    return banks


def _pattern_files(root: Path, subdir: str, builtin_files: tuple[str, ...]) -> list[str]:
    """Ordered filenames: built-ins first (fixed order), then user files
    alphabetically — matching the firmware's load order."""
    directory = root / subdir
    if not directory.exists():
        return []
    existing = {p.name for p in directory.glob("*.json")}
    ordered: list[str] = []
    for name in builtin_files:
        if name in existing:
            ordered.append(name)
    user_files = sorted(name for name in existing if name not in builtin_files)
    ordered.extend(user_files)
    return ordered


def load_rhythms(root: Path) -> list[tuple[str, RhythmPattern]]:
    out: list[tuple[str, RhythmPattern]] = []
    for filename in _pattern_files(root, RHYTHMS_DIR, RHYTHM_FILES):
        path = root / RHYTHMS_DIR / filename
        try:
            data = read_json_file(path)
        except (json.JSONDecodeError, OSError, ValueError):
            continue
        pattern = RhythmPattern.from_dict(data)
        out.append((filename, pattern))
    return out


def load_bass(root: Path) -> list[tuple[str, BassPattern]]:
    out: list[tuple[str, BassPattern]] = []
    for filename in _pattern_files(root, BASS_DIR, BASS_FILES):
        path = root / BASS_DIR / filename
        try:
            data = read_json_file(path)
        except (json.JSONDecodeError, OSError, ValueError):
            continue
        pattern = BassPattern.from_dict(data)
        if pattern.valid():
            out.append((filename, pattern))
    return out


def save_tree(tree: ConfigTree, directory: str | Path) -> None:
    """Write the whole tree to a directory."""
    root = Path(directory)
    _write_json(root / CONFIG_FILE, tree.config.to_dict())
    _write_json(root / KEYMAP_FILE, tree.keymap.to_dict())

    for bank in range(10):
        _write_json(root / PRESETS_DIR / f"bank{bank + 1}.json",
                    [slot.to_dict() for slot in tree.banks[bank]])

    for filename, pattern in tree.rhythms:
        _write_json(root / RHYTHMS_DIR / filename, pattern.to_dict())

    for filename, pattern in tree.bass:
        _write_json(root / BASS_DIR / filename, pattern.to_dict())


def delete_user_patterns(directory: str | Path, subdir: str,
                         builtin_files: tuple[str, ...]) -> list[str]:
    """Remove user pattern files (not built-ins); returns removed names."""
    root = Path(directory)
    directory_path = root / subdir
    if not directory_path.exists():
        return []
    removed = []
    for p in directory_path.glob("*.json"):
        if p.name not in builtin_files:
            p.unlink()
            removed.append(p.name)
    return removed
