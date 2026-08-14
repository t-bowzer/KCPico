#pragma once

#include <string>
#include "params.h"


class StorageAdapter;

struct AppConfig {
    bool     din_enabled             = true;
    bool     midi_clock_enabled      = false;
    uint8_t  base_root_midi          = 60;
    uint8_t  note_range_low          = 48;
    uint8_t  note_range_high         = 84;
    uint16_t display_revert_ms       = 1500;
    uint16_t display_prompt_ms       = 5000;
    bool     bpm_indicator           = true;
    uint8_t  led_flash_ms            = 40;
    bool     accent_downbeat         = true;
    uint8_t  accent_flash_ms         = 80;
    bool     led_only_when_rhythm    = true;
    std::string startup_preset       = "B1:P1";

    static AppConfig defaults();
    static AppConfig load(StorageAdapter& storage);
    bool save(StorageAdapter& storage) const;
};


