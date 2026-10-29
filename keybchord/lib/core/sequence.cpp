#include "sequence.h"

#include <algorithm>

#include <ArduinoJson.h>

#include "base.h"
#include "chordpattern.h"


namespace {

bool recordedEqualsDefaults(const ChordParams& p) {
    const ChordParams d = ChordParams::defaults();
    return p.octave == d.octave &&
           p.note_duration_ms == d.note_duration_ms &&
           p.velocity == d.velocity &&
           p.pan == d.pan &&
           p.voicing_mode == d.voicing_mode &&
           p.chord_roll_ms == d.chord_roll_ms &&
           p.min_notes == d.min_notes &&
           p.min_interval == d.min_interval &&
           p.inversion == d.inversion &&
           p.arp_mode == d.arp_mode &&
           p.pattern == d.pattern;
}

void writeRecorded(JsonObject obj, const ChordParams& p) {
    auto arr = obj["recorded"].to<JsonArray>();
    arr.add(p.octave);
    arr.add(p.note_duration_ms);
    arr.add(p.velocity);
    arr.add(p.pan);
    arr.add(static_cast<int>(p.voicing_mode));
    arr.add(p.chord_roll_ms);
    arr.add(p.min_notes);
    arr.add(p.min_interval);
    arr.add(static_cast<int>(p.inversion));
    arr.add(static_cast<int>(p.arp_mode));
    arr.add(p.pattern);
}

void readRecorded(JsonVariant v, ChordParams& p) {
    if (!v.is<JsonArray>()) return;
    auto arr = v.as<JsonArray>();
    int i = 0;
    for (JsonVariant item : arr) {
        if (i >= SEQUENCE_SETTING_COUNT) break;
        if (!item.is<int>()) { i++; continue; }
        int value = item.as<int>();
        switch (static_cast<SequenceSetting>(i)) {
            case SequenceSetting::Octave:
                p.octave = static_cast<int8_t>(clamp<int>(
                    value, param_bounds::CHORD_OCTAVE_MIN,
                    param_bounds::CHORD_OCTAVE_MAX));
                break;
            case SequenceSetting::NoteDuration:
                p.note_duration_ms = static_cast<int16_t>(clamp<int>(
                    value, param_bounds::CHORD_NOTE_DURATION_MIN,
                    param_bounds::CHORD_NOTE_DURATION_MAX));
                break;
            case SequenceSetting::Velocity:
                p.velocity = static_cast<uint8_t>(clamp<int>(
                    value, param_bounds::CHORD_VELOCITY_MIN,
                    param_bounds::CHORD_VELOCITY_MAX));
                break;
            case SequenceSetting::Pan:
                p.pan = static_cast<uint8_t>(clamp<int>(
                    value, param_bounds::CHORD_PAN_MIN,
                    param_bounds::CHORD_PAN_MAX));
                break;
            case SequenceSetting::Voicing:
                p.voicing_mode = static_cast<VoicingMode>(clamp<int>(
                    value, 0, static_cast<int>(VoicingMode::COUNT) - 1));
                break;
            case SequenceSetting::ChordRoll:
                p.chord_roll_ms = static_cast<int16_t>(clamp<int>(
                    value, param_bounds::CHORD_ROLL_MIN,
                    param_bounds::CHORD_ROLL_MAX));
                break;
            case SequenceSetting::MinNotes:
                p.min_notes = static_cast<uint8_t>(clamp<int>(
                    value, param_bounds::CHORD_MIN_NOTES_MIN,
                    param_bounds::CHORD_MIN_NOTES_MAX));
                break;
            case SequenceSetting::MinInterval:
                p.min_interval = static_cast<uint8_t>(clamp<int>(
                    value, param_bounds::CHORD_MIN_INTERVAL_MIN,
                    param_bounds::CHORD_MIN_INTERVAL_MAX));
                break;
            case SequenceSetting::Inversion:
                p.inversion = static_cast<InversionMode>(clamp<int>(
                    value, 0, static_cast<int>(InversionMode::COUNT) - 1));
                break;
            case SequenceSetting::ArpMode:
                p.arp_mode = static_cast<ArpMode>(clamp<int>(
                    value, 0, static_cast<int>(ArpMode::COUNT) - 1));
                break;
            case SequenceSetting::Pattern:
                p.pattern = static_cast<uint8_t>(clamp<int>(
                    value, 0, MAX_CHORD_PATTERNS - 1));
                break;
            default:
                break;
        }
        i++;
    }
}

bool parseEvent(JsonVariant v, uint16_t& step, uint16_t& chordId) {
    if (v.is<JsonArray>()) {
        auto arr = v.as<JsonArray>();
        if (arr.size() < 2 || !arr[0].is<int>() || !arr[1].is<int>()) {
            return false;
        }
        int s = arr[0].as<int>();
        int c = arr[1].as<int>();
        if (s < 0 || s > 0xFFFF || c < 0 || c > 0xFFFF) return false;
        step = static_cast<uint16_t>(s);
        chordId = static_cast<uint16_t>(c);
        return true;
    }
    if (v.is<JsonObject>()) {
        auto obj = v.as<JsonObject>();
        if (!obj["step"].is<int>() || !obj["chord"].is<int>()) return false;
        int s = obj["step"].as<int>();
        int c = obj["chord"].as<int>();
        if (s < 0 || s > 0xFFFF || c < 0 || c > 0xFFFF) return false;
        step = static_cast<uint16_t>(s);
        chordId = static_cast<uint16_t>(c);
        return true;
    }
    return false;
}

void parseSequence(JsonObject obj, Sequence& seq) {
    seq = Sequence();

    if (obj["bars"].is<int>()) {
        seq.bars = clampSequenceBars(obj["bars"].as<int>());
    }
    if (obj["steps_per_bar"].is<int>()) {
        seq.steps_per_bar = static_cast<uint8_t>(clamp<int>(
            obj["steps_per_bar"].as<int>(),
            SEQUENCE_MIN_STEPS_PER_BAR, SEQUENCE_MAX_STEPS_PER_BAR));
    }
    if (obj["recorded"].is<JsonArray>()) {
        readRecorded(obj["recorded"], seq.recorded);
    }
    if (obj["live"].is<int>()) {
        int mask = obj["live"].as<int>();
        if (mask < 0) mask = 0;
        if (mask > SEQUENCE_LIVE_MASK_ALL) mask = SEQUENCE_LIVE_MASK_ALL;
        seq.live_mask = static_cast<uint16_t>(mask);
    }

    if (!obj["events"].is<JsonArray>()) return;
    int loopSteps = seq.loopSteps();
    if (loopSteps <= 0) return;

    for (JsonVariant ev : obj["events"].as<JsonArray>()) {
        uint16_t step = 0, chordId = 0;
        if (!parseEvent(ev, step, chordId)) continue;
        if (static_cast<int>(step) >= loopSteps) continue;
        ResolvedChord chord;
        if (!sequenceChordFromId(chordId, chord)) continue;
        if (!seq.setEvent(step, chord)) {
            // Cap reached; ignore any further events (firmware truncates).
            if (static_cast<int>(seq.events.size()) >= MAX_SEQUENCE_EVENTS) break;
        }
    }
}

} // namespace


