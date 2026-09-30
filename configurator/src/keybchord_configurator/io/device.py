"""Detect and eject the "KeybChord" mass-storage volume.

Uses only the standard library: ``ctypes`` on Windows, ``subprocess`` on macOS
and Linux. The device name ("KeybChord") matches the firmware's USB Mass Storage
volume label.
"""

from __future__ import annotations

import os
import string
import subprocess
import sys
from pathlib import Path
from typing import Optional

VOLUME_NAME = "KeybChord"

_IS_WINDOWS = sys.platform == "win32"
_IS_MAC = sys.platform == "darwin"


def _matches_volume(name: str) -> bool:
    """Case-insensitive match against the firmware's volume label.

    FAT labels are conventionally displayed uppercase by Windows (``KEYBCHORD``)
    even though the firmware writes ``KeybChord``, so we can't compare exactly.
    """
    return name.strip().casefold() == VOLUME_NAME.casefold()


def _windows_volume_label(drive_letter: str) -> Optional[str]:
    import ctypes

    root = f"{drive_letter}:\\"
    volume_name = ctypes.create_unicode_buffer(261)
    kernel32 = ctypes.windll.kernel32
    result = kernel32.GetVolumeInformationW(
        ctypes.c_wchar_p(root),
        volume_name,
        len(volume_name),
        None, None, None, None, 0,
    )
    if not result:
        return None
    return volume_name.value


def find_device() -> Optional[str]:
    """Return the mount path of the KeybChord volume, or ``None``."""
    if _IS_WINDOWS:
        for letter in string.ascii_uppercase:
            path = f"{letter}:\\"
            try:
                if not os.path.exists(path):
                    continue
                label = _windows_volume_label(letter)
                if label and _matches_volume(label):
                    return path
            except OSError:
                continue
        return None

    # macOS / Linux: scan well-known mount roots, matching case-insensitively.
    search_roots: list[str] = []
    if _IS_MAC:
        search_roots.append("/Volumes")
    else:
        user = os.environ.get("USER", "")
        search_roots.extend([
            f"/media/{user}",
            "/media",
            f"/run/media/{user}",
            "/run/media",
            "/mnt",
        ])
    for root in search_roots:
        try:
            entries = os.listdir(root)
        except OSError:
            continue
        for entry in entries:
            if _matches_volume(entry):
                candidate = os.path.join(root, entry)
                if os.path.isdir(candidate):
                    return candidate
    return None


def _linux_block_device(mountpoint: str) -> Optional[str]:
    """Map a mount point to a block device via /proc/self/mountinfo."""
    try:
        with open("/proc/self/mountinfo", "r", encoding="utf-8") as fh:
            for line in fh:
                parts = line.split()
                if len(parts) >= 5 and parts[4] == mountpoint:
                    # parts[3] is the "major:minor"; parts[9] is the mount source.
                    return parts[9] if len(parts) > 9 else None
    except OSError:
        return None
    return None


def eject(path: str) -> tuple[bool, str]:
    """Eject/unmount a volume. Returns (ok, message)."""
    if not os.path.exists(path):
        return False, f"{path} does not exist"

    if _IS_WINDOWS:
        return _eject_windows(path)

    if _IS_MAC:
        return _run_eject(["diskutil", "eject", path])

    # Linux: prefer udisksctl, fall back to umount.
    device = _linux_block_device(path)
    if device:
        ok, msg = _run_eject(["udisksctl", "unmount", "-b", device])
        if ok:
            return True, msg
    return _run_eject(["umount", path])


def _eject_windows(path: str) -> tuple[bool, str]:
    import ctypes
    from ctypes import wintypes

    GENERIC_READ = 0x80000000
    GENERIC_WRITE = 0x40000000
    FILE_SHARE_READ = 0x00000001
    FILE_SHARE_WRITE = 0x00000002
    OPEN_EXISTING = 3
    IOCTL_STORAGE_EJECT_MEDIA = 0x002D4808

    drive = path[:2]  # e.g. "D:"
    handle = ctypes.windll.kernel32.CreateFileW(
        ctypes.c_wchar_p(f"\\\\.\\{drive}"),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        None,
        OPEN_EXISTING,
        0,
        None,
    )
    if handle == -1 or handle == 0xFFFFFFFFFFFFFFFF:
        return False, f"Could not open {drive} for eject"

    try:
        bytes_returned = wintypes.DWORD(0)
        result = ctypes.windll.kernel32.DeviceIoControl(
            handle,
            IOCTL_STORAGE_EJECT_MEDIA,
            None, 0,
            None, 0,
            ctypes.byref(bytes_returned),
            None,
        )
        ctypes.windll.kernel32.CloseHandle(handle)
        if result == 0:
            return False, f"Failed to eject {drive}"
        return True, f"Ejected {drive}"
    finally:
        ctypes.windll.kernel32.CloseHandle(handle)


def _run_eject(command: list[str]) -> tuple[bool, str]:
    try:
        proc = subprocess.run(command, capture_output=True, text=True, timeout=30)
    except FileNotFoundError:
        return False, f"Command not found: {command[0]}"
    except subprocess.TimeoutExpired:
        return False, f"Eject timed out: {' '.join(command)}"
    if proc.returncode == 0:
        return True, proc.stdout.strip() or "Ejected"
    return False, (proc.stderr or proc.stdout or "Eject failed").strip()


def is_writeable_directory(path: str | Path) -> tuple[bool, str]:
    p = Path(path)
    if not p.exists():
        return False, f"{path} does not exist"
    if not p.is_dir():
        return False, f"{path} is not a directory"
    if not os.access(p, os.W_OK):
        return False, f"{path} is not writable"
    return True, ""
