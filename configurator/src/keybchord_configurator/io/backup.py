"""Backup and restore the whole configuration as a zip archive."""

from __future__ import annotations

import zipfile
from pathlib import Path

from .storage import (
    BASS_DIR,
    CONFIG_FILE,
    KEYMAP_FILE,
    PRESETS_DIR,
    RHYTHMS_DIR,
)

CONFIG_PATHS = (CONFIG_FILE, KEYMAP_FILE)
CONFIG_DIRS = (PRESETS_DIR, RHYTHMS_DIR, BASS_DIR)


def _is_config_path(name: str) -> bool:
    parts = Path(name).parts
    if len(parts) == 1 and parts[0] in CONFIG_PATHS:
        return True
    if len(parts) == 2 and parts[0] in CONFIG_DIRS and parts[1].endswith(".json"):
        return True
    return False


def backup_to_zip(directory: str | Path, zip_path: str | Path) -> list[str]:
    """Create a zip of the configuration files. Returns the archived names."""
    root = Path(directory)
    archived: list[str] = []
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for name in CONFIG_PATHS:
            path = root / name
            if path.exists():
                zf.write(path, arcname=name)
                archived.append(name)
        for subdir in CONFIG_DIRS:
            directory_path = root / subdir
            if not directory_path.exists():
                continue
            for path in sorted(directory_path.glob("*.json")):
                arcname = path.relative_to(root).as_posix()
                zf.write(path, arcname=arcname)
                archived.append(arcname)
    return archived


def restore_from_zip(zip_path: str | Path, directory: str | Path) -> list[str]:
    """Extract configuration files from a zip into ``directory``.

    Only recognized configuration files are restored (guarding against path
    traversal). Returns the restored names.
    """
    root = Path(directory)
    restored: list[str] = []
    with zipfile.ZipFile(zip_path, "r") as zf:
        for info in zf.infolist():
            if info.is_dir():
                continue
            name = info.filename.replace("\\", "/")
            if not _is_config_path(name):
                continue
            # Reject path traversal.
            target = (root / name).resolve()
            if not str(target).startswith(str(root.resolve()) + "/") and \
                    target != root.resolve() / name:
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            with zf.open(info) as src, open(target, "wb") as dst:
                dst.write(src.read())
            restored.append(name)
    return restored