bool Sequence::isDefault() const {
    return events.empty() &&
           bars == SEQUENCE_DEFAULT_BARS &&
           steps_per_bar == SEQUENCE_DEFAULT_STEPS &&
           live_mask == 0 &&
           recordedEqualsDefaults(recorded);
}

bool Sequence::setEvent(uint16_t step, const ResolvedChord& chord) {
    for (auto& ev : events) {
        if (ev.step == step) {
            ev.chord = chord;
            return true;
        }
    }
    if (static_cast<int>(events.size()) >= MAX_SEQUENCE_EVENTS) return false;

    SequenceEvent ev;
    ev.step = step;
    ev.chord = chord;
    auto it = std::lower_bound(
        events.begin(), events.end(), step,
        [](const SequenceEvent& e, uint16_t s) { return e.step < s; });
    events.insert(it, ev);
    return true;
}

bool Sequence::removeEvent(uint16_t step) {
    for (auto it = events.begin(); it != events.end(); ++it) {
        if (it->step == step) {
            events.erase(it);
            return true;
        }
    }
    return false;
}

const SequenceEvent* Sequence::eventAt(uint16_t step) const {
    for (const auto& ev : events) {
        if (ev.step == step) return &ev;
        if (ev.step > step) break;
    }
    return nullptr;
}

ChordParams Sequence::mergedParams(const ChordParams& live,
                                   bool use_recorded) const {
    ChordParams out = live;
    if (use_recorded) {
        auto useRecorded = [this](SequenceSetting s) {
            uint16_t bit = static_cast<uint16_t>(1u << static_cast<int>(s));
            return (live_mask & bit) == 0;
        };
        if (useRecorded(SequenceSetting::Octave)) out.octave = recorded.octave;
        if (useRecorded(SequenceSetting::NoteDuration))
            out.note_duration_ms = recorded.note_duration_ms;
        if (useRecorded(SequenceSetting::Velocity))
            out.velocity = recorded.velocity;
        if (useRecorded(SequenceSetting::Pan)) out.pan = recorded.pan;
        if (useRecorded(SequenceSetting::Voicing))
            out.voicing_mode = recorded.voicing_mode;
        if (useRecorded(SequenceSetting::ChordRoll))
            out.chord_roll_ms = recorded.chord_roll_ms;
        if (useRecorded(SequenceSetting::MinNotes))
            out.min_notes = recorded.min_notes;
        if (useRecorded(SequenceSetting::MinInterval))
            out.min_interval = recorded.min_interval;
        if (useRecorded(SequenceSetting::Inversion))
            out.inversion = recorded.inversion;
        if (useRecorded(SequenceSetting::ArpMode))
            out.arp_mode = recorded.arp_mode;
        if (useRecorded(SequenceSetting::Pattern))
            out.pattern = recorded.pattern;
    }
    out.play_mode = PlayMode::Held;
    return out;
}

