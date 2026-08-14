#include "chord_engine.h"

#include "midimsg.h"
#include "midi_router.h"
#include "voicing.h"


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

        case ActionType::PlayModeCycle:
            if (ev.pressed) {
                int next = (static_cast<int>(state_.pendingChord.play_mode) + 1)
                           % static_cast<int>(PlayMode::COUNT);
                state_.pendingChord.play_mode = static_cast<PlayMode>(next);

                // Leaving a sustaining mode (Held/Rhythm) with no chord key
                // held releases the latched chord; momentary modes won't keep
                // it sounding on their own.
                if (next != static_cast<int>(PlayMode::Held) &&
                    next != static_cast<int>(PlayMode::Rhythm) &&
                    heldCells_.empty()) {
                    releaseChord();
                }
            }
            break;

        case ActionType::VoicingToggle:
            if (ev.pressed) {
                state_.pendingChord.voicing_mode =
                    (state_.pendingChord.voicing_mode == VoicingMode::RootPosition)
                        ? VoicingMode::Smart
                        : VoicingMode::RootPosition;
            }
            break;

        case ActionType::ExtToggle9:
            if (ev.pressed) state_.pendingChord.add9 = !state_.pendingChord.add9;
            break;
        case ActionType::ExtToggle11:
            if (ev.pressed) state_.pendingChord.add11 = !state_.pendingChord.add11;
            break;
        case ActionType::ExtToggle13:
            if (ev.pressed) state_.pendingChord.add13 = !state_.pendingChord.add13;
            break;

        case ActionType::OctaveUp:
            if (ev.pressed) {
                state_.pendingChord.octave = clamp<int8_t>(
                    state_.pendingChord.octave + 1,
                    param_bounds::CHORD_OCTAVE_MIN,
                    param_bounds::CHORD_OCTAVE_MAX);
            }
            break;
        case ActionType::OctaveDown:
            if (ev.pressed) {
                state_.pendingChord.octave = clamp<int8_t>(
                    state_.pendingChord.octave - 1,
                    param_bounds::CHORD_OCTAVE_MIN,
                    param_bounds::CHORD_OCTAVE_MAX);
            }
            break;

        default:
            break;
    }
}

void ChordEngine::update(uint64_t now_us) {
    if (arpActive_ && now_us >= arpNext_us_) {
        stepArpeggio(now_us);
    }

    if (releaseBufferPending_ && now_us >= releaseBufferDeadlineUs_) {
        releaseBufferPending_ = false;
        resolveAndTriggerIfChanged(now_us, false);
    }
}

void ChordEngine::allNotesOff() {
    stopSound();
    prevVoicing_.clear();
    currentChordValid_ = false;
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

// A key press re-resolves immediately (cancels any pending release debounce).
void ChordEngine::onPress(uint64_t now_us) {
    releaseBufferPending_ = false;
    resolveAndTriggerIfChanged(now_us, true);
}

// A key release: in Held/Rhythm modes the chord latches — releasing keys does
// nothing, and the notes are released only when another chord is played
// (FR-C10). Momentary modes (press-to-play/arpeggio) debounce the release so a
// combination lifted "at once" doesn't glitch to the last-released key.
void ChordEngine::onRelease(uint64_t now_us) {
    if (state_.activeChord.play_mode == PlayMode::Held ||
        state_.activeChord.play_mode == PlayMode::Rhythm) {
        return;
    }
    releaseBufferPending_ = true;
    releaseBufferDeadlineUs_ = now_us + RELEASE_BUFFER_US;
}

void ChordEngine::resolveAndTriggerIfChanged(uint64_t now_us, bool force) {
    ResolvedChord chord;
    if (resolveChord(heldCells_, backtickHeld_, chord)) {
        // A fresh press always triggers (applies any pending param changes);
        // a release-buffer re-resolution only triggers if the chord changed.
        if (force || chordChanged(chord)) {
            triggerChord(chord, now_us);
        }
        return;
    }

    // No valid chord.
    if (heldCells_.empty()) {
        // All keys released. Held/Rhythm sustain; momentary modes release.
        if (state_.activeChord.play_mode == PlayMode::Held ||
            state_.activeChord.play_mode == PlayMode::Rhythm) {
            return;
        }
        releaseChord();
        return;
    }

    // Keys still held but no valid combination: keep the current chord.
}

void ChordEngine::triggerChord(const ResolvedChord& chord, uint64_t now_us) {
    // Latch: snapshot pending -> active; the sounding chord uses the snapshot
    // for its entire lifetime (FR-C9 / VR-5).
    state_.snapshotChord();
    const ChordParams& p = state_.activeChord;

    // Merge independently-toggled extensions (arrows) into the resolved chord.
    // The combination matrix may also set add9 (left-adjacent); both sources OR.
    ResolvedChord merged = chord;
    merged.add9  = merged.add9  || p.add9;
    merged.add11 = merged.add11 || p.add11;
    merged.add13 = merged.add13 || p.add13;

    const AppConfig& cfg = state_.config;
    std::vector<uint8_t> notes;
    if (p.voicing_mode == VoicingMode::Smart) {
        notes = voiceSmart(merged, cfg.base_root_midi, p.octave,
                           cfg.note_range_low, cfg.note_range_high, prevVoicing_);
    } else {
        notes = voiceRootPosition(merged, cfg.base_root_midi, p.octave,
                                  cfg.note_range_low, cfg.note_range_high);
    }
    prevVoicing_ = notes;

    // End whatever is currently sounding (using the channel it was played on).
    stopSound();
    activeChannel_ = p.channel;

    currentChord_ = merged;
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
            activeNotes_ = notes;
            arpIndex_ = 0;
            arpActive_ = true;
            router_.cc(p.channel, midi::CC_PAN, p.pan);
            router_.noteOn(p.channel, notes[0], p.velocity);
            arpNext_us_ = now_us + static_cast<uint64_t>(p.note_duration_ms) * 1000ULL;
            break;

        case PlayMode::Held:
        case PlayMode::PressToPlay:
        case PlayMode::Rhythm:  // M3: rhythm behaves as held until M5 clock sync
        default:
            sounding_ = true;
            activeNotes_ = notes;
            router_.cc(p.channel, midi::CC_PAN, p.pan);
            for (uint8_t n : notes) {
                router_.noteOn(p.channel, n, p.velocity);
            }
            break;
    }
}

void ChordEngine::releaseChord() {
    stopSound();
    currentChordValid_ = false;
}

void ChordEngine::stepArpeggio(uint64_t now_us) {
    const ChordParams& p = state_.activeChord;
    if (activeNotes_.empty()) {
        arpActive_ = false;
        return;
    }

    router_.noteOff(p.channel, activeNotes_[arpIndex_]);
    arpIndex_ = (arpIndex_ + 1) % activeNotes_.size();
    router_.noteOn(p.channel, activeNotes_[arpIndex_], p.velocity);
    arpNext_us_ = now_us + static_cast<uint64_t>(p.note_duration_ms) * 1000ULL;
}

void ChordEngine::stopSound() {
    for (uint8_t n : activeNotes_) {
        router_.noteOff(activeChannel_, n);
    }
    activeNotes_.clear();
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

