#pragma once

class StorageAdapter;

// First-boot self-provisioning (NFR-9): writes the shipped default config,
// 10 banks x 8 presets, and the 12 named rhythms to storage. Called when the
// backing filesystem is empty. Pure logic — unit-testable via StorageStub.
void provisionDefaults(StorageAdapter& storage);
