"""A modal "please wait" progress dialog for blocking I/O.

Filesystem/device operations (USB mass storage, volume enumeration, eject) can
take seconds and would otherwise freeze the UI. ``run_with_progress`` runs a
sequence of blocking callables in a background thread while showing a modal,
indeterminate progress dialog whose label updates between steps.
"""

from __future__ import annotations

import threading
import time
from typing import Callable, Optional

from PySide6.QtCore import Qt
from PySide6.QtWidgets import QApplication, QProgressDialog


def run_with_progress(parent, steps: list[tuple[str, Callable]]) -> list:
    """Run a sequence of ``(label, callable)`` steps with a busy dialog.

    Each callable runs on a background thread in order; its return value is
    appended to the result list. The dialog label is updated to each step's
    text before it runs. Raises the first exception a step raises.

    Returns the list of step results.
    """
    state = {"label": "", "error": None, "results": [], "done": False}

    def worker() -> None:
        try:
            for label, fn in steps:
                state["label"] = label
                state["results"].append(fn())
        except Exception as exc:  # noqa: BLE001
            state["error"] = exc
        finally:
            state["done"] = True

    progress = QProgressDialog("", None, 0, 0, parent)
    progress.setWindowTitle("Please wait")
    progress.setWindowModality(Qt.WindowModal)
    progress.setMinimumDuration(0)
    progress.setRange(0, 0)
    progress.setCancelButton(None)
    progress.show()

    thread = threading.Thread(target=worker, daemon=True)
    thread.start()

    app = QApplication.instance()
    last_label = ""
    while not state["done"]:
        if state["label"] != last_label:
            progress.setLabelText(state["label"])
            last_label = state["label"]
        if app is not None:
            app.processEvents()
        time.sleep(0.02)

    progress.close()
    if state["error"] is not None:
        raise state["error"]
    return state["results"]
