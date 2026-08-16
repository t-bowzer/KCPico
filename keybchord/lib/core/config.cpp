#include "config.h"
#include "base.h"
#include <ArduinoJson.h>


static const char* CONFIG_PATH = "/config.json";

static int clampInt(int value, int minVal, int maxVal, int fallback) {
    if (value < minVal || value > maxVal) return fallback;
    return value;
}

AppConfig AppConfig::defaults() {
    AppConfig c;
    c.din_enabled          = true;
    c.midi_clock_enabled   = false;
    c.base_root_midi       = 60;
    c.note_range_low       = 48;
    c.note_range_high      = 84;
    c.display_revert_ms    = 1500;
    c.display_prompt_ms    = 5000;
    c.cursor_timeout_ms    = 5000;
    c.menu_timeout_ms      = 10000;
    c.bpm_indicator        = true;
    c.led_indicator        = LedTarget::NumLock;
    c.led_flash_ms         = 40;
    c.startup_preset       = "B1:P1";
    return c;
}

AppConfig AppConfig::load(StorageAdapter& storage) {
    AppConfig cfg = defaults();

    if (!storage.exists(CONFIG_PATH)) {
        return cfg;
    }

    std::string raw = storage.readFile(CONFIG_PATH);
    if (raw.empty()) {
        return cfg;
    }

    JsonDocument doc;
    auto error = deserializeJson(doc, raw);
    if (error) {
        return cfg;
    }

    if (doc.containsKey("midi")) {
        auto midi = doc["midi"];
        if (midi.containsKey("din_enabled") && midi["din_enabled"].is<bool>()) {
            cfg.din_enabled = midi["din_enabled"].as<bool>();
        }
        if (midi.containsKey("clock_enabled") && midi["clock_enabled"].is<bool>()) {
            cfg.midi_clock_enabled = midi["clock_enabled"].as<bool>();
        }
    }

    if (doc.containsKey("chord")) {
        auto chord = doc["chord"];
        if (chord.containsKey("base_root_midi") && chord["base_root_midi"].is<int>()) {
            cfg.base_root_midi = static_cast<uint8_t>(clampInt(
                chord["base_root_midi"].as<int>(), 0, 127, cfg.base_root_midi));
        }
        if (chord.containsKey("note_range") && chord["note_range"].is<JsonArray>() &&
            chord["note_range"].size() >= 2) {
            cfg.note_range_low = static_cast<uint8_t>(clampInt(
                chord["note_range"][0].as<int>(), 0, 127, cfg.note_range_low));
            cfg.note_range_high = static_cast<uint8_t>(clampInt(
                chord["note_range"][1].as<int>(), 0, 127, cfg.note_range_high));
        }
    }

    if (doc.containsKey("display")) {
        auto display = doc["display"];
        if (display.containsKey("revert_timeout_ms") && display["revert_timeout_ms"].is<int>()) {
            cfg.display_revert_ms = static_cast<uint16_t>(clampInt(
                display["revert_timeout_ms"].as<int>(), 250, 5000, cfg.display_revert_ms));
        }
        if (display.containsKey("prompt_timeout_ms") && display["prompt_timeout_ms"].is<int>()) {
            cfg.display_prompt_ms = static_cast<uint16_t>(clampInt(
                display["prompt_timeout_ms"].as<int>(), 1000, 30000, cfg.display_prompt_ms));
        }
        if (display.containsKey("cursor_timeout_ms") && display["cursor_timeout_ms"].is<int>()) {
            cfg.cursor_timeout_ms = static_cast<uint16_t>(clampInt(
                display["cursor_timeout_ms"].as<int>(), 500, 30000, cfg.cursor_timeout_ms));
        }
        if (display.containsKey("menu_timeout_ms") && display["menu_timeout_ms"].is<int>()) {
            cfg.menu_timeout_ms = static_cast<uint16_t>(clampInt(
                display["menu_timeout_ms"].as<int>(), 500, 30000, cfg.menu_timeout_ms));
        }
    }

    if (doc.containsKey("led")) {
        auto led = doc["led"];
        if (led.containsKey("bpm_indicator") && led["bpm_indicator"].is<bool>()) {
            cfg.bpm_indicator = led["bpm_indicator"].as<bool>();
        }
        if (led.containsKey("led") && led["led"].is<const char*>()) {
            std::string which = led["led"].as<std::string>();
            if (which == "caps_lock") cfg.led_indicator = LedTarget::CapsLock;
            else if (which == "num_lock") cfg.led_indicator = LedTarget::NumLock;
            else if (which == "scroll_lock") cfg.led_indicator = LedTarget::ScrollLock;
            else if (which == "all") cfg.led_indicator = LedTarget::All;
            else cfg.led_indicator = LedTarget::NumLock;
        }
        if (led.containsKey("flash_ms") && led["flash_ms"].is<int>()) {
            cfg.led_flash_ms = static_cast<uint8_t>(clampInt(
                led["flash_ms"].as<int>(), 5, 500, cfg.led_flash_ms));
        }
    }

    if (doc.containsKey("startup_preset") && doc["startup_preset"].is<const char*>()) {
        cfg.startup_preset = doc["startup_preset"].as<std::string>();
    }

    return cfg;
}

bool AppConfig::save(StorageAdapter& storage) const {
    JsonDocument doc;

    auto midi = doc["midi"].to<JsonObject>();
    midi["din_enabled"]   = din_enabled;
    midi["clock_enabled"] = midi_clock_enabled;

    auto chord = doc["chord"].to<JsonObject>();
    chord["base_root_midi"] = base_root_midi;
    auto range = chord["note_range"].to<JsonArray>();
    range.add(note_range_low);
    range.add(note_range_high);

    auto display = doc["display"].to<JsonObject>();
    display["revert_timeout_ms"] = display_revert_ms;
    display["prompt_timeout_ms"] = display_prompt_ms;
    display["cursor_timeout_ms"] = cursor_timeout_ms;
    display["menu_timeout_ms"]   = menu_timeout_ms;

    auto led = doc["led"].to<JsonObject>();
    led["bpm_indicator"]          = bpm_indicator;
    switch (led_indicator) {
        case LedTarget::CapsLock:  led["led"] = "caps_lock";  break;
        case LedTarget::NumLock:   led["led"] = "num_lock";   break;
        case LedTarget::ScrollLock: led["led"] = "scroll_lock"; break;
        case LedTarget::All:
        default:                   led["led"] = "all";        break;
    }
    led["flash_ms"]               = led_flash_ms;

    doc["startup_preset"] = startup_preset;

    std::string out;
    serializeJson(doc, out);

    return storage.writeFile(CONFIG_PATH, out);
}


