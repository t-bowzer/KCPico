#pragma once

#include <string>
#include <vector>

#include "keymap.h"


class StorageAdapter;

struct KeyBinding {
    uint8_t   modifierMask = 0;  // KMOD_* bits (0 = no modifier)
    uint8_t   usage        = 0;  // non-modifier HID usage
    KeyAction action;
};

struct KeymapLoadResult;

// A data-driven keymap loaded from /keymap.json at boot (falling back to the
// built-in defaults when the file is missing). Only non-reserved keys (and
// Super/Ctrl/Alt combos on non-reserved keys) may be bound; chord/strum keys,
// backtick, Esc, the arrow keys, Enter/Backspace and the Super+1..8 preset-load
// + Super+Esc panic combos are reserved and hardcoded in the resolver.
class KeymapConfig {
public:
    std::vector<KeyBinding> bindings;

    static KeymapConfig defaults();
    static KeymapLoadResult load(StorageAdapter& storage);

    const KeyAction* find(uint8_t modifierMask, uint8_t usage) const;
};

struct KeymapLoadResult {
    bool ok = true;
    KeymapConfig config;              // parsed config (or defaults() on error/missing)
    std::vector<std::string> errors;  // populated when ok == false
};

// The canonical default keymap, as JSON (written to /keymap.json on first boot
// by provisionDefaults). Kept in sync with KeymapConfig::defaults().
std::string keymapDefaultJson();
