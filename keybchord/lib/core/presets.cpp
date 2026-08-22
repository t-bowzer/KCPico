#include "presets.h"
#include "base.h"
#include "rhythm.h"
#include "strum.h"
#include <ArduinoJson.h>
#include <cstdio>
#include <cstdlib>


PresetSlot PresetSlot::defaults() {
    PresetSlot p;
    p.name   = "Default";
    p.chord  = ChordParams::defaults();
    p.strum  = StrumParams::defaults();
    p.bass   = BassParams::defaults();
    p.rhythm = RhythmParams::defaults();
    return p;
}

bool PresetSlot::operator==(const PresetSlot& other) const {
    return name == other.name && sameParams(other);
}

bool PresetSlot::operator!=(const PresetSlot& other) const {
    return !(*this == other);
}

bool PresetSlot::sameParams(const PresetSlot& other) const {
    return chord.play_mode        == other.chord.play_mode
        && chord.octave           == other.chord.octave
        && chord.note_duration_ms == other.chord.note_duration_ms
        && chord.velocity         == other.chord.velocity
        && chord.pan              == other.chord.pan
        && chord.voicing_mode     == other.chord.voicing_mode
        && chord.chord_roll_ms    == other.chord.chord_roll_ms
        && chord.min_notes        == other.chord.min_notes
        && chord.min_interval     == other.chord.min_interval
        && chord.inversion        == other.chord.inversion
        && chord.arp_mode         == other.chord.arp_mode
        && chord.channel          == other.chord.channel
        && strum.octave           == other.strum.octave
        && strum.note_duration_ms == other.strum.note_duration_ms
        && strum.velocity         == other.strum.velocity
        && strum.limited_keys     == other.strum.limited_keys
        && strum.mode             == other.strum.mode
        && strum.root_pc          == other.strum.root_pc
        && strum.scale_type       == other.strum.scale_type
        && strum.channel          == other.strum.channel
        && bass.enabled           == other.bass.enabled
        && bass.octave            == other.bass.octave
        && bass.note_duration_ms  == other.bass.note_duration_ms
        && bass.velocity          == other.bass.velocity
        && bass.channel           == other.bass.channel
        && bass.pattern           == other.bass.pattern
        && rhythm.enabled == other.rhythm.enabled
        && rhythm.tempo   == other.rhythm.tempo
        && rhythm.swing   == other.rhythm.swing
        && rhythm.muted   == other.rhythm.muted
        && rhythm.channel == other.rhythm.channel
        && rhythm.pattern == other.rhythm.pattern
        && rhythm.drums   == other.rhythm.drums;
}

PresetSlot makePreset(const ChordParams& chord, const StrumParams& strum,
                      const BassParams& bass, const RhythmParams& rhythm,
                      const std::string& name) {
    PresetSlot p;
    p.name   = name;
    p.chord  = chord;
    p.strum  = strum;
    p.bass   = bass;
    p.rhythm = rhythm;
    return p;
}

bool parsePresetLocation(const std::string& loc, int& bank, int& slot) {
    // Expected form "B<bank>:P<slot>" (1-based, case-insensitive).
    size_t b = loc.find_first_of("bB");
    size_t colon = loc.find(':');
    size_t p = loc.find_first_of("pP");
    if (b == std::string::npos || colon == std::string::npos ||
        p == std::string::npos || b > colon || colon > p) {
        return false;
    }

    std::string bankStr = loc.substr(b + 1, colon - b - 1);
    std::string slotStr = loc.substr(p + 1);
    if (bankStr.empty() || slotStr.empty()) return false;

    int bv = 0, sv = 0;
    for (char c : bankStr) if (c < '0' || c > '9') return false;
    for (char c : slotStr) if (c < '0' || c > '9') return false;
    bv = std::atoi(bankStr.c_str());
    sv = std::atoi(slotStr.c_str());

    if (bv < 1 || bv > NUM_BANKS || sv < 1 || sv > NUM_SLOTS) return false;
    bank = bv - 1;
    slot = sv - 1;
    return true;
}

static std::string bankPath(int bank) {
    char buf[32];
    snprintf(buf, sizeof(buf), "/presets/bank%d.json", bank + 1);
    return buf;
}

