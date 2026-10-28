Import("env")

import subprocess


def git_short_hash() -> str:
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "--short=8", "HEAD"],
            cwd=env.subst("$PROJECT_DIR"),
            stderr=subprocess.DEVNULL,
            text=True,
        ).strip() or "unknown"
    except (OSError, subprocess.CalledProcessError):
        return "unknown"


env.Append(CPPDEFINES=[
    ("KEYBCHORD_GIT_HASH", env.StringifyMacro(git_short_hash())),
])
