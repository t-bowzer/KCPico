#include "chord_engine.h"

#include <algorithm>

#include "defaults.h"
#include "midimsg.h"
#include "midi_router.h"
#include "voicing.h"
#include "rhythm.h"


namespace {

// How long to defer re-resolution after a key release (microseconds). Gives a
// human enough time to lift both keys of a combination before the remaining
// key's chord takes over. Tunable; 50ms is imperceptible for chord changes.
constexpr uint64_t RELEASE_BUFFER_US = 25000;

} // namespace


ChordEngine::ChordEngine(StateManager& state, MidiRouter& router)
    : state_(state), router_(router), keymap_(&state.keymap) {}

void ChordEngine::handleKeyEvent(const KeyEvent& ev, uint64_t now_us) {
    KeyAction a = keymap_.resolve(ev.hid_usage, ev.modifiers);

    switch (a.cmd) {
        case KeyCmd::ChordKey:
            if (ev.pressed) { addCell(a.cell); onPress(now_us); }
            else            { removeCell(a.cell); onRelease(now_us); }
            break;

        case KeyCmd::Backtick:
            backtickHeld_ = ev.pressed;
            if (ev.pressed) onPress(now_us);
            else            onRelease(now_us);
            break;

        case KeyCmd::Ext9:   setExt(9,  ev.pressed, now_us); break;
        case KeyCmd::Ext11:  setExt(11, ev.pressed, now_us); break;
        case KeyCmd::Ext13:  setExt(13, ev.pressed, now_us); break;

        default:
            break;
    }
}

// Held extensions (FR-C7): Left/Down/Right add add9/add11/add13 to the
// currently-sounding chord for as long as the arrow is held. The extension
// latches onto the current chord (including a Held-mode chord whose keys have
// already been released). When a chord is already sounding, only the changed
// note(s) are added/removed so the base chord is not re-attacked.
void ChordEngine::setExt(int which, bool held, uint64_t now_us) {
    bool* flag = nullptr;
    switch (which) {
        case 9:  flag = &ext9Held_;  break;
        case 11: flag = &ext11Held_; break;
        case 13: flag = &ext13Held_; break;
        default: return;
    }
    if (*flag == held) return;
    *flag = held;

    // Re-resolve the base chord (the grid keys may still be held) and merge the
    // held extension flags over it.
    ResolvedChord base;
    if (resolveChord(heldCells_, backtickHeld_, base)) {
        baseChord_ = base;
        baseChordValid_ = true;
    }

    if (!baseChordValid_) return;

    ResolvedChord merged = baseChord_;
    merged.add9  = merged.add9  || ext9Held_;
    merged.add11 = merged.add11 || ext11Held_;
    merged.add13 = merged.add13 || ext13Held_;

    state_.selectedChord = merged;
    state_.selectedChordValid = true;

    // When a chord is already sounding, add/remove only the changed extension
    // note(s) so the base chord is not re-attacked on extension press/release.
    // Arpeggio rebuilds its step sequence in place instead of restarting.
    // Otherwise (Silent, chord roll) fall back to a full re-trigger.
    if (arpActive_) {
        applyArpExtension(merged, now_us);
    } else if (sounding_ && rollPending_.empty() &&
               (state_.activeChord.play_mode == PlayMode::Held ||
                state_.activeChord.play_mode == PlayMode::PressToPlay)) {
        applyExtensionMerge(merged, now_us);
    } else {
        triggerChord(merged, now_us);
    }
}

// Compute the voiced base chord (no extensions) plus the extension tensions
// appended above it. Splitting them keeps extension toggles from re-running the
// min-interval spread (which would re-assign and re-attack the base notes).
void ChordEngine::computeVoicing(const ResolvedChord& chord,
                                 std::vector<uint8_t>& base,
                                 std::vector<uint8_t>& combined) {
    const ChordParams& p = state_.activeChord;
    const AppConfig& cfg = state_.config;

    ResolvedChord baseChord = chord;
    baseChord.add9 = baseChord.add11 = baseChord.add13 = false;

    VoicingConfig vc;
    vc.voicing_mode = p.voicing_mode;
    vc.inversion    = p.inversion;
    vc.min_notes    = p.min_notes;
    vc.min_interval = p.min_interval;

    base = voiceChord(baseChord, cfg.base_root_midi, p.octave,
                      cfg.note_range_low, cfg.note_range_high, vc, prevVoicing_);

    // Extension tensions: the notes in the full root-position set that are not
    // part of the base chord's intervals.
    int root = voicingRoot(chord.rootPc, cfg.base_root_midi, p.octave);
    std::vector<uint8_t> full = chordNotes(chord, root);
    std::vector<uint8_t> baseRP = chordNotes(baseChord, root);

    combined = base;
    for (uint8_t n : full) {
        if (std::find(baseRP.begin(), baseRP.end(), n) == baseRP.end()) {
            combined.push_back(n);
        }
    }
    std::sort(combined.begin(), combined.end());
    combined.erase(std::unique(combined.begin(), combined.end()), combined.end());
}

