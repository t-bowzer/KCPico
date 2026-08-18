#include "strum_engine.h"

#include "midimsg.h"
#include "midi_router.h"
#include "strum.h"
#include "debug_log.h"


StrumEngine::StrumEngine(StateManager& state, MidiRouter& router)
    : state_(state), router_(router) {}

StrumLayout StrumEngine::activeLayout() const {
    return state_.pendingStrum.limited_keys ? StrumLayout::Limited
                                            : StrumLayout::Full;
}

std::vector<uint8_t> StrumEngine::notePool(StrumLayout layout, size_t count) const {
    const StrumParams& sp = state_.pendingStrum;
    switch (sp.mode) {
        case StrumMode::Scale:
            return buildScalePool(sp.scale_type, sp.root_pc, state_.config.base_root_midi,
                                  sp.octave, count);
        case StrumMode::Piano:
            return buildPianoPool(sp.root_pc, state_.config.base_root_midi,
                                  sp.octave, count);
        case StrumMode::FollowChord:
        default: {
            // Immediate pickup (FR-S5): the selected chord already carries any
            // held extensions (merged in the chord engine), so the pool reflects
            // the latest edits with no latching.
            if (!state_.selectedChordValid) return {};
            return buildNotePool(state_.selectedChord, state_.config.base_root_midi,
                                 sp.octave, count);
        }
    }
}

void StrumEngine::handleKeyEvent(const KeyEvent& ev, uint64_t now_us) {
    KeyAction a = keymap_.resolve(ev.hid_usage, ev.modifiers);

    switch (a.type) {
        case ActionType::StrumKey:
            if (ev.pressed) playStrum(ev.hid_usage, now_us);
            else            releaseStrum(ev.hid_usage, now_us);
            break;

        default:
            break;
    }
}

void StrumEngine::update(uint64_t now_us) {
    for (auto& n : notes_) {
        if (n.active && n.released && now_us >= n.deadline_us) {
            noteOff(n);
        }
    }
}

void StrumEngine::allNotesOff() {
    for (auto& n : notes_) {
        if (n.active) noteOff(n);
    }
}

void StrumEngine::playStrum(uint8_t usage, uint64_t now_us) {
    StrumLayout layout = activeLayout();
    int idx = strumIndexFor(usage, layout);
    if (idx < 0) return;

    const StrumParams& sp = state_.pendingStrum;
    auto pool = notePool(layout, static_cast<size_t>(strumKeyCount(layout)));
    if (idx >= static_cast<int>(pool.size())) return;

    uint8_t note = pool[idx];

    // Re-articulate a note already sounding on this key (re-strum) so the new
    // press uses the current duration/velocity and the synth never sees an
    // un-paired duplicate note-on.
    for (auto& n : notes_) {
        if (n.active && n.usage == usage) {
            noteOff(n);
        }
    }

    logStrumPlay(static_cast<uint8_t>(idx), note, sp.channel, sp.note_duration_ms);
    router_.noteOn(sp.channel, note, sp.velocity);
    addSoundingNote(usage, sp.channel, note,
                    now_us + static_cast<uint64_t>(sp.note_duration_ms) * 1000ULL);
}

void StrumEngine::releaseStrum(uint8_t usage, uint64_t now_us) {
    for (auto& n : notes_) {
        if (n.active && n.usage == usage) {
            n.released = true;
            // If the minimum duration has already elapsed, release immediately;
            // otherwise update() releases it when the deadline passes.
            if (now_us >= n.deadline_us) {
                noteOff(n);
            }
        }
    }
}

void StrumEngine::noteOff(StrumNote& n) {
    router_.noteOff(n.channel, n.note);
    n.active = false;
    n.released = false;
}

void StrumEngine::addSoundingNote(uint8_t usage, uint8_t channel, uint8_t note,
                                  uint64_t deadline_us) {
    // First free slot.
    for (auto& n : notes_) {
        if (!n.active) {
            n.usage       = usage;
            n.note        = note;
            n.channel     = channel;
            n.deadline_us = deadline_us;
            n.released    = false;
            n.active      = true;
            return;
        }
    }

    // Overloaded: release the earliest-expiring note before reusing its slot so
    // nothing gets stuck when more than MAX_STRUM_NOTES notes overlap.
    size_t earliest = 0;
    for (size_t i = 1; i < MAX_STRUM_NOTES; i++) {
        if (notes_[i].deadline_us < notes_[earliest].deadline_us) earliest = i;
    }
    noteOff(notes_[earliest]);
    notes_[earliest].usage       = usage;
    notes_[earliest].note        = note;
    notes_[earliest].channel     = channel;
    notes_[earliest].deadline_us = deadline_us;
    notes_[earliest].released    = false;
    notes_[earliest].active      = true;
}
