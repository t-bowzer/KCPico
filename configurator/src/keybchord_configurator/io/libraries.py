"""Discovery and loading of MIDI command libraries."""

from __future__ import annotations

import json
import os
import sys
from pathlib import Path
from typing import Iterable

from ..models.library import MidiLibrary


def builtin_library_dir() -> Path:
    return Path(__file__).resolve().parents[1] / "libraries"


def user_library_dirs() -> list[Path]:
    dirs: list[Path] = []
    env = os.environ.get("KEYBCHORD_LIBRARIES", "")
    for entry in env.split(os.pathsep):
        if entry.strip():
            dirs.append(Path(entry).expanduser())

    if sys.platform.startswith("win"):
        base = Path(os.environ.get("APPDATA", Path.home() / "AppData" / "Roaming"))
        dirs.append(base / "KeybChord" / "libraries")
    elif sys.platform == "darwin":
        dirs.append(Path.home() / "Library" / "Application Support" /
                    "KeybChord" / "libraries")
    else:
        base = Path(os.environ.get("XDG_DATA_HOME",
                                   Path.home() / ".local" / "share"))
        dirs.append(base / "keybchord" / "libraries")

    cwd = Path.cwd()
    dirs.append(cwd / "libraries")
    dirs.append(cwd / "configurator" / "libraries")
    return dirs


def _library_files(directory: Path) -> list[Path]:
    if not directory.exists() or not directory.is_dir():
        return []
    return sorted(directory.glob("*.json"))


def _load_file(path: Path) -> tuple[MidiLibrary | None, str | None]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError, ValueError) as exc:
        return None, f"{path.name}: {exc}"
    library = MidiLibrary.from_dict(data)
    if library is None:
        return None, f"{path.name}: no valid commands found"
    return library, None


def load_libraries(extra_dirs: Iterable[Path] | None = None) -> tuple[list[MidiLibrary], list[str]]:
    paths: list[Path] = []
    paths.extend(_library_files(builtin_library_dir()))
    for directory in user_library_dirs():
        paths.extend(_library_files(directory))
    if extra_dirs:
        for directory in extra_dirs:
            paths.extend(_library_files(Path(directory)))

    seen_paths: set[Path] = set()
    libraries: dict[str, MidiLibrary] = {}
    errors: list[str] = []
    for path in paths:
        try:
            resolved = path.resolve()
        except OSError:
            resolved = path
        if resolved in seen_paths:
            continue
        seen_paths.add(resolved)
        library, error = _load_file(path)
        if error:
            errors.append(error)
            continue
        if library is not None:
            libraries[library.name] = library
    return list(libraries.values()), errors