// Diff-based extension update: voice the merged chord and note-off/note-on only
// the notes that changed, leaving the sustained base chord untouched.
void ChordEngine::applyExtensionMerge(const ResolvedChord& chord, uint64_t now_us) {
    const ChordParams& p = state_.activeChord;

    std::vector<uint8_t> base, notes;
    computeVoicing(chord, base, notes);

    for (uint8_t n : activeNotes_) {
        if (std::find(notes.begin(), notes.end(), n) == notes.end()) {
            router_.noteOff(activeChannel_, n);
        }
    }
    for (uint8_t n : notes) {
        if (std::find(activeNotes_.begin(), activeNotes_.end(), n) == activeNotes_.end()) {
            router_.noteOn(activeChannel_, n, p.velocity);
        }
    }

    prevVoicing_ = base;
    voicing_ = notes;
    activeNotes_ = notes;
    currentChord_ = chord;
    currentChordValid_ = true;
}

// Extension change during arpeggio: rebuild the arp sequence in place (add/remove
// the extension note) without restarting or re-attacking the held chord.
void ChordEngine::applyArpExtension(const ResolvedChord& chord, uint64_t now_us) {
    const ChordParams& p = state_.activeChord;

    std::vector<uint8_t> base, notes;
    computeVoicing(chord, base, notes);

    prevVoicing_ = base;
    voicing_ = notes;
    currentChord_ = chord;
    currentChordValid_ = true;

    if (!voicing_.empty()) {
        arpSeq_ = arpSequence(p.arp_mode, voicing_.size());
        if (!arpSeq_.empty()) arpPos_ = arpPos_ % arpSeq_.size();
    }

    // If the currently-sounding note was removed (extension released), replace it
    // with the note at the current arp position.
    if (!activeNotes_.empty()) {
        uint8_t sounding = activeNotes_[0];
        bool stillVoiced = std::find(voicing_.begin(), voicing_.end(), sounding) != voicing_.end();
        if (!stillVoiced && !voicing_.empty()) {
            for (uint8_t n : activeNotes_) router_.noteOff(activeChannel_, n);
            activeNotes_.clear();
            size_t idx = arpSeq_.empty() ? 0 : arpSeq_[arpPos_ % arpSeq_.size()];
            uint8_t note = voicing_[idx];
            router_.noteOn(activeChannel_, note, p.velocity);
            activeNotes_.push_back(note);
        }
    }
}

void ChordEngine::onModeChanged() {
    PlayMode mode = state_.pendingChord.play_mode;
    if (mode != PlayMode::Held && heldCells_.empty()) {
        releaseChord();
    }
}

void ChordEngine::onArpModeChanged() {
    // Adopt the newly-edited mode onto the running arpeggio (the active chord is
    // a snapshot taken at trigger time, so its arp_mode is otherwise stale).
    state_.activeChord.arp_mode = state_.pendingChord.arp_mode;
    if (!arpActive_ || voicing_.empty()) return;
    arpSeq_ = arpSequence(state_.activeChord.arp_mode, voicing_.size());
    arpPos_ = 0;
}

void ChordEngine::resetVoicing() {
    prevVoicing_.clear();
}

void ChordEngine::setPatterns(std::vector<ChordPattern> patterns) {
    patterns_ = std::move(patterns);
    if (!patterns_.empty() && state_.pendingChord.pattern >= patterns_.size()) {
        state_.pendingChord.pattern = 0;
    }
}

const ChordPattern* ChordEngine::patternAt(int index) const {
    if (index < 0 || index >= static_cast<int>(patterns_.size())) return nullptr;
    return &patterns_[index];
}

