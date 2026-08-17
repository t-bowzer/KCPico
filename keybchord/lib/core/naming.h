#pragma once

#include <cstdint>
#include <string>
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

// Flat-spelling MIDI note name (pitch class + octave), e.g. "C4", "F#3".
std::string noteName(uint8_t note);

// Human-readable name for common CC numbers, else "CC<n>".
std::string ccName(uint8_t cc);

// Human-readable MIDI message type for a status byte (channel-agnostic).
const char* messageTypeName(uint8_t status);
