#pragma once

#include <cstdint>
#include <vector>

#include "base.h"
#include "keymap.h"
#include "state.h"
#include "strum.h"

class MidiRouter;


class StrumEngine {
public:
    StrumEngine(StateManager& state, MidiRouter& router);

    void handleKeyEvent(const KeyEvent& ev, uint64_t now_us);
    void update(uint64_t now_us);
    void allNotesOff();

private:
    // A strummed note. note_duration_ms is a *minimum*: the note stays sounding
    // while its key is held, and is released at max(release, deadline).
    struct StrumNote {
        uint8_t  usage       = 0;
        uint8_t  note        = 0;
        uint8_t  channel     = 0;
        uint64_t deadline_us = 0;
        bool     active      = false;
        bool     released    = false;
    };

    static constexpr size_t MAX_STRUM_NOTES = 16;

    StateManager& state_;
    MidiRouter&   router_;
    KeymapResolver keymap_;

    StrumNote notes_[MAX_STRUM_NOTES];

    StrumLayout activeLayout() const;
    std::vector<uint8_t> notePool(StrumLayout layout, size_t count) const;
    void playStrum(uint8_t usage, uint64_t now_us);
    void releaseStrum(uint8_t usage, uint64_t now_us);
    void noteOff(StrumNote& n);
    void addSoundingNote(uint8_t usage, uint8_t channel, uint8_t note, uint64_t deadline_us);
};