const ChordPattern* ChordEngine::currentPattern() const {
    int idx = state_.pendingChord.pattern;
    if (idx < 0 || idx >= static_cast<int>(patterns_.size())) return nullptr;
    return &patterns_[idx];
}

void ChordEngine::beginPattern(uint64_t now_us) {
    patternActive_ = true;
    patternStep_ = 0;
    lastPatternClockStep_ = state_.rhythmClock.stepAbs;
    patternNextUs_ = now_us + stepUs(state_.pendingRhythm.tempo);

    const ChordPattern* pat = currentPattern();
    int spb = pat ? pat->steps_per_bar : 16;
    int rhythmSpb = state_.rhythmClock.stepsPerBar.load();
    uint32_t effective = static_cast<uint32_t>(
        (rhythmSpb > 0 && rhythmSpb < spb) ? rhythmSpb : spb);
    if (effective == 0) effective = 16;
    firePatternStep(0, now_us, effective);
}

void ChordEngine::firePatternStep(int stepInBar, uint64_t now_us, uint32_t effectiveSteps) {
    if (!currentChordValid_) return;

    const ChordPattern* pat = currentPattern();
    if (!pat) return;
    if (stepInBar < 0 || stepInBar >= static_cast<int>(pat->steps.size())) return;

    uint8_t mask = pat->steps[stepInBar];
    if (mask == 0) return;  // rest: keep the current notes sustaining

    const ChordParams& p = state_.activeChord;
    releasePatternNotes();

    std::vector<int> offsets = chordPatternOffsets(currentChord_.type, mask);
    int root = rootMidi(currentChord_.rootPc, state_.config.base_root_midi, p.octave);
    for (int off : offsets) {
        int note = root + off;
        if (note < 0) note = 0;
        if (note > 127) note = 127;
        router_.noteOn(activeChannel_, static_cast<uint8_t>(note), p.velocity);
        activeNotes_.push_back(static_cast<uint8_t>(note));
    }

    int sustain = chordPatternStepSustain(*pat, stepInBar);
    if (sustain > static_cast<int>(effectiveSteps)) sustain = static_cast<int>(effectiveSteps);
    if (sustain > 0) {
        patternOffUs_ = now_us +
            static_cast<uint64_t>(stepUs(state_.pendingRhythm.tempo)) *
            static_cast<uint64_t>(sustain);
    } else {
        patternOffUs_ = now_us + static_cast<uint64_t>(p.note_duration_ms) * 1000ULL;
    }
}

void ChordEngine::releasePatternNotes() {
    for (uint8_t n : activeNotes_) {
        router_.noteOff(activeChannel_, n);
    }
    activeNotes_.clear();
}

void ChordEngine::update(uint64_t now_us) {
    // Chord roll (VR-9): fire note-ons as their stagger deadlines pass.
    if (!rollPending_.empty()) {
        auto it = rollPending_.begin();
        while (it != rollPending_.end() && it->deadline <= now_us) {
            router_.noteOn(activeChannel_, it->note, state_.activeChord.velocity);
            activeNotes_.push_back(it->note);
            it = rollPending_.erase(it);
        }
    }

    if (arpActive_) {
        if (followRhythmClock()) {
            uint32_t stepAbs = state_.rhythmClock.stepAbs;
            if (stepAbs != lastRhythmStep_) {
                lastRhythmStep_ = stepAbs;
                stepArpeggio(now_us);
            }
        } else if (now_us >= arpNext_us_) {
            stepArpeggio(now_us);
        }
    }

    if (patternActive_) {
        // Release sustained pattern notes whose deadline passed (covers rests and
        // the Block pattern; percussive steps release between steps).
        if (!activeNotes_.empty() && now_us >= patternOffUs_) {
            releasePatternNotes();
        }

        const ChordPattern* pat = currentPattern();
        int spb = pat ? pat->steps_per_bar : 16;
        int rhythmSpb = state_.rhythmClock.stepsPerBar.load();
        uint32_t effective = static_cast<uint32_t>(
            (rhythmSpb > 0 && rhythmSpb < spb) ? rhythmSpb : spb);
        if (effective == 0) effective = 16;

        if (followRhythmClock()) {
            uint32_t stepAbs = state_.rhythmClock.stepAbs;
            if (stepAbs != lastPatternClockStep_) {
                lastPatternClockStep_ = stepAbs;
                patternStep_ = (patternStep_ + 1) % effective;
                firePatternStep(static_cast<int>(patternStep_), now_us, effective);
            }
        } else if (now_us >= patternNextUs_) {
            patternStep_ = (patternStep_ + 1) % effective;
            firePatternStep(static_cast<int>(patternStep_), now_us, effective);
            patternNextUs_ = now_us + stepUs(state_.pendingRhythm.tempo);
        }
    }

    if (releaseBufferPending_ && now_us >= releaseBufferDeadlineUs_) {
        releaseBufferPending_ = false;
        resolveAndTriggerIfChanged(now_us, false);
    }
}