namespace {

int legacyPlayModeRemap(int pm) {
    // M10: the old `Rhythm` play mode (enum value 3) was removed; the walking
    // bass replaced it. Remap legacy int values: 3 (Rhythm) -> 2 (Arpeggio);
    // 4 (Silent) -> 3 (Silent). Values 0..2 map unchanged.
    switch (pm) {
        case 3: return static_cast<int>(PlayMode::Arpeggio);  // old Rhythm
        case 4: return static_cast<int>(PlayMode::Silent);    // old Silent
        default:
            if (pm >= 0 && pm < 3) return pm;
            return static_cast<int>(PlayMode::Held);
    }
}

InversionMode parseInversion(const std::string& s) {
    if (s == "first")  return InversionMode::First;
    if (s == "second") return InversionMode::Second;
    if (s == "third")  return InversionMode::Third;
    return InversionMode::Root;
}

const char* inversionName(InversionMode m) {
    switch (m) {
        case InversionMode::First:  return "first";
        case InversionMode::Second: return "second";
        case InversionMode::Third:  return "third";
        default:                    return "root";
    }
}

ArpMode parseArpMode(const std::string& s) {
    if (s == "down")        return ArpMode::Down;
    if (s == "up_down")     return ArpMode::UpDown;
    if (s == "alternating") return ArpMode::Alternating;
    if (s == "random")      return ArpMode::Random;
    return ArpMode::Up;
}

const char* arpModeName(ArpMode m) {
    switch (m) {
        case ArpMode::Down:        return "down";
        case ArpMode::UpDown:      return "up_down";
        case ArpMode::Alternating: return "alternating";
        case ArpMode::Random:      return "random";
        default:                   return "up";
    }
}

StrumMode parseStrumMode(const std::string& s) {
    if (s == "scale") return StrumMode::Scale;
    if (s == "piano") return StrumMode::Piano;
    return StrumMode::FollowChord;
}

const char* strumModeName(StrumMode m) {
    switch (m) {
        case StrumMode::Scale: return "scale";
        case StrumMode::Piano: return "piano";
        default:               return "follow_chord";
    }
}

ScaleType parseScaleType(const std::string& s) {
    if (s == "dorian")             return ScaleType::Dorian;
    if (s == "phrygian")           return ScaleType::Phrygian;
    if (s == "lydian")             return ScaleType::Lydian;
    if (s == "mixolydian")         return ScaleType::Mixolydian;
    if (s == "aeolian")            return ScaleType::Aeolian;
    if (s == "locrian")            return ScaleType::Locrian;
    if (s == "harmonic_minor")     return ScaleType::HarmonicMinor;
    if (s == "melodic_minor")      return ScaleType::MelodicMinor;
    if (s == "major_pentatonic")   return ScaleType::MajorPentatonic;
    if (s == "minor_pentatonic")   return ScaleType::MinorPentatonic;
    if (s == "blues")              return ScaleType::Blues;
    return ScaleType::Ionian;
}

BassPattern parseBassPattern(const std::string& s) {
    if (s == "whole")          return BassPattern::Whole;
    if (s == "half")           return BassPattern::Half;
    if (s == "quarter")        return BassPattern::Quarter;
    if (s == "half_alt")       return BassPattern::HalfAlt;
    if (s == "quarter_alt")    return BassPattern::QuarterAlt;
    if (s == "three_four_alt") return BassPattern::ThreeFourAlt;
    if (s == "hold")           return BassPattern::Hold;
    if (s == "walk_no_6th")    return BassPattern::WalkNoSixth;
    return BassPattern::Walking;
}

const char* bassPatternName(BassPattern p) {
    switch (p) {
        case BassPattern::Whole:        return "whole";
        case BassPattern::Half:         return "half";
        case BassPattern::Quarter:      return "quarter";
        case BassPattern::HalfAlt:      return "half_alt";
        case BassPattern::QuarterAlt:   return "quarter_alt";
        case BassPattern::ThreeFourAlt: return "three_four_alt";
        case BassPattern::Hold:         return "hold";
        case BassPattern::WalkNoSixth:  return "walk_no_6th";
        default:                        return "walking";
    }
}

} // namespace

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
        if (c.containsKey("chord_roll_ms") && c["chord_roll_ms"].is<int>())
            p.chord.chord_roll_ms = static_cast<int16_t>(clamp<int>(c["chord_roll_ms"].as<int>(), -2000, 2000));
        if (c.containsKey("min_notes") && c["min_notes"].is<int>())
            p.chord.min_notes = static_cast<uint8_t>(clamp<int>(c["min_notes"].as<int>(), 2, 6));
        if (c.containsKey("min_interval") && c["min_interval"].is<int>())
            p.chord.min_interval = static_cast<uint8_t>(clamp<int>(c["min_interval"].as<int>(), 0, 12));
        if (c.containsKey("inversion") && c["inversion"].is<const char*>())
            p.chord.inversion = parseInversion(c["inversion"].as<std::string>());
        if (c.containsKey("arp_mode") && c["arp_mode"].is<const char*>())
            p.chord.arp_mode = parseArpMode(c["arp_mode"].as<std::string>());
        if (c.containsKey("play_mode")) {
            if (c["play_mode"].is<int>()) {
                p.chord.play_mode = static_cast<PlayMode>(legacyPlayModeRemap(c["play_mode"].as<int>()));
            } else if (c["play_mode"].is<const char*>()) {
                std::string pm = c["play_mode"].as<std::string>();
                if (pm == "press_to_play")      p.chord.play_mode = PlayMode::PressToPlay;
                else if (pm == "arpeggio")      p.chord.play_mode = PlayMode::Arpeggio;
                else if (pm == "rhythm")        p.chord.play_mode = PlayMode::Arpeggio;  // legacy
                else if (pm == "silent")        p.chord.play_mode = PlayMode::Silent;
                else                             p.chord.play_mode = PlayMode::Held;
            }
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
        if (s.containsKey("mode") && s["mode"].is<const char*>())
            p.strum.mode = parseStrumMode(s["mode"].as<std::string>());
        if (s.containsKey("root_pc") && s["root_pc"].is<int>())
            p.strum.root_pc = static_cast<uint8_t>(clamp<int>(s["root_pc"].as<int>(), 0, 11));
        if (s.containsKey("scale_type") && s["scale_type"].is<const char*>())
            p.strum.scale_type = parseScaleType(s["scale_type"].as<std::string>());
    }

    if (obj.containsKey("bass")) {
        auto b = obj["bass"];
        if (b.containsKey("channel") && b["channel"].is<int>())
            p.bass.channel = static_cast<uint8_t>(clamp<int>(b["channel"].as<int>(), 1, 16));
        if (b.containsKey("octave") && b["octave"].is<int>())
            p.bass.octave = static_cast<int8_t>(clamp<int>(b["octave"].as<int>(), -3, 3));
        if (b.containsKey("note_duration_ms") && b["note_duration_ms"].is<int>())
            p.bass.note_duration_ms = static_cast<int16_t>(clamp<int>(b["note_duration_ms"].as<int>(), 50, 4000));
        if (b.containsKey("velocity") && b["velocity"].is<int>())
            p.bass.velocity = static_cast<uint8_t>(clamp<int>(b["velocity"].as<int>(), 1, 127));
        if (b.containsKey("enabled") && b["enabled"].is<bool>())
            p.bass.enabled = b["enabled"].as<bool>();
        if (b.containsKey("pattern") && b["pattern"].is<const char*>())
            p.bass.pattern = parseBassPattern(b["pattern"].as<std::string>());
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
            if (d.containsKey("kick_vel") && d["kick_vel"].is<int>())
                p.rhythm.drums.kick_vel = static_cast<uint8_t>(clamp<int>(d["kick_vel"].as<int>(), 0, 128));
            if (d.containsKey("snare") && d["snare"].is<int>())
                p.rhythm.drums.snare = static_cast<uint8_t>(clamp<int>(d["snare"].as<int>(), 0, 127));
            if (d.containsKey("snare_vel") && d["snare_vel"].is<int>())
                p.rhythm.drums.snare_vel = static_cast<uint8_t>(clamp<int>(d["snare_vel"].as<int>(), 0, 128));
            if (d.containsKey("hihat") && d["hihat"].is<int>())
                p.rhythm.drums.hihat = static_cast<uint8_t>(clamp<int>(d["hihat"].as<int>(), 0, 127));
            if (d.containsKey("hihat_vel") && d["hihat_vel"].is<int>())
                p.rhythm.drums.hihat_vel = static_cast<uint8_t>(clamp<int>(d["hihat_vel"].as<int>(), 0, 128));
            if (d.containsKey("open_hat") && d["open_hat"].is<int>())
                p.rhythm.drums.open_hat = static_cast<uint8_t>(clamp<int>(d["open_hat"].as<int>(), 0, 127));
            if (d.containsKey("open_hat_vel") && d["open_hat_vel"].is<int>())
                p.rhythm.drums.open_hat_vel = static_cast<uint8_t>(clamp<int>(d["open_hat_vel"].as<int>(), 0, 128));
            if (d.containsKey("rimshot") && d["rimshot"].is<int>())
                p.rhythm.drums.rimshot = static_cast<uint8_t>(clamp<int>(d["rimshot"].as<int>(), 0, 127));
            if (d.containsKey("rimshot_vel") && d["rimshot_vel"].is<int>())
                p.rhythm.drums.rimshot_vel = static_cast<uint8_t>(clamp<int>(d["rimshot_vel"].as<int>(), 0, 128));
            if (d.containsKey("clap") && d["clap"].is<int>())
                p.rhythm.drums.clap = static_cast<uint8_t>(clamp<int>(d["clap"].as<int>(), 0, 127));
            if (d.containsKey("clap_vel") && d["clap_vel"].is<int>())
                p.rhythm.drums.clap_vel = static_cast<uint8_t>(clamp<int>(d["clap_vel"].as<int>(), 0, 128));
            if (d.containsKey("crash") && d["crash"].is<int>())
                p.rhythm.drums.crash = static_cast<uint8_t>(clamp<int>(d["crash"].as<int>(), 0, 127));
            if (d.containsKey("crash_vel") && d["crash_vel"].is<int>())
                p.rhythm.drums.crash_vel = static_cast<uint8_t>(clamp<int>(d["crash_vel"].as<int>(), 0, 128));
            if (d.containsKey("ride") && d["ride"].is<int>())
                p.rhythm.drums.ride = static_cast<uint8_t>(clamp<int>(d["ride"].as<int>(), 0, 127));
            if (d.containsKey("ride_vel") && d["ride_vel"].is<int>())
                p.rhythm.drums.ride_vel = static_cast<uint8_t>(clamp<int>(d["ride_vel"].as<int>(), 0, 128));
            if (d.containsKey("bongo") && d["bongo"].is<int>())
                p.rhythm.drums.bongo = static_cast<uint8_t>(clamp<int>(d["bongo"].as<int>(), 0, 127));
            if (d.containsKey("bongo_vel") && d["bongo_vel"].is<int>())
                p.rhythm.drums.bongo_vel = static_cast<uint8_t>(clamp<int>(d["bongo_vel"].as<int>(), 0, 128));
            if (d.containsKey("conga_lo") && d["conga_lo"].is<int>())
                p.rhythm.drums.conga_lo = static_cast<uint8_t>(clamp<int>(d["conga_lo"].as<int>(), 0, 127));
            if (d.containsKey("conga_lo_vel") && d["conga_lo_vel"].is<int>())
                p.rhythm.drums.conga_lo_vel = static_cast<uint8_t>(clamp<int>(d["conga_lo_vel"].as<int>(), 0, 128));
            if (d.containsKey("conga_hi") && d["conga_hi"].is<int>())
                p.rhythm.drums.conga_hi = static_cast<uint8_t>(clamp<int>(d["conga_hi"].as<int>(), 0, 127));
            if (d.containsKey("conga_hi_vel") && d["conga_hi_vel"].is<int>())
                p.rhythm.drums.conga_hi_vel = static_cast<uint8_t>(clamp<int>(d["conga_hi_vel"].as<int>(), 0, 128));
            if (d.containsKey("clave") && d["clave"].is<int>())
                p.rhythm.drums.clave = static_cast<uint8_t>(clamp<int>(d["clave"].as<int>(), 0, 127));
            if (d.containsKey("clave_vel") && d["clave_vel"].is<int>())
                p.rhythm.drums.clave_vel = static_cast<uint8_t>(clamp<int>(d["clave_vel"].as<int>(), 0, 128));
            if (d.containsKey("shaker") && d["shaker"].is<int>())
                p.rhythm.drums.shaker = static_cast<uint8_t>(clamp<int>(d["shaker"].as<int>(), 0, 127));
            if (d.containsKey("shaker_vel") && d["shaker_vel"].is<int>())
                p.rhythm.drums.shaker_vel = static_cast<uint8_t>(clamp<int>(d["shaker_vel"].as<int>(), 0, 128));
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
    chord["chord_roll_ms"]    = preset.chord.chord_roll_ms;
    chord["min_notes"]        = preset.chord.min_notes;
    chord["min_interval"]     = preset.chord.min_interval;
    chord["inversion"]        = inversionName(preset.chord.inversion);
    chord["arp_mode"]         = arpModeName(preset.chord.arp_mode);
    chord["play_mode"]        = static_cast<int>(preset.chord.play_mode);

    auto strum = obj["strum"].to<JsonObject>();
    strum["channel"]          = preset.strum.channel;
    strum["octave"]           = preset.strum.octave;
    strum["note_duration_ms"] = preset.strum.note_duration_ms;
    strum["velocity"]         = preset.strum.velocity;
    strum["limited_keys"]     = preset.strum.limited_keys;
    strum["mode"]             = strumModeName(preset.strum.mode);
    strum["root_pc"]          = preset.strum.root_pc;
    strum["scale_type"]       = scaleTypeName(preset.strum.scale_type);

    auto bass = obj["bass"].to<JsonObject>();
    bass["channel"]          = preset.bass.channel;
    bass["octave"]           = preset.bass.octave;
    bass["note_duration_ms"] = preset.bass.note_duration_ms;
    bass["velocity"]         = preset.bass.velocity;
    bass["enabled"]          = preset.bass.enabled;
    bass["pattern"]          = bassPatternName(preset.bass.pattern);

    auto rhythm = obj["rhythm"].to<JsonObject>();
    rhythm["channel"] = preset.rhythm.channel;
    rhythm["tempo"]   = preset.rhythm.tempo;
    rhythm["swing"]   = preset.rhythm.swing;
    rhythm["enabled"] = preset.rhythm.enabled;
    rhythm["muted"]   = preset.rhythm.muted;
    rhythm["pattern"] = rhythmName(preset.rhythm.pattern);
    auto drums = rhythm["drums"].to<JsonObject>();
    drums["kick"]         = preset.rhythm.drums.kick;
    drums["kick_vel"]     = preset.rhythm.drums.kick_vel;
    drums["snare"]        = preset.rhythm.drums.snare;
    drums["snare_vel"]    = preset.rhythm.drums.snare_vel;
    drums["hihat"]        = preset.rhythm.drums.hihat;
    drums["hihat_vel"]    = preset.rhythm.drums.hihat_vel;
    drums["open_hat"]     = preset.rhythm.drums.open_hat;
    drums["open_hat_vel"] = preset.rhythm.drums.open_hat_vel;
    drums["rimshot"]      = preset.rhythm.drums.rimshot;
    drums["rimshot_vel"]  = preset.rhythm.drums.rimshot_vel;
    drums["clap"]         = preset.rhythm.drums.clap;
    drums["clap_vel"]     = preset.rhythm.drums.clap_vel;
    drums["crash"]        = preset.rhythm.drums.crash;
    drums["crash_vel"]    = preset.rhythm.drums.crash_vel;
    drums["ride"]         = preset.rhythm.drums.ride;
    drums["ride_vel"]     = preset.rhythm.drums.ride_vel;
    drums["bongo"]        = preset.rhythm.drums.bongo;
    drums["bongo_vel"]    = preset.rhythm.drums.bongo_vel;
    drums["conga_lo"]     = preset.rhythm.drums.conga_lo;
    drums["conga_lo_vel"] = preset.rhythm.drums.conga_lo_vel;
    drums["conga_hi"]     = preset.rhythm.drums.conga_hi;
    drums["conga_hi_vel"] = preset.rhythm.drums.conga_hi_vel;
    drums["clave"]        = preset.rhythm.drums.clave;
    drums["clave_vel"]    = preset.rhythm.drums.clave_vel;
    drums["shaker"]       = preset.rhythm.drums.shaker;
    drums["shaker_vel"]   = preset.rhythm.drums.shaker_vel;

    std::string out;
    serializeJsonPretty(doc, out);

    return storage.writeFile(path, out);
}

PresetSlot loadPresetOrDefault(StorageAdapter& storage, int bank, int slot) {
    if (!storage.exists(bankPath(bank))) {
        return PresetSlot::defaults();
    }
    return loadPreset(storage, bank, slot);
}
