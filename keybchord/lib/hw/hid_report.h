#pragma once

#include <cstdint>
#include <vector>
#include "base.h"

// Shared HID boot-protocol keyboard report parser.
//
// Used by the CH559 UART-bridge `InputCh559` adapter so the report -> KeyEvent
// logic lives in exactly one place. Pure C++ (no Arduino), so it is
// unit-testable under KEYBCHORD_NATIVE.

namespace hid_report {

constexpr size_t MAX_KEYS = 6;

inline uint8_t modifierBitToUsage(int bit) {
    static const uint8_t map[8] = {
        0xE1, 0xE2, 0xE3, 0xE4,
        0xE5, 0xE6, 0xE7, 0xE8
    };
    return map[bit];
}

// Stateful diffing parser for the standard 8-byte boot-format keyboard report
// ([modifier, reserved, key0..key5]). `report_id` is the keyboard input report
// ID: 0 means no prefix (boot protocol); a non-zero value causes a leading byte
// equal to that ID to be skipped (report-protocol keyboards). Modifier and key
// transitions are appended to `out` as KeyEvents; `prev_*` state is updated in
// place. Returns false if the report is too short to parse.
class BootReportParser {
public:
    void reset() {
        prev_mods_ = 0;
        for (size_t i = 0; i < MAX_KEYS; i++) prev_keys_[i] = 0;
    }

    bool apply(const uint8_t* report, uint16_t len, uint8_t report_id,
               uint64_t received_us, std::vector<KeyEvent>& out) {
        uint8_t off = 0;
        if (report_id != 0 && len > 0 && report[0] == report_id) {
            off = 1;
        }
        if (len < off + 8) return false;

        uint8_t modifiers = report[off];

        uint8_t mod_changed = modifiers ^ prev_mods_;
        for (int bit = 0; bit < 8; bit++) {
            if (mod_changed & (1 << bit)) {
                bool pressed = modifiers & (1 << bit);
                out.push_back({modifierBitToUsage(bit), pressed, modifiers,
                               received_us});
            }
        }
        prev_mods_ = modifiers;

        uint8_t current[MAX_KEYS];
        for (size_t i = 0; i < MAX_KEYS; i++) {
            current[i] = report[off + 2 + i];
        }

        for (size_t i = 0; i < MAX_KEYS; i++) {
            if (current[i] == 0) continue;
            bool found = false;
            for (size_t j = 0; j < MAX_KEYS; j++) {
                if (prev_keys_[j] == current[i]) { found = true; break; }
            }
            if (!found) {
                out.push_back({current[i], true, modifiers, received_us});
            }
        }

        for (size_t i = 0; i < MAX_KEYS; i++) {
            if (prev_keys_[i] == 0) continue;
            bool found = false;
            for (size_t j = 0; j < MAX_KEYS; j++) {
                if (current[j] == prev_keys_[i]) { found = true; break; }
            }
            if (!found) {
                out.push_back({prev_keys_[i], false, modifiers, received_us});
            }
        }

        for (size_t i = 0; i < MAX_KEYS; i++) prev_keys_[i] = current[i];
        return true;
    }

private:
    uint8_t prev_mods_ = 0;
    uint8_t prev_keys_[MAX_KEYS]{};
};

}  // namespace hid_report