bool ChordEngine::followRhythmClock() const {
    return state_.rhythmClock.running &&
           (state_.activeChord.play_mode == PlayMode::Arpeggio ||
            state_.activeChord.play_mode == PlayMode::ArpHold ||
            state_.activeChord.play_mode == PlayMode::Pattern ||
            state_.activeChord.play_mode == PlayMode::PatternHold);
}

void ChordEngine::allNotesOff() {
    stopSound();
    prevVoicing_.clear();
    currentChordValid_ = false;
    baseChordValid_ = false;
    releaseBufferPending_ = false;
    pendingSwitch_ = false;
}

void ChordEngine::addCell(const GridCell& cell) {
    for (const auto& c : heldCells_) {
        if (c.quality == cell.quality && c.column == cell.column) return;
    }
    heldCells_.push_back(cell);
}

void ChordEngine::removeCell(const GridCell& cell) {
    for (auto it = heldCells_.begin(); it != heldCells_.end(); ++it) {
        if (it->quality == cell.quality && it->column == cell.column) {
            heldCells_.erase(it);
            return;
        }
    }
}

void ChordEngine::onPress(uint64_t now_us) {
    releaseBufferPending_ = false;
    resolveAndTriggerIfChanged(now_us, true);
}

void ChordEngine::onRelease(uint64_t now_us) {
    // A pending switch (an overlapped new-chord press that formed an invalid
    // combination) re-resolves immediately once the stale key is lifted, in
    // every play mode, so the new chord lands without the release-buffer delay.
    if (pendingSwitch_) {
        pendingSwitch_ = false;
        resolveAndTriggerIfChanged(now_us, false);
        return;
    }
    if (state_.activeChord.play_mode == PlayMode::Held ||
        state_.activeChord.play_mode == PlayMode::ArpHold ||
        state_.activeChord.play_mode == PlayMode::PatternHold) {
        return;
    }
    releaseBufferPending_ = true;
    releaseBufferDeadlineUs_ = now_us + RELEASE_BUFFER_US;
}

void ChordEngine::resolveAndTriggerIfChanged(uint64_t now_us, bool force) {
    ResolvedChord chord;
    if (resolveChord(heldCells_, backtickHeld_, chord)) {
        pendingSwitch_ = false;
        baseChord_ = chord;
        baseChordValid_ = true;

        ResolvedChord merged = chord;
        merged.add9  = merged.add9  || ext9Held_;
        merged.add11 = merged.add11 || ext11Held_;
        merged.add13 = merged.add13 || ext13Held_;

        // Record the currently selected chord for the strum plate (FR-S4), with
        // any held extensions already merged in.
        state_.selectedChord = merged;
        state_.selectedChordValid = true;

        if (force || chordChanged(merged)) {
            triggerChord(merged, now_us);
        }
        return;
    }

    if (heldCells_.empty()) {
        // All keys released. Held/ArpHold/PatternHold sustain; momentary modes release.
        if (state_.activeChord.play_mode == PlayMode::Held ||
            state_.activeChord.play_mode == PlayMode::ArpHold ||
            state_.activeChord.play_mode == PlayMode::PatternHold) {
            return;
        }
        releaseChord();
        return;
    }

    // Keys still held but no valid combination: keep the current chord. Remember
    // the intent so the chord can switch once the stale key is released.
    if (currentChordValid_) {
        pendingSwitch_ = true;
    }
}

