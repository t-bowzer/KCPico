#include "bass_engine.h"

#include <algorithm>

#include "bass.h"
#include "chord_engine.h"
#include "defaults.h"
#include "midi_router.h"
#include "rhythm.h"


namespace {

// Whole/half notes release slightly before the next note's attack so the
// articulation stays clean instead of smearing into the following note.
constexpr uint64_t SUSTAIN_RELEASE_EARLY_US = 30000;

// Built-in fallback pattern: root-3rd-5th-6th/7th on beats 1-4 (spec 6.8).
// Used when no pattern JSON can be loaded from storage.
BassPattern builtinWalking() {
    BassPattern p;
    p.name = "Walking";
    p.steps_per_bar = 16;
    p.steps = {0,-1,-1,-1, 1,-1,-1,-1, 2,-1,-1,-1, 3,-1,-1,-1};
    return p;
}

} // namespace


BassEngine::BassEngine(StateManager& state, MidiRouter& router)
    : state_(state), router_(router) {}

void BassEngine::setPatterns(std::vector<BassPattern> patterns) {
    patterns_ = std::move(patterns);
    if (!patterns_.empty() && state_.pendingBass.pattern >= patterns_.size()) {
        state_.pendingBass.pattern = 0;
    }
}

const BassPattern* BassEngine::patternAt(int index) const {
    if (index < 0 || index >= static_cast<int>(patterns_.size())) return nullptr;
    return &patterns_[index];
}

const BassPattern* BassEngine::currentPattern() const {
    int idx = state_.pendingBass.pattern;
    if (idx < 0 || idx >= static_cast<int>(patterns_.size())) return nullptr;
    return &patterns_[idx];
}

void BassEngine::setChordEngine(const ChordEngine* chord) {
    chord_ = chord;
}

void BassEngine::update(uint64_t now_us) {
    const BassParams& b = state_.pendingBass;

    // Release a note whose deadline has passed (percussive note-off FR-B4, or
    // the end of a sustained whole/half note).
    if (noteActive_ && now_us >= offDeadlineUs_) {
        releaseNote();
    }

    if (!b.enabled) {
        if (noteActive_) releaseNote();
        lastStepAbs_ = state_.rhythmClock.stepAbs;
        return;
    }

    bool chordChanged = detectChordChange();

    // The Hold pattern is not beat-driven: it tracks the chord's sounding state.
    const BassPattern* pat = currentPattern();
    if (pat && pat->hold) {
        updateHold(now_us);
        lastStepAbs_ = state_.rhythmClock.stepAbs;
        return;
    }

    // A new chord re-articulates a sustained whole/half note immediately (so the
    // bass follows the new root mid-measure). Percussive patterns already pick
    // up the new root on the next step edge.
    if (chordChanged && noteActive_ && sustainSteps_ > 0) {
        refireRoot(now_us);
    }

    bool running = state_.rhythmClock.running;
    uint32_t stepAbs = state_.rhythmClock.stepAbs;

    if (running && stepAbs != lastStepAbs_) {
        if (pat) {
            int spb = pat->steps_per_bar;
            int rhythmSpb = state_.rhythmClock.stepsPerBar.load();
            // Follow the rhythm's meter when it is shorter (e.g. a 4/4 bass over
            // a 3/4 waltz plays only its first 12 steps, then wraps).
            uint32_t effective = static_cast<uint32_t>(
                (rhythmSpb > 0 && rhythmSpb < spb) ? rhythmSpb : spb);
            if (effective == 0) effective = 16;
            // `stepAbs` is incremented after each fired step, so the step that
            // just fired is (stepAbs - 1) on the shared 16th-note grid.
            uint32_t stepInBar = (stepAbs - 1) % effective;
            fireStep(stepInBar, now_us, effective);
        }
    }

    lastStepAbs_ = stepAbs;
}

void BassEngine::fireStep(uint32_t stepInBar, uint64_t now_us, uint32_t effectiveSteps) {
    const BassParams& b = state_.pendingBass;
    if (state_.pendingChord.play_mode == PlayMode::Silent) return;  // FR-B1
    if (!state_.selectedChordValid) return;

    const BassPattern* pat = currentPattern();
    if (!pat) return;

    int offset = bassStepOffset(*pat, state_.selectedChord.type,
                                static_cast<int>(stepInBar));
    if (offset < 0) return;  // rest on this step

    // Monophonic: release the previous note before the new one.
    if (noteActive_) releaseNote();

    const ResolvedChord& chord = state_.selectedChord;
    int note = rootMidi(chord.rootPc, state_.config.base_root_midi, b.octave) + offset;
    if (note < 0) note = 0;
    if (note > 127) note = 127;

    router_.noteOn(b.channel, static_cast<uint8_t>(note), b.velocity);
    note_ = static_cast<uint8_t>(note);
    noteActive_ = true;

    int sustain = bassStepSustain(*pat, static_cast<int>(stepInBar));
    if (sustain > static_cast<int>(effectiveSteps)) sustain = static_cast<int>(effectiveSteps);
    sustainSteps_ = sustain;
    offDeadlineUs_ = sustainDeadline(now_us, sustain);
}

