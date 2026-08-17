#pragma once

#include <string>
#include "params.h"


class StorageAdapter;

// Which keyboard LED(s) the BPM indicator flashes (FR-R8 / spec 9). RGB
// keyboards often lack a physical Scroll Lock LED, so this is configurable;
// "All" pulses num/caps/scroll simultaneously.
enum class LedTarget : uint8_t {
    All = 0,
    ScrollLock,
    CapsLock,
    NumLock,
};

struct AppConfig {
    bool     din_enabled             = true;
    bool     midi_clock_enabled      = false;
    uint8_t  base_root_midi          = 60;
    uint8_t  note_range_low          = 48;
    uint8_t  note_range_high         = 84;
    uint16_t display_revert_ms       = 1500;
    uint16_t display_prompt_ms       = 5000;
    uint16_t cursor_timeout_ms       = 5000;
    uint16_t menu_timeout_ms         = 10000;
    bool     bpm_indicator           = true;
    LedTarget led_indicator          = LedTarget::NumLock;
    uint8_t  led_flash_ms            = 40;
    std::string startup_preset       = "B1:P1";
    bool     debug_log_enabled       = true;   // NFR-8: key/strum/LED debug log
    bool     midi_monitor_enabled    = true;   // NFR-8: outgoing MIDI decode

    static AppConfig defaults();
    static AppConfig load(StorageAdapter& storage);
    bool save(StorageAdapter& storage) const;
};