void ChordEngine::triggerChord(const ResolvedChord& chord, uint64_t now_us) {
    state_.snapshotChord();
    state_.lastChordUs = now_us;
    const ChordParams& p = state_.activeChord;

    std::vector<uint8_t> base, notes;
    computeVoicing(chord, base, notes);
    prevVoicing_ = base;

    stopSound();
    activeChannel_ = p.channel;
    voicing_ = notes;

    currentChord_ = chord;
    currentChordValid_ = true;

    if (notes.empty()) {
        sounding_ = false;
        return;
    }

    switch (p.play_mode) {
        case PlayMode::Silent:
            sounding_ = false;
            break;

        case PlayMode::Arpeggio:
        case PlayMode::ArpHold:
            sounding_ = true;
            router_.cc(p.channel, midi::CC_PAN, p.pan);
            beginArp(now_us);
            break;

        case PlayMode::Pattern:
        case PlayMode::PatternHold:
            sounding_ = true;
            router_.cc(p.channel, midi::CC_PAN, p.pan);
            beginPattern(now_us);
            break;

        case PlayMode::Held:
        case PlayMode::PressToPlay:
        default:
            sounding_ = true;
            router_.cc(p.channel, midi::CC_PAN, p.pan);
            if (p.chord_roll_ms == 0) {
                activeNotes_ = notes;
                for (uint8_t n : notes) {
                    router_.noteOn(p.channel, n, p.velocity);
                }
            } else {
                activeNotes_.clear();
                scheduleRoll(notes, now_us);
            }
            break;
    }
}

// Staggered chord note-ons (VR-9): positive ascends, negative descends;
// note-offs still release together via stopSound().
void ChordEngine::scheduleRoll(const std::vector<uint8_t>& notes, uint64_t now_us) {
    // chord_roll_ms is milliseconds; convert to microseconds before staggering.
    uint64_t stepUs = static_cast<uint64_t>(
        std::abs(static_cast<int>(state_.activeChord.chord_roll_ms))) * 1000ULL;
    bool ascending = state_.activeChord.chord_roll_ms > 0;

    std::vector<uint8_t> order = notes;
    if (!ascending) std::reverse(order.begin(), order.end());

    rollPending_.clear();
    for (size_t i = 0; i < order.size(); i++) {
        rollPending_.push_back({order[i], now_us + stepUs * i});
    }
}

std::vector<int> ChordEngine::arpSequence(ArpMode mode, size_t n) {
    std::vector<int> seq;
    seq.reserve(n);
    if (n == 0) return seq;

    switch (mode) {
        case ArpMode::Down:
            for (size_t i = n; i-- > 0;) seq.push_back(static_cast<int>(i));
            break;
        case ArpMode::UpDown:
            for (size_t i = 0; i < n; i++) seq.push_back(static_cast<int>(i));
            for (size_t i = n - 1; i-- > 1;) seq.push_back(static_cast<int>(i));
            break;
        case ArpMode::Alternating:
            // 1 -> 3 -> 2 -> 4 ... (indices 0,2,1,3 for 4 notes).
            for (size_t i = 0; i < n; i += 2) seq.push_back(static_cast<int>(i));
            for (size_t i = 1; i < n; i += 2) seq.push_back(static_cast<int>(i));
            break;
        case ArpMode::Random:
            // Handled specially in stepArpeggio; leave empty.
            break;
        case ArpMode::Up:
        default:
            for (size_t i = 0; i < n; i++) seq.push_back(static_cast<int>(i));
            break;
    }
    return seq;
}

void ChordEngine::beginArp(uint64_t now_us) {
    const ChordParams& p = state_.activeChord;
    arpSeq_ = arpSequence(p.arp_mode, voicing_.size());
    arpPos_ = 0;
    arpActive_ = true;
    lastRhythmStep_ = state_.rhythmClock.stepAbs;

    size_t idx;
    if (p.arp_mode == ArpMode::Random) {
        rngState_ = rngState_ * 1103515245u + 12345u;
        idx = rngState_ % voicing_.size();
    } else {
        idx = static_cast<size_t>(arpSeq_[0]);
    }

    uint8_t note = voicing_[idx];
    router_.noteOn(p.channel, note, p.velocity);
    activeNotes_.push_back(note);
    arpNext_us_ = now_us + stepUs(state_.pendingRhythm.tempo);
}

