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

void StrumEngine::handleKeyEvent(const KeyEvent& ev, uint64_t now_us) {
    KeyAction a = keymap_.resolve(ev.hid_usage, ev.modifiers);

    switch (a.type) {
        case ActionType::StrumKey:
            if (ev.pressed) playStrum(ev.hid_usage, now_us);
            break;

        case ActionType::StrumOctave:
            if (ev.pressed) state_.editTarget = EditTarget::StrumOctave;
            break;
        case ActionType::StrumDuration:
            if (ev.pressed) state_.editTarget = EditTarget::StrumDuration;
            break;
        case ActionType::StrumVelocity:
            if (ev.pressed) state_.editTarget = EditTarget::StrumVelocity;
            break;

        case ActionType::LimitedToggle:
            if (ev.pressed) {
                state_.pendingStrum.limited_keys = !state_.pendingStrum.limited_keys;
            }
            break;

        case ActionType::ClearEdit:
            if (ev.pressed) state_.editTarget = EditTarget::None;
            break;

        case ActionType::OctaveUp:
            if (ev.pressed && state_.editTarget != EditTarget::None) stepEdit(true);
            break;
        case ActionType::OctaveDown:
            if (ev.pressed && state_.editTarget != EditTarget::None) stepEdit(false);
            break;

        default:
            break;
    }
}

void StrumEngine::update(uint64_t now_us) {
    for (auto& n : notes_) {
        if (n.active && now_us >= n.deadline_us) {
            router_.noteOff(n.channel, n.note);
            n.active = false;
        }
    }
}

void StrumEngine::allNotesOff() {
    for (auto& n : notes_) {
        if (n.active) {
            router_.noteOff(n.channel, n.note);
            n.active = false;
        }
    }
}

void StrumEngine::playStrum(uint8_t usage, uint64_t now_us) {
    StrumLayout layout = activeLayout();
    int idx = strumIndexFor(usage, layout);
    if (idx < 0 || !state_.selectedChordValid) return;

    // Immediate pickup (FR-S5): read current strum params directly (no latching)
    // and merge any independently-toggled chord extensions over the selected
    // chord so the pool reflects the latest edits.
    const StrumParams& sp = state_.pendingStrum;

    ResolvedChord chord = state_.selectedChord;
    chord.add9  = chord.add9  || state_.pendingChord.add9;
    chord.add11 = chord.add11 || state_.pendingChord.add11;
    chord.add13 = chord.add13 || state_.pendingChord.add13;

    auto pool = buildNotePool(chord, state_.config.base_root_midi, sp.octave,
                              static_cast<size_t>(strumKeyCount(layout)));
    if (idx >= static_cast<int>(pool.size())) return;

    uint8_t note = pool[idx];

    // Re-articulate an already-sounding note: send its note-off before the new
    // note-on so a re-strum uses the current note duration (FR-S5) and the synth
    // never sees an un-paired duplicate note-on (which can hold the old duration
    // or hang on some synths).
    releaseNote(sp.channel, note);

    logStrumPlay(static_cast<uint8_t>(idx), note, sp.channel, sp.note_duration_ms);
    router_.noteOn(sp.channel, note, sp.velocity);
    addSoundingNote(sp.channel, note,
                    now_us + static_cast<uint64_t>(sp.note_duration_ms) * 1000ULL);
}

void StrumEngine::stepEdit(bool up) {
    int8_t delta = up ? 1 : -1;
    switch (state_.editTarget) {
        case EditTarget::StrumOctave:
            state_.pendingStrum.octave = clamp<int8_t>(
                state_.pendingStrum.octave + delta,
                param_bounds::STRUM_OCTAVE_MIN, param_bounds::STRUM_OCTAVE_MAX);
            logStrumEdit("octave", state_.pendingStrum.octave);
            break;
        case EditTarget::StrumDuration:
            state_.pendingStrum.note_duration_ms = clamp<int16_t>(
                state_.pendingStrum.note_duration_ms + static_cast<int16_t>(delta * 50),
                param_bounds::STRUM_NOTE_DURATION_MIN, param_bounds::STRUM_NOTE_DURATION_MAX);
            logStrumEdit("duration", state_.pendingStrum.note_duration_ms);
            break;
        case EditTarget::StrumVelocity:
            state_.pendingStrum.velocity = clamp<uint8_t>(
                static_cast<int>(state_.pendingStrum.velocity) + delta,
                param_bounds::STRUM_VELOCITY_MIN, param_bounds::STRUM_VELOCITY_MAX);
            logStrumEdit("velocity", state_.pendingStrum.velocity);
            break;
        default:
            break;
    }
}

void StrumEngine::releaseNote(uint8_t channel, uint8_t note) {
    for (auto& n : notes_) {
        if (n.active && n.channel == channel && n.note == note) {
            router_.noteOff(n.channel, n.note);
            n.active = false;
            return;
        }
    }
}

void StrumEngine::addSoundingNote(uint8_t channel, uint8_t note, uint64_t deadline_us) {
    // First free slot.
    for (auto& n : notes_) {
        if (!n.active) {
            n.note        = note;
            n.channel     = channel;
            n.deadline_us = deadline_us;
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
    router_.noteOff(notes_[earliest].channel, notes_[earliest].note);
    notes_[earliest].note        = note;
    notes_[earliest].channel     = channel;
    notes_[earliest].deadline_us = deadline_us;
}
