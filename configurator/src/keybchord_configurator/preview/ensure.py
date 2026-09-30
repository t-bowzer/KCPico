"""Prompt to install the optional preview dependencies and restart the app."""

from __future__ import annotations

import os
import subprocess
import sys
import threading
import time
from typing import Optional

from PySide6.QtCore import Qt
from PySide6.QtWidgets import QApplication, QMessageBox, QProgressDialog

from . import synth

PREVIEW_PACKAGES = ["numpy", "sounddevice"]


def ensure_available(parent=None) -> bool:
    """Return True if preview is available; otherwise offer to install.

    If the user agrees, the dependencies are installed and the app is restarted
    (in that case this function does not return normally). Returns False when
    the user declines, installation fails, or this is a frozen standalone build
    that cannot install packages.
    """
    if synth.available():
        return True

    if getattr(sys, "frozen", False):
        QMessageBox.information(
            parent, "Preview unavailable",
            "Audio preview needs the optional 'preview' extra (numpy + "
            "sounddevice), which isn't bundled in this standalone build.\n\n"
            "Run the source install (pip install -e \".[preview]\") to enable it.")
        return False

    ret = QMessageBox.question(
        parent, "Install preview support",
        "Audio preview needs the optional 'preview' extra "
        "(numpy + sounddevice).\n\nInstall it now and restart the app?",
        QMessageBox.Yes | QMessageBox.No, QMessageBox.Yes)
    if ret != QMessageBox.Yes:
        return False

    if not _install(parent):
        return False

    _restart()
    return False  # reached on Windows after quit(); never on POSIX


def _install(parent: Optional[object]) -> bool:
    result: dict = {}
    done = threading.Event()

    def worker() -> None:
        cmd = [sys.executable, "-m", "pip", "install", *PREVIEW_PACKAGES]
        try:
            proc = subprocess.run(cmd, capture_output=True, text=True, timeout=900)
            result["returncode"] = proc.returncode
            result["output"] = (proc.stderr or proc.stdout or "Unknown error").strip()
        except Exception as exc:  # noqa: BLE001
            result["error"] = str(exc)
        finally:
            done.set()

    progress = QProgressDialog(
        f"Installing {' + '.join(PREVIEW_PACKAGES)}...", None, 0, 0, parent)
    progress.setWindowTitle("Installing")
    progress.setWindowModality(Qt.WindowModal)
    progress.setCancelButton(None)
    progress.setMinimumDuration(0)
    progress.setRange(0, 0)
    progress.show()

    thread = threading.Thread(target=worker, daemon=True)
    thread.start()

    app = QApplication.instance()
    while not done.is_set():
        if app is not None:
            app.processEvents()
        time.sleep(0.05)
    progress.close()

    if "error" in result:
        QMessageBox.warning(parent, "Install failed", result["error"])
        return False
    if result.get("returncode") != 0:
        QMessageBox.warning(parent, "Install failed",
                            result.get("output", "Unknown error"))
        return False
    return True


def _restart() -> None:
    frozen = getattr(sys, "frozen", False)
    args = [sys.executable] + (sys.argv[1:] if frozen else sys.argv)
    if os.name == "nt":
        subprocess.Popen(args)
        QApplication.instance().quit()
    else:
        os.execv(sys.executable, args)