void BassEngine::updateHold(uint64_t now_us) {
    const BassParams& b = state_.pendingBass;

    bool chordHeld = state_.selectedChordValid &&
                     state_.pendingChord.play_mode != PlayMode::Silent &&
                     chord_ != nullptr && chord_->isSounding();

    if (chordHeld) {
        int note = rootNote();
        if (!noteActive_) {
            router_.noteOn(b.channel, static_cast<uint8_t>(note), b.velocity);
            note_ = static_cast<uint8_t>(note);
            noteActive_ = true;
            sustainSteps_ = 0;
            offDeadlineUs_ = UINT64_MAX;  // sustains until the chord is released
        } else if (note_ != static_cast<uint8_t>(note)) {
            // Chord changed while still sounding: follow the new root.
            releaseNote();
            router_.noteOn(b.channel, static_cast<uint8_t>(note), b.velocity);
            note_ = static_cast<uint8_t>(note);
            noteActive_ = true;
            sustainSteps_ = 0;
            offDeadlineUs_ = UINT64_MAX;
        }
    } else if (noteActive_) {
        releaseNote();
    }
}

bool BassEngine::detectChordChange() {
    if (!state_.selectedChordValid) {
        lastChordValid_ = false;
        return false;
    }
    const ResolvedChord& c = state_.selectedChord;
    bool changed = !lastChordValid_ || c.rootPc != lastRootPc_ || c.type != lastType_;
    lastRootPc_ = c.rootPc;
    lastType_ = c.type;
    lastChordValid_ = true;
    return changed;
}

void BassEngine::refireRoot(uint64_t now_us) {
    const BassParams& b = state_.pendingBass;
    if (noteActive_) releaseNote();

    int note = rootNote();
    router_.noteOn(b.channel, static_cast<uint8_t>(note), b.velocity);
    note_ = static_cast<uint8_t>(note);
    noteActive_ = true;
    offDeadlineUs_ = sustainDeadline(now_us, sustainSteps_);
}

int BassEngine::rootNote() const {
    const BassParams& b = state_.pendingBass;
    const ResolvedChord& chord = state_.selectedChord;
    int note = rootMidi(chord.rootPc, state_.config.base_root_midi, b.octave);
    if (note < 0) note = 0;
    if (note > 127) note = 127;
    return note;
}

uint64_t BassEngine::sustainDeadline(uint64_t now_us, int sustainSteps) const {
    const BassParams& b = state_.pendingBass;
    if (sustainSteps > 0) {
        uint64_t total = static_cast<uint64_t>(stepUs(state_.pendingRhythm.tempo))
                         * static_cast<uint64_t>(sustainSteps);
        if (total > SUSTAIN_RELEASE_EARLY_US) {
            total -= SUSTAIN_RELEASE_EARLY_US;
        } else {
            total = 0;
        }
        return now_us + total;
    }
    return now_us + static_cast<uint64_t>(b.note_duration_ms) * 1000ULL;
}

void BassEngine::releaseNote() {
    router_.noteOff(state_.pendingBass.channel, note_);
    noteActive_ = false;
    sustainSteps_ = 0;
}

void BassEngine::allNotesOff() {
    if (noteActive_) {
        releaseNote();
    }
}

BassLibrary loadBassPatterns(StorageAdapter& storage) {
    BassLibrary lib;
    lib.patterns.reserve(BASS_COUNT + 4);
    lib.names.reserve(BASS_COUNT + 4);

    // 1. Built-in patterns, in fixed order (indices 0..BASS_COUNT-1). A missing
    // or unparseable file is re-provisioned from the embedded template so
    // indices stay stable and presets that store a built-in index keep pointing
    // at the right pattern.
    for (int i = 0; i < BASS_COUNT; i++) {
        std::string path = "/bass/" + std::string(bassFileName(i));
        std::string raw;
        if (storage.exists(path)) raw = storage.readFile(path);

        BassPattern p;
        if (!parseBassPattern(raw, p)) {
            const char* tmpl = defaultBassJson(i);
            raw = tmpl ? tmpl : "";
            storage.writeFile(path, raw);
            parseBassPattern(raw, p);
        }
        lib.patterns.push_back(p);
        lib.names.push_back(p.name.empty() ? bassName(i) : p.name);
    }

    // 2. User-provided patterns: any other *.json in /bass/, sorted by filename
    // so the order is deterministic. Appended at indices BASS_COUNT.. up to
    // MAX_BASS_PATTERNS total.
    auto files = storage.listFiles("/bass");
    std::sort(files.begin(), files.end());
    for (const auto& f : files) {
        if (static_cast<int>(lib.patterns.size()) >= MAX_BASS_PATTERNS) break;
        if (f.size() < 5 || f.compare(f.size() - 5, 5, ".json") != 0) continue;
        bool builtin = false;
        for (int i = 0; i < BASS_COUNT; i++) {
            if (f == bassFileName(i)) { builtin = true; break; }
        }
        if (builtin) continue;

        std::string raw = storage.readFile("/bass/" + f);
        BassPattern p;
        if (!parseBassPattern(raw, p) || !p.valid()) continue;
        lib.patterns.push_back(p);
        lib.names.push_back(p.name.empty() ? f.substr(0, f.size() - 5) : p.name);
    }

    if (lib.patterns.empty()) {
        lib.patterns.push_back(builtinWalking());
        lib.names.push_back("Walking");
    }
    return lib;
}
