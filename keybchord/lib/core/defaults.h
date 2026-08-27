#pragma once

#include <cstddef>

class StorageAdapter;

// First-boot self-provisioning (NFR-9): writes the shipped default config,
// 10 banks x 8 presets, and the 12 named rhythms to storage. Called when the
// backing filesystem is empty. Pure logic — unit-testable via StorageStub.
void provisionDefaults(StorageAdapter& storage);

// JSON for built-in rhythm `index` (0..RHYTHM_COUNT-1), or nullptr out of range.
// Used to self-heal a missing/corrupt built-in rhythm file on load.
const char* defaultRhythmJson(int index);
