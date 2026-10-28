#pragma once

// Firmware release version. Keep this in X.Y.Z form: it is shown on the LCD.
#ifndef KEYBCHORD_FW_VERSION
#define KEYBCHORD_FW_VERSION "0.2.0"
#endif

// The PlatformIO build supplies the first 8 characters of the current git
// commit via scripts/git_version.py. Fall back to "unknown" for source archives
// or builds without git.
#ifndef KEYBCHORD_GIT_HASH
#define KEYBCHORD_GIT_HASH "unknown"
#endif

namespace keybchord {

inline constexpr const char* kFirmwareVersion = KEYBCHORD_FW_VERSION;
inline constexpr const char* kGitHash = KEYBCHORD_GIT_HASH;

}  // namespace keybchord
