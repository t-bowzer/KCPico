#include "chord_engine.h"

#include <algorithm>

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
    : state_(state), router_(router) {}

void ChordEngine::handleKeyEvent(const KeyEvent& ev, uint64_t now_us) {
    KeyAction a = keymap_.resolve(ev.hid_usage, ev.modifiers);

    switch (a.type) {
        case ActionType::ChordKey:
            if (ev.pressed) { addCell(a.cell); onPress(now_us); }
            else            { removeCell(a.cell); onRelease(now_us); }
            break;

        case ActionType::Backtick:
            backtickHeld_ = ev.pressed;
            if (ev.pressed) onPress(now_us);
            else            onRelease(now_us);
            break;

        case ActionType::Ext9:   setExt(9,  ev.pressed, now_us); break;
        case ActionType::Ext11:  setExt(11, ev.pressed, now_us); break;
        case ActionType::Ext13:  setExt(13, ev.pressed, now_us); break;

        default:
            break;
    }
}

// Held extensions (FR-C7): Left/Down/Right add add9/add11/add13 to the
// currently-sounding chord for as long as the arrow is held. The extension
// latches onto the current chord (re-triggers it), including a Held-mode chord
// whose keys have already been released.
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

    // Force a re-trigger so the extension is audible immediately, even on a
    // latched Held-mode chord (no keys held).
    triggerChord(merged, now_us);
}

void ChordEngine::onModeChanged() {
    PlayMode mode = state_.pendingChord.play_mode;
    if (mode != PlayMode::Held && heldCells_.empty()) {
        releaseChord();
    }
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
            if (state_.rhythmClock.running &&
                state_.rhythmClock.stepAbs != lastRhythmStep_) {
                lastRhythmStep_ = state_.rhythmClock.stepAbs;
                stepArpeggio(now_us);
            }
        } else if (now_us >= arpNext_us_) {
            stepArpeggio(now_us);
        }
    }

    if (releaseBufferPending_ && now_us >= releaseBufferDeadlineUs_) {
        releaseBufferPending_ = false;
        resolveAndTriggerIfChanged(now_us, false);
    }
}

bool ChordEngine::followRhythmClock() const {
    return state_.pendingRhythm.enabled &&
           state_.activeChord.play_mode == PlayMode::Arpeggio;
}

void ChordEngine::allNotesOff() {
    stopSound();
    prevVoicing_.clear();
    currentChordValid_ = false;
    baseChordValid_ = false;
    releaseBufferPending_ = false;
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
    if (state_.activeChord.play_mode == PlayMode::Held) {
        return;
    }
    releaseBufferPending_ = true;
    releaseBufferDeadlineUs_ = now_us + RELEASE_BUFFER_US;
}

void ChordEngine::resolveAndTriggerIfChanged(uint64_t now_us, bool force) {
    ResolvedChord chord;
    if (resolveChord(heldCells_, backtickHeld_, chord)) {
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
        // All keys released. Held sustains; momentary modes release.
        if (state_.activeChord.play_mode == PlayMode::Held) {
            return;
        }
        releaseChord();
        return;
    }

    // Keys still held but no valid combination: keep the current chord.
}

void ChordEngine::triggerChord(const ResolvedChord& chord, uint64_t now_us) {
    state_.snapshotChord();
    const ChordParams& p = state_.activeChord;
    const AppConfig& cfg = state_.config;

    VoicingConfig vc;
    vc.voicing_mode = p.voicing_mode;
    vc.inversion    = p.inversion;
    vc.min_notes    = p.min_notes;
    vc.min_interval = p.min_interval;

    std::vector<uint8_t> notes = voiceChord(chord, cfg.base_root_midi, p.octave,
                                            cfg.note_range_low, cfg.note_range_high,
                                            vc, prevVoicing_);
    prevVoicing_ = notes;

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
            sounding_ = true;
            router_.cc(p.channel, midi::CC_PAN, p.pan);
            beginArp(now_us);
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
    int step = std::abs(static_cast<int>(state_.activeChord.chord_roll_ms));
    bool ascending = state_.activeChord.chord_roll_ms > 0;

    std::vector<uint8_t> order = notes;
    if (!ascending) std::reverse(order.begin(), order.end());

    rollPending_.clear();
    for (size_t i = 0; i < order.size(); i++) {
        rollPending_.push_back({order[i], now_us + static_cast<uint64_t>(step) * i});
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
}

void ChordEngine::stopSound() {
    for (uint8_t n : activeNotes_) {
        router_.noteOff(activeChannel_, n);
    }
    activeNotes_.clear();
    rollPending_.clear();
    sounding_ = false;
    arpActive_ = false;
}

bool ChordEngine::chordChanged(const ResolvedChord& chord) const {
    if (!currentChordValid_) return true;
    return chord.rootPc != currentChord_.rootPc
        || chord.type   != currentChord_.type
        || chord.add9   != currentChord_.add9
        || chord.add11  != currentChord_.add11
        || chord.add13  != currentChord_.add13;
}
