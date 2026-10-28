#pragma once

#include <string>
#include <vector>

#include "hid_usage.h"
#include "keymap.h"


class StorageAdapter;

struct KeyBinding {
    uint8_t  modifierMask = 0;  // KMOD_* bits (0 = no modifier)
    uint16_t usage        = 0;  // non-modifier HID usage
    KeyAction action;
    uint8_t  usagePage    = hid_usage::PAGE_KEYBOARD;
};

struct KeymapLoadResult;

// A data-driven keymap loaded from /keymap.json at boot (falling back to the
// built-in defaults when the file is missing). Only non-reserved keys (and
// Super/Ctrl/Alt combos on non-reserved keyboard keys) may be bound; chord/
// strum keys, backtick, Esc, Enter/Backspace, the fixed volume keys and the
// Super+1..8 preset-load + Super+Esc panic and Ctrl+Esc firmware-info combos are
// reserved and hardcoded in the resolver. Consumer/media keys are bound by
// (usage page, usage) and cannot use modifiers.
class KeymapConfig {
public:
    std::vector<KeyBinding> bindings;

    static KeymapConfig defaults();
    static KeymapLoadResult load(StorageAdapter& storage);

    const KeyAction* find(uint8_t modifierMask, uint8_t usagePage,
                          uint16_t usage) const;

    // Backwards-compatible keyboard-page lookup.
    const KeyAction* find(uint8_t modifierMask, uint16_t usage) const {
        return find(modifierMask, hid_usage::PAGE_KEYBOARD, usage);
    }
};

struct KeymapLoadResult {
    bool ok = true;
    KeymapConfig config;              // parsed config (or defaults() on error/missing)
    std::vector<std::string> errors;  // populated when ok == false
};

// The canonical default keymap, as JSON (written to /keymap.json on first boot
// by provisionDefaults). Kept in sync with KeymapConfig::defaults().
std::string keymapDefaultJson();

// Shared parameter-name/value parsing (also used by per-preset macros so the
// keymap and macros accept identical spellings).
bool keymapLookupParamName(const std::string& name, ParamId& id);
const char* keymapParamName(ParamId id);
bool keymapParseParamValue(ParamId id, const std::string& value, int& out);