bool sequencerLibraryEmpty(const SequencerLibrary& lib) {
    for (const auto& seq : lib.slots) {
        if (!seq.isDefault()) return false;
    }
    return true;
}

uint16_t sequenceChordId(const ResolvedChord& chord) {
    int root = chord.rootPc;
    if (root < 0) root = 0;
    if (root > 11) root = 11;
    int type = static_cast<int>(chord.type);
    if (type < 0 || type >= static_cast<int>(ChordType::COUNT)) type = 0;
    int ext = (chord.add9 ? 1 : 0) | (chord.add11 ? 2 : 0) |
              (chord.add13 ? 4 : 0);
    return static_cast<uint16_t>(root * 128 + type * 8 + ext);
}

bool sequenceChordFromId(uint16_t id, ResolvedChord& out) {
    int root = id / 128;
    int rem = id % 128;
    int type = rem / 8;
    int ext = rem % 8;
    if (root < 0 || root > 11) return false;
    if (type < 0 || type >= static_cast<int>(ChordType::COUNT)) return false;

    out.rootPc = root;
    out.type = static_cast<ChordType>(type);
    out.add9 = (ext & 1) != 0;
    out.add11 = (ext & 2) != 0;
    out.add13 = (ext & 4) != 0;
    return true;
}

bool parseSequenceFile(const std::string& json, SequencerLibrary& out) {
    JsonDocument doc;
    if (deserializeJson(doc, json)) return false;

    JsonArray arr;
    if (doc.is<JsonObject>()) {
        if (!doc["sequences"].is<JsonArray>()) return false;
        arr = doc["sequences"].as<JsonArray>();
    } else if (doc.is<JsonArray>()) {
        arr = doc.as<JsonArray>();
        for (JsonVariant v : arr) {
            if (!v.is<JsonObject>() && !v.isNull()) return false;
        }
    } else {
        return false;
    }

    SequencerLibrary lib;
    int i = 0;
    for (JsonVariant v : arr) {
        if (i >= SEQUENCE_SLOTS) break;
        if (v.is<JsonObject>()) parseSequence(v.as<JsonObject>(), lib.slots[i]);
        i++;
    }
    out = lib;
    return true;
}

std::string serializeSequenceFile(const SequencerLibrary& lib, bool pretty) {
    JsonDocument doc;
    doc["version"] = 1;
    auto arr = doc["sequences"].to<JsonArray>();

    for (const auto& seq : lib.slots) {
        if (seq.isDefault()) {
            arr.add();
            continue;
        }
        auto obj = arr.add<JsonObject>();
        obj["bars"] = seq.bars;
        obj["steps_per_bar"] = seq.steps_per_bar;
        if (!recordedEqualsDefaults(seq.recorded)) {
            writeRecorded(obj, seq.recorded);
        }
        if (seq.live_mask != 0) obj["live"] = seq.live_mask;
        if (!seq.events.empty()) {
            auto events = obj["events"].to<JsonArray>();
            for (const auto& ev : seq.events) {
                auto e = events.add<JsonArray>();
                e.add(ev.step);
                e.add(sequenceChordId(ev.chord));
            }
        }
    }

    std::string out;
    if (pretty) {
        serializeJsonPretty(doc, out);
    } else {
        serializeJson(doc, out);
    }
    return out;
}

SequencerLibrary loadSequenceFile(StorageAdapter& storage, int bank, int slot) {
    SequencerLibrary lib;
    if (bank < 0 || slot < 0) return lib;
    std::string path = sequenceFileName(bank, slot);
    if (path.empty() || !storage.exists(path)) return lib;
    std::string raw = storage.readFile(path);
    if (raw.empty()) return lib;
    SequencerLibrary parsed;
    if (!parseSequenceFile(raw, parsed)) return lib;
    return parsed;
}

bool saveSequenceFile(StorageAdapter& storage, int bank, int slot,
                      const SequencerLibrary& lib) {
    if (bank < 0 || slot < 0) return false;
    std::string path = sequenceFileName(bank, slot);
    if (path.empty()) return false;

    if (sequencerLibraryEmpty(lib)) {
        if (storage.exists(path) && !storage.remove(path)) return false;
        return true;
    }

    storage.mkdir("/sequences");
    return storage.writeFile(path, serializeSequenceFile(lib));
}

bool removeSequenceFile(StorageAdapter& storage, int bank, int slot) {
    if (bank < 0 || slot < 0) return false;
    std::string path = sequenceFileName(bank, slot);
    if (path.empty() || !storage.exists(path)) return true;
    return storage.remove(path);
}

std::string sequenceFileName(int bank, int slot) {
    if (bank < 0 || slot < 0) return "";
    return "/sequences/b" + std::to_string(bank + 1) +
           "p" + std::to_string(slot + 1) + ".json";
}

uint8_t clampSequenceBars(int bars) {
    if (bars <= 1) return 1;
    if (bars <= 2) return 2;
    if (bars <= 4) return 4;
    return 8;
}
