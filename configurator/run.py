"""Run the configurator without installing the package.

Run from the repository (or double-click ``run.bat`` on Windows):

    python run.py

This inserts the ``src/`` directory onto ``sys.path`` so the package can be
imported directly. PySide6 must still be installed (``pip install PySide6``).
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "src"))

from keybchord_configurator.app import main  # noqa: E402

if __name__ == "__main__":
    sys.exit(main())
