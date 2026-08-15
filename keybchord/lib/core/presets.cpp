#include "presets.h"
#include "base.h"
#include "rhythm.h"
#include <ArduinoJson.h>
#include <cstdio>


PresetSlot PresetSlot::defaults() {
    PresetSlot p;
    p.name   = "Default";
    p.chord  = ChordParams::defaults();
    p.strum  = StrumParams::defaults();
    p.rhythm = RhythmParams::defaults();
    return p;
}

bool PresetSlot::operator==(const PresetSlot& other) const {
    return name == other.name
        && chord.play_mode        == other.chord.play_mode
        && chord.octave           == other.chord.octave
        && chord.note_duration_ms == other.chord.note_duration_ms
        && chord.velocity         == other.chord.velocity
        && chord.pan              == other.chord.pan
        && chord.voicing_mode     == other.chord.voicing_mode
        && chord.add9             == other.chord.add9
        && chord.add11            == other.chord.add11
        && chord.add13            == other.chord.add13
        && chord.channel          == other.chord.channel
        && strum.octave           == other.strum.octave
        && strum.note_duration_ms == other.strum.note_duration_ms
        && strum.velocity         == other.strum.velocity
        && strum.limited_keys     == other.strum.limited_keys
        && strum.channel          == other.strum.channel
        && rhythm.enabled == other.rhythm.enabled
        && rhythm.tempo   == other.rhythm.tempo
        && rhythm.swing   == other.rhythm.swing
        && rhythm.muted   == other.rhythm.muted
        && rhythm.channel == other.rhythm.channel
        && rhythm.pattern == other.rhythm.pattern
        && rhythm.drums   == other.rhythm.drums;
}

bool PresetSlot::operator!=(const PresetSlot& other) const {
    return !(*this == other);
}

static std::string bankPath(int bank) {
    char buf[32];
    snprintf(buf, sizeof(buf), "/presets/bank%d.json", bank + 1);
    return buf;
}

PresetSlot loadPreset(StorageAdapter& storage, int bank, int slot) {
    if (bank < 0 || bank >= NUM_BANKS || slot < 0 || slot >= NUM_SLOTS) {
        return PresetSlot::defaults();
    }

    std::string path = bankPath(bank);
    if (!storage.exists(path)) {
        return PresetSlot::defaults();
    }

    std::string raw = storage.readFile(path);
    if (raw.empty()) {
        return PresetSlot::defaults();
    }

    JsonDocument doc;
    auto error = deserializeJson(doc, raw);
    if (error || !doc.is<JsonArray>()) {
        return PresetSlot::defaults();
    }

    auto arr = doc.as<JsonArray>();
    if (static_cast<int>(arr.size()) <= slot) {
        return PresetSlot::defaults();
    }

    auto obj = arr[slot];
    if (!obj.is<JsonObject>()) {
        return PresetSlot::defaults();
    }

    PresetSlot p = PresetSlot::defaults();

    if (obj.containsKey("name") && obj["name"].is<const char*>()) {
        p.name = obj["name"].as<std::string>();
    }

    if (obj.containsKey("chord")) {
        auto c = obj["chord"];
        if (c.containsKey("channel") && c["channel"].is<int>())
            p.chord.channel = static_cast<uint8_t>(clamp<int>(c["channel"].as<int>(), 1, 16));
        if (c.containsKey("octave") && c["octave"].is<int>())
            p.chord.octave = static_cast<int8_t>(clamp<int>(c["octave"].as<int>(), -3, 3));
        if (c.containsKey("note_duration_ms") && c["note_duration_ms"].is<int>())
            p.chord.note_duration_ms = static_cast<int16_t>(clamp<int>(c["note_duration_ms"].as<int>(), 50, 4000));
        if (c.containsKey("velocity") && c["velocity"].is<int>())
            p.chord.velocity = static_cast<uint8_t>(clamp<int>(c["velocity"].as<int>(), 1, 127));
        if (c.containsKey("pan") && c["pan"].is<int>())
            p.chord.pan = static_cast<uint8_t>(clamp<int>(c["pan"].as<int>(), 0, 127));
        if (c.containsKey("voicing_mode") && c["voicing_mode"].is<const char*>()) {
            std::string vm = c["voicing_mode"].as<std::string>();
            if (vm == "smart") p.chord.voicing_mode = VoicingMode::Smart;
        }
        if (c.containsKey("extensions") && c["extensions"].is<JsonObject>()) {
            auto ext = c["extensions"];
            if (ext.containsKey("add9"))  p.chord.add9  = ext["add9"].as<bool>();
            if (ext.containsKey("add11")) p.chord.add11 = ext["add11"].as<bool>();
            if (ext.containsKey("add13")) p.chord.add13 = ext["add13"].as<bool>();
        }
        if (c.containsKey("play_mode") && c["play_mode"].is<int>()) {
            int pm = c["play_mode"].as<int>();
            if (pm >= 0 && pm < static_cast<int>(PlayMode::COUNT))
                p.chord.play_mode = static_cast<PlayMode>(pm);
        }
    }

    if (obj.containsKey("strum")) {
        auto s = obj["strum"];
        if (s.containsKey("channel") && s["channel"].is<int>())
            p.strum.channel = static_cast<uint8_t>(clamp<int>(s["channel"].as<int>(), 1, 16));
        if (s.containsKey("octave") && s["octave"].is<int>())
            p.strum.octave = static_cast<int8_t>(clamp<int>(s["octave"].as<int>(), -3, 3));
        if (s.containsKey("note_duration_ms") && s["note_duration_ms"].is<int>())
            p.strum.note_duration_ms = static_cast<int16_t>(clamp<int>(s["note_duration_ms"].as<int>(), 50, 4000));
        if (s.containsKey("velocity") && s["velocity"].is<int>())
            p.strum.velocity = static_cast<uint8_t>(clamp<int>(s["velocity"].as<int>(), 1, 127));
        if (s.containsKey("limited_keys") && s["limited_keys"].is<bool>())
            p.strum.limited_keys = s["limited_keys"].as<bool>();
    }

    if (obj.containsKey("rhythm")) {
        auto r = obj["rhythm"];
        if (r.containsKey("channel") && r["channel"].is<int>())
            p.rhythm.channel = static_cast<uint8_t>(clamp<int>(r["channel"].as<int>(), 1, 16));
        if (r.containsKey("tempo") && r["tempo"].is<int>())
            p.rhythm.tempo = static_cast<uint16_t>(clamp<int>(r["tempo"].as<int>(), 40, 260));
        if (r.containsKey("swing") && r["swing"].is<int>())
            p.rhythm.swing = static_cast<int8_t>(clamp<int>(r["swing"].as<int>(), -75, 75));
        if (r.containsKey("enabled") && r["enabled"].is<bool>())
            p.rhythm.enabled = r["enabled"].as<bool>();
        if (r.containsKey("muted") && r["muted"].is<bool>())
            p.rhythm.muted = r["muted"].as<bool>();
        if (r.containsKey("pattern") && r["pattern"].is<const char*>()) {
            int idx = rhythmIndex(r["pattern"].as<std::string>());
            if (idx >= 0) p.rhythm.pattern = static_cast<uint8_t>(idx);
        }
        if (r.containsKey("drums") && r["drums"].is<JsonObject>()) {
            auto d = r["drums"];
            if (d.containsKey("kick") && d["kick"].is<int>())
                p.rhythm.drums.kick = static_cast<uint8_t>(clamp<int>(d["kick"].as<int>(), 0, 127));
            if (d.containsKey("snare") && d["snare"].is<int>())
                p.rhythm.drums.snare = static_cast<uint8_t>(clamp<int>(d["snare"].as<int>(), 0, 127));
            if (d.containsKey("hihat") && d["hihat"].is<int>())
                p.rhythm.drums.hihat = static_cast<uint8_t>(clamp<int>(d["hihat"].as<int>(), 0, 127));
            if (d.containsKey("open_hat") && d["open_hat"].is<int>())
                p.rhythm.drums.open_hat = static_cast<uint8_t>(clamp<int>(d["open_hat"].as<int>(), 0, 127));
        }
    }

    return p;
}

