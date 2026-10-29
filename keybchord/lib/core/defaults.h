#pragma once

#include <cstddef>

class StorageAdapter;

// First-boot self-provisioning (NFR-9): writes the shipped default config,
// 5 banks x 8 presets, the 12 named rhythms, and the /sequences directory to
// storage. Called when the backing filesystem is empty. Pure logic —
// unit-testable via StorageStub.
void provisionDefaults(StorageAdapter& storage);

// v0.2.0 reduces the preset count from 10 banks to 5 (80 -> 40 presets). Remove
// unsupported legacy bank and sequence files so their flash space is reclaimed.
// Returns the number of files removed.
int migrateLegacyPresetBanks(StorageAdapter& storage);

// JSON for built-in rhythm `index` (0..RHYTHM_COUNT-1), or nullptr out of range.
// Used to self-heal a missing/corrupt built-in rhythm file on load.
const char* defaultRhythmJson(int index);

// JSON for built-in bass pattern `index` (0..BASS_COUNT-1), or nullptr out of
// range. Used to self-heal a missing/corrupt built-in bass file on load.
const char* defaultBassJson(int index);

// JSON for built-in chord pattern `index` (0..CHORD_PATTERN_COUNT-1), or nullptr
// out of range. Used to self-heal a missing/corrupt built-in chord pattern file.
const char* defaultChordPatternJson(int index);
