#include "bass.h"

#include <ArduinoJson.h>

#include "params.h"


namespace {

constexpr const char* kBassNames[BASS_COUNT] = {
    "Walking", "Whole", "Half", "Quarter", "Half Alt",
    "Quarter Alt", "3/4 Alt", "Hold", "No 6th",
};

constexpr const char* kBassFiles[BASS_COUNT] = {
    "walking.json", "whole.json", "half.json", "quarter.json",
    "half_alt.json", "quarter_alt.json", "three_four_alt.json",
    "hold.json", "walk_no_6th.json",
};

// Four-degree interval blueprint per chord type (spec 6.8). Degree 3 uses the
// chord's 7th when present, else the 6th (offset 9).
constexpr int8_t kBlueprints[][4] = {
    {0, 4, 7, 9},    // Major
    {0, 3, 7, 9},    // Minor
    {0, 4, 7, 10},   // Dom7
    {0, 4, 7, 11},   // Maj7
    {0, 3, 7, 10},   // Min7
    {0, 3, 6, 9},    // Dim
    {0, 3, 6, 9},    // Dim7
    {0, 4, 8, 9},    // Aug
    {0, 5, 7, 9},    // Sus4
    {0, 2, 7, 9},    // Sus2
    {0, 3, 6, 10},   // Min7b5
};

// Runtime name list (built-ins + user files). Empty means "use the built-in
// table". Installed by installBassNames() after loading patterns.
std::vector<std::string> g_names;

} // namespace


bool parseBassPattern(const std::string& json, BassPattern& out) {
    JsonDocument doc;
    if (deserializeJson(doc, json)) return false;
    if (!doc.is<JsonObject>()) return false;

    BassPattern p;
    if (doc.containsKey("name") && doc["name"].is<const char*>()) {
        p.name = doc["name"].as<std::string>();
    }
    if (doc.containsKey("steps_per_bar") && doc["steps_per_bar"].is<int>()) {
        p.steps_per_bar = doc["steps_per_bar"].as<int>();
    }
    if (doc.containsKey("hold") && doc["hold"].is<bool>()) {
        p.hold = doc["hold"].as<bool>();
    }
    if (doc.containsKey("steps") && doc["steps"].is<JsonArray>()) {
        for (JsonVariant v : doc["steps"].as<JsonArray>()) {
            int val = v.is<int>() ? v.as<int>() : 0;
            p.steps.push_back(static_cast<int8_t>(
                clamp<int>(val, -1, 3)));
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

int bassDegreeOffset(ChordType type, int degree) {
    int t = static_cast<int>(type);
    if (t < 0 || t >= static_cast<int>(ChordType::COUNT)) t = 0;
    if (degree < 0 || degree >= 4) return -1;
    return kBlueprints[t][degree];
}

int bassStepOffset(const BassPattern& p, ChordType type, int step) {
    if (p.hold) return -1;
    if (step < 0 || step >= static_cast<int>(p.steps.size())) return -1;
    int8_t degree = p.steps[step];
    if (degree < 0) return -1;  // rest
    return bassDegreeOffset(type, degree);
}

int bassStepSustain(const BassPattern& p, int step) {
    if (step < 0 || step >= static_cast<int>(p.sustain_steps.size())) return 0;
    return p.sustain_steps[step];
}

const char* bassName(int index) {
    if (!g_names.empty()) {
        if (index < 0 || index >= static_cast<int>(g_names.size())) return g_names[0].c_str();
        return g_names[index].c_str();
    }
    if (index < 0 || index >= BASS_COUNT) return kBassNames[0];
    return kBassNames[index];
}

int bassIndex(const std::string& name) {
    if (!g_names.empty()) {
        for (size_t i = 0; i < g_names.size(); i++) {
            if (name == g_names[i]) return static_cast<int>(i);
        }
        return -1;
    }
    for (int i = 0; i < BASS_COUNT; i++) {
        if (name == kBassNames[i]) return i;
    }
    return -1;
}

int bassCount() {
    return g_names.empty() ? BASS_COUNT : static_cast<int>(g_names.size());
}

void installBassNames(std::vector<std::string> names) {
    g_names = std::move(names);
}

void clearBassNames() {
    g_names.clear();
}

const char* bassFileName(int index) {
    if (index < 0 || index >= BASS_COUNT) return kBassFiles[0];
    return kBassFiles[index];
}