bool savePreset(StorageAdapter& storage, int bank, int slot, const PresetSlot& preset) {
    if (bank < 0 || bank >= NUM_BANKS || slot < 0 || slot >= NUM_SLOTS) {
        return false;
    }

    std::string path = bankPath(bank);

    JsonDocument doc;
    JsonArray arr;

    if (storage.exists(path)) {
        std::string raw = storage.readFile(path);
        if (!raw.empty()) {
            auto error = deserializeJson(doc, raw);
            if (!error && doc.is<JsonArray>()) {
                arr = doc.as<JsonArray>();
            }
        }
    }

    if (arr.isNull()) {
        arr = doc.to<JsonArray>();
    }

    while (static_cast<int>(arr.size()) <= slot) {
        arr.add(JsonObject());
    }

    auto obj = arr[slot];
    obj["name"] = preset.name;

    auto chord = obj["chord"].to<JsonObject>();
    chord["channel"]          = preset.chord.channel;
    chord["octave"]           = preset.chord.octave;
    chord["note_duration_ms"] = preset.chord.note_duration_ms;
    chord["velocity"]         = preset.chord.velocity;
    chord["pan"]              = preset.chord.pan;
    chord["voicing_mode"]     = (preset.chord.voicing_mode == VoicingMode::Smart) ? "smart" : "root_position";
    auto ext = chord["extensions"].to<JsonObject>();
    ext["add9"]  = preset.chord.add9;
    ext["add11"] = preset.chord.add11;
    ext["add13"] = preset.chord.add13;
    chord["play_mode"] = static_cast<int>(preset.chord.play_mode);

    auto strum = obj["strum"].to<JsonObject>();
    strum["channel"]          = preset.strum.channel;
    strum["octave"]           = preset.strum.octave;
    strum["note_duration_ms"] = preset.strum.note_duration_ms;
    strum["velocity"]         = preset.strum.velocity;
    strum["limited_keys"]     = preset.strum.limited_keys;

    auto rhythm = obj["rhythm"].to<JsonObject>();
    rhythm["channel"] = preset.rhythm.channel;
    rhythm["tempo"]   = preset.rhythm.tempo;
    rhythm["swing"]   = preset.rhythm.swing;
    rhythm["enabled"] = preset.rhythm.enabled;
    rhythm["muted"]   = preset.rhythm.muted;
    rhythm["pattern"] = rhythmName(preset.rhythm.pattern);
    auto drums = rhythm["drums"].to<JsonObject>();
    drums["kick"]     = preset.rhythm.drums.kick;
    drums["snare"]    = preset.rhythm.drums.snare;
    drums["hihat"]    = preset.rhythm.drums.hihat;
    drums["open_hat"] = preset.rhythm.drums.open_hat;

    std::string out;
    serializeJson(doc, out);

    return storage.writeFile(path, out);
}

PresetSlot loadPresetOrDefault(StorageAdapter& storage, int bank, int slot) {
    if (!storage.exists(bankPath(bank))) {
        return PresetSlot::defaults();
    }
    return loadPreset(storage, bank, slot);
}


