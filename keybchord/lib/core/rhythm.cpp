#include "rhythm.h"

#include <ArduinoJson.h>

#include "params.h"


namespace {

constexpr const char* kRhythmNames[RHYTHM_COUNT] = {
    "Rock 1", "Rock 2", "Waltz", "Swing", "Slow Rock", "Bossa Nova",
    "Rhumba", "Tango", "March", "Samba", "Disco", "Foxtrot",
};

constexpr const char* kRhythmFiles[RHYTHM_COUNT] = {
    "rock1.json", "rock2.json", "waltz.json", "swing.json",
    "slow_rock.json", "bossa_nova.json", "rhumba.json", "tango.json",
    "march.json", "samba.json", "disco.json", "foxtrot.json",
};

} // namespace


bool parseRhythmPattern(const std::string& json, RhythmPattern& out) {
    JsonDocument doc;
    if (deserializeJson(doc, json)) return false;
    if (!doc.is<JsonObject>()) return false;

    RhythmPattern p;
    if (doc.containsKey("name") && doc["name"].is<const char*>()) {
        p.name = doc["name"].as<std::string>();
    }
    if (doc.containsKey("steps_per_bar") && doc["steps_per_bar"].is<int>()) {
        p.steps_per_bar = doc["steps_per_bar"].as<int>();
    }
    if (doc.containsKey("swing") && doc["swing"].is<int>()) {
        p.swing = static_cast<int8_t>(
            clamp<int>(doc["swing"].as<int>(), -75, 75));
    }
    if (doc.containsKey("tracks") && doc["tracks"].is<JsonArray>()) {
        for (JsonVariant tv : doc["tracks"].as<JsonArray>()) {
            if (!tv.is<JsonObject>()) continue;
            RhythmTrack t;
            if (tv.containsKey("note") && tv["note"].is<int>()) {
                t.note = static_cast<uint8_t>(tv["note"].as<int>());
            }
            if (tv.containsKey("name") && tv["name"].is<const char*>()) {
                t.name = tv["name"].as<std::string>();
            }
            if (tv.containsKey("pattern") && tv["pattern"].is<JsonArray>()) {
                for (JsonVariant v : tv["pattern"].as<JsonArray>()) {
                    int val = v.is<int>() ? v.as<int>() : 0;
                    t.pattern.push_back(static_cast<uint8_t>(
                        clamp<int>(val, 0, 127)));
                }
            }
            p.tracks.push_back(t);
        }
    }

    out = p;
    return true;
}

uint32_t stepUs(uint16_t bpm) {
    if (bpm == 0) bpm = 120;
    return 60000000u / (static_cast<uint32_t>(bpm) * RHYTHM_STEPS_PER_BEAT);
}

uint32_t clockTickUs(uint16_t bpm) {
    return stepUs(bpm) / CLOCK_TICKS_PER_STEP;
}

uint64_t stepOffsetUs(int step, uint32_t base_step_us, int8_t swing) {
    if (step < 0) return 0;
    uint64_t t = static_cast<uint64_t>(step) * base_step_us;
    int stepInBeat = step % RHYTHM_STEPS_PER_BEAT;
    // Swing the off-beat 8th note (the "and") of each beat, not the 16th "e"/"a":
    // standard drum patterns place their hats on the 8th grid, so this is what
    // makes the drums audibly swing.
    if (stepInBeat == RHYTHM_STEPS_PER_BEAT / 2) {
        // Signed swing: positive delays the off-beat, negative rushes it.
        t += (static_cast<int64_t>(base_step_us) * swing) / 100;
    }
    return t;
}

std::vector<StepEvent> stepEvents(const RhythmPattern& p, int step) {
    std::vector<StepEvent> out;
    if (step < 0 || step >= p.steps_per_bar) return out;
    for (const auto& t : p.tracks) {
        if (step < static_cast<int>(t.pattern.size()) && t.pattern[step] > 0) {
            uint8_t v = t.pattern[step];
            if (v == 1) v = RHYTHM_DEFAULT_VELOCITY;  // "1" = default velocity
            out.push_back({t.note, v});
        }
    }
    return out;
}

uint8_t mapDrumNote(uint8_t note, const DrumMap& drums) {
    switch (note) {
        case 36: return drums.kick;      // Bass Drum 1
        case 38: return drums.snare;     // Acoustic Snare
        case 42: return drums.hihat;     // Closed Hi-Hat
        case 46: return drums.open_hat;  // Open Hi-Hat
        case 37: return drums.rimshot;   // Side Stick
        case 39: return drums.clap;      // Hand Clap
        case 49: return drums.crash;     // Crash Cymbal 1
        case 51: return drums.ride;      // Ride Cymbal 1
        case 61: return drums.bongo;     // Low Bongo
        case 62: return drums.conga_lo;  // Mute Hi Conga
        case 63: return drums.conga_hi;  // Open Hi Conga
        case 75: return drums.clave;     // Claves
        case 82: return drums.shaker;    // Shaker
        default: return note;
    }
}

uint8_t mapDrumVelocity(uint8_t note, const DrumMap& drums, uint8_t patternVelocity) {
    uint8_t v = 0;
    switch (note) {
        case 36: v = drums.kick_vel;      break;
        case 38: v = drums.snare_vel;     break;
        case 42: v = drums.hihat_vel;     break;
        case 46: v = drums.open_hat_vel;  break;
        case 37: v = drums.rimshot_vel;   break;
        case 39: v = drums.clap_vel;      break;
        case 49: v = drums.crash_vel;     break;
        case 51: v = drums.ride_vel;      break;
        case 61: v = drums.bongo_vel;     break;
        case 62: v = drums.conga_lo_vel;  break;
        case 63: v = drums.conga_hi_vel;  break;
        case 75: v = drums.clave_vel;     break;
        case 82: v = drums.shaker_vel;    break;
        default: return patternVelocity;
    }
    if (v == param_bounds::DRUM_VELOCITY_OFF) return 0;  // muted piece
    return (v == 0) ? patternVelocity : v;
}

const char* rhythmName(int index) {
    if (index < 0 || index >= RHYTHM_COUNT) return kRhythmNames[0];
    return kRhythmNames[index];
}

int rhythmIndex(const std::string& name) {
    for (int i = 0; i < RHYTHM_COUNT; i++) {
        if (name == kRhythmNames[i]) return i;
    }
    return -1;
}

const char* rhythmFileName(int index) {
    if (index < 0 || index >= RHYTHM_COUNT) return kRhythmFiles[0];
    return kRhythmFiles[index];
}
