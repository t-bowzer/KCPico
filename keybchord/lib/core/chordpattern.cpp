#include "chordpattern.h"

#include <ArduinoJson.h>

#include <algorithm>


namespace {

constexpr const char* kChordPatternNames[CHORD_PATTERN_COUNT] = {
    "Block", "Alberti", "Broken Up", "Broken Down", "Pulse",
    "Oom-Pah", "Roll", "Split", "Arpeggio",
};

constexpr const char* kChordPatternFiles[CHORD_PATTERN_COUNT] = {
    "block.json", "alberti.json", "broken_up.json", "broken_down.json",
    "pulse.json", "oom_pah.json", "roll.json", "split.json", "arpeggio.json",
};

// Chord-tone semitone offsets per chord type (root, 3rd, 5th, 7th). Degree 3
// (7th) uses the chord's 7th when present; on a triad it wraps to 12 (the root
// an octave up).
constexpr int8_t kChordTones[][4] = {
    {0, 4, 7, 12},   // Major
    {0, 3, 7, 12},   // Minor
    {0, 4, 7, 10},   // Dom7
    {0, 4, 7, 11},   // Maj7
    {0, 3, 7, 10},   // Min7
    {0, 3, 6, 12},   // Dim
    {0, 3, 6, 9},    // Dim7
    {0, 4, 8, 12},   // Aug
    {0, 5, 7, 12},   // Sus4
    {0, 2, 7, 12},   // Sus2
    {0, 3, 6, 10},   // Min7b5
};

// Runtime name list (built-ins + user files). Empty means "use the built-in
// table".
std::vector<std::string> g_names;

} // namespace


bool parseChordPattern(const std::string& json, ChordPattern& out) {
    JsonDocument doc;
    if (deserializeJson(doc, json)) return false;
    if (!doc.is<JsonObject>()) return false;

    ChordPattern p;
    if (doc.containsKey("name") && doc["name"].is<const char*>()) {
        p.name = doc["name"].as<std::string>();
    }
    if (doc.containsKey("steps_per_bar") && doc["steps_per_bar"].is<int>()) {
        p.steps_per_bar = doc["steps_per_bar"].as<int>();
    }
    if (doc.containsKey("steps") && doc["steps"].is<JsonArray>()) {
        for (JsonVariant v : doc["steps"].as<JsonArray>()) {
            int val = v.is<int>() ? v.as<int>() : 0;
            p.steps.push_back(static_cast<uint8_t>(
                clamp<int>(val, 0, CHORD_MASK_SEVENTH)));
        }
    }
    if (doc.containsKey("sustain_steps") && doc["sustain_steps"].is<JsonArray>()) {
        for (JsonVariant v : doc["sustain_steps"].as<JsonArray>()) {
            int val = v.is<int>() ? v.as<int>() : 0;
            p.sustain_steps.push_back(static_cast<uint8_t>(
                clamp<int>(val, 0, 64)));
        }
    }

    if (p.steps.empty()) return false;

    out = p;
    return true;
}

std::vector<int> chordPatternOffsets(ChordType type, uint8_t mask) {
    int t = static_cast<int>(type);
    if (t < 0 || t >= static_cast<int>(ChordType::COUNT)) t = 0;

    std::vector<int> offsets;
    offsets.reserve(4);
    for (int bit = 0; bit < 4; bit++) {
        if (mask & (1u << bit)) {
            offsets.push_back(kChordTones[t][bit]);
        }
    }
    return offsets;
}

int chordPatternStepSustain(const ChordPattern& p, int step) {
    if (step < 0 || step >= static_cast<int>(p.sustain_steps.size())) return 0;
    return p.sustain_steps[step];
}

const char* chordPatternName(int index) {
    if (!g_names.empty()) {
        if (index < 0 || index >= static_cast<int>(g_names.size())) return g_names[0].c_str();
        return g_names[index].c_str();
    }
    if (index < 0 || index >= CHORD_PATTERN_COUNT) return kChordPatternNames[0];
    return kChordPatternNames[index];
}

int chordPatternIndex(const std::string& name) {
    if (!g_names.empty()) {
        for (size_t i = 0; i < g_names.size(); i++) {
            if (name == g_names[i]) return static_cast<int>(i);
        }
        return -1;
    }
    for (int i = 0; i < CHORD_PATTERN_COUNT; i++) {
        if (name == kChordPatternNames[i]) return i;
    }
    return -1;
}

int chordPatternCount() {
    return g_names.empty() ? CHORD_PATTERN_COUNT : static_cast<int>(g_names.size());
}

void installChordPatternNames(std::vector<std::string> names) {
    g_names = std::move(names);
}

void clearChordPatternNames() {
    g_names.clear();
}

const char* chordPatternFileName(int index) {
    if (index < 0 || index >= CHORD_PATTERN_COUNT) return kChordPatternFiles[0];
    return kChordPatternFiles[index];
}