void ChordEngine::stepArpeggio(uint64_t now_us) {
    const ChordParams& p = state_.activeChord;
    if (voicing_.empty()) {
        arpActive_ = false;
        return;
    }

    size_t idx;
    if (p.arp_mode == ArpMode::Random) {
        rngState_ = rngState_ * 1103515245u + 12345u;
        idx = rngState_ % voicing_.size();
    } else {
        // The sequence should match the current mode, but a mode switch out of
        // Random (or an extension edit while in Random) can leave arpSeq_ empty.
        // Rebuild defensively so the modulo below can never divide by zero.
        if (arpSeq_.empty()) {
            arpSeq_ = arpSequence(p.arp_mode, voicing_.size());
            arpPos_ = 0;
            if (arpSeq_.empty()) { arpActive_ = false; return; }
        }
        arpPos_ = (arpPos_ + 1) % arpSeq_.size();
        idx = static_cast<size_t>(arpSeq_[arpPos_]);
    }

    for (uint8_t n : activeNotes_) {
        router_.noteOff(p.channel, n);
    }
    activeNotes_.clear();

    uint8_t note = voicing_[idx];
    router_.noteOn(p.channel, note, p.velocity);
    activeNotes_.push_back(note);
    arpNext_us_ = now_us + stepUs(state_.pendingRhythm.tempo);
}

void ChordEngine::releaseChord() {
    stopSound();
    currentChordValid_ = false;
    baseChordValid_ = false;
    pendingSwitch_ = false;
}

void ChordEngine::stopSound() {
    for (uint8_t n : activeNotes_) {
        router_.noteOff(activeChannel_, n);
    }
    activeNotes_.clear();
    rollPending_.clear();
    sounding_ = false;
    arpActive_ = false;
    patternActive_ = false;
}

bool ChordEngine::chordChanged(const ResolvedChord& chord) const {
    if (!currentChordValid_) return true;
    return chord.rootPc != currentChord_.rootPc
        || chord.type   != currentChord_.type
        || chord.add9   != currentChord_.add9
        || chord.add11  != currentChord_.add11
        || chord.add13  != currentChord_.add13;
}

namespace {

// Built-in fallback pattern: full chord held for the bar. Used when no pattern
// JSON can be loaded from storage.
ChordPattern builtinBlockPattern() {
    ChordPattern p;
    p.name = "Block";
    p.steps_per_bar = 16;
    p.steps = {CHORD_MASK_SEVENTH,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};
    p.sustain_steps = {16,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};
    return p;
}

} // namespace

ChordPatternLibrary loadChordPatterns(StorageAdapter& storage) {
    ChordPatternLibrary lib;
    lib.patterns.reserve(CHORD_PATTERN_COUNT + 4);
    lib.names.reserve(CHORD_PATTERN_COUNT + 4);

    // 1. Built-in patterns, in fixed order (indices 0..CHORD_PATTERN_COUNT-1).
    // A missing or unparseable file is re-provisioned from the embedded template
    // so indices stay stable and presets that store a built-in index keep
    // pointing at the right pattern.
    for (int i = 0; i < CHORD_PATTERN_COUNT; i++) {
        std::string path = "/chordpatterns/" + std::string(chordPatternFileName(i));
        std::string raw;
        if (storage.exists(path)) raw = storage.readFile(path);

        ChordPattern p;
        if (!parseChordPattern(raw, p)) {
            const char* tmpl = defaultChordPatternJson(i);
            raw = tmpl ? tmpl : "";
            storage.writeFile(path, raw);
            parseChordPattern(raw, p);
        }
        lib.patterns.push_back(p);
        lib.names.push_back(p.name.empty() ? chordPatternName(i) : p.name);
    }

    // 2. User-provided patterns: any other *.json in /chordpatterns/, sorted by
    // filename. Appended at indices CHORD_PATTERN_COUNT.. up to MAX_CHORD_PATTERNS.
    auto files = storage.listFiles("/chordpatterns");
    std::sort(files.begin(), files.end());
    for (const auto& f : files) {
        if (static_cast<int>(lib.patterns.size()) >= MAX_CHORD_PATTERNS) break;
        if (f.size() < 5 || f.compare(f.size() - 5, 5, ".json") != 0) continue;
        bool builtin = false;
        for (int i = 0; i < CHORD_PATTERN_COUNT; i++) {
            if (f == chordPatternFileName(i)) { builtin = true; break; }
        }
        if (builtin) continue;

        std::string raw = storage.readFile("/chordpatterns/" + f);
        ChordPattern p;
        if (!parseChordPattern(raw, p) || !p.valid()) continue;
        lib.patterns.push_back(p);
        lib.names.push_back(p.name.empty() ? f.substr(0, f.size() - 5) : p.name);
    }

    if (lib.patterns.empty()) {
        lib.patterns.push_back(builtinBlockPattern());
        lib.names.push_back("Block");
    }
    return lib;
}
