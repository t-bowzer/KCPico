#pragma once

#include <cstdint>
#include "params.h"

// Human-readable display strings for the LCD (spec 4.6 / 6.5). Pure logic, no
// hardware includes, unit-testable on the native host.

// Short play-mode name for the idle line-2 / edit screens (FR-D1/FR-D2).
const char* playModeShort(PlayMode mode);

// Short voicing-mode name.
const char* voicingModeName(VoicingMode mode);

// 2-letter rhythm short code for the idle line-2 (draft table, data-sourced
// for post-hardware tuning). Out-of-range falls back to index 0.
const char* rhythmShortCode(int index);
