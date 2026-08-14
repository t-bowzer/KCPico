#pragma once

#include <cstdint>
#include <vector>
#include "params.h"
#include "presets.h"
#include "config.h"


struct ActiveNote {
    uint8_t note;
    uint8_t channel;
};

class StateManager {
public:
    ChordParams  pendingChord;
    StrumParams  pendingStrum;
    RhythmParams pendingRhythm;

    ChordParams  activeChord;
    StrumParams  activeStrum;
    RhythmParams activeRhythm;

    std::vector<ActiveNote> activeNotes;

    int  currentBank = 0;
    int  currentSlot = 0;
    bool dirty       = false;

    AppConfig config;

    StateManager();

    void snapshotActive();
    void snapshotChord();

    void noteOn(uint8_t channel, uint8_t note);
    void noteOff(uint8_t channel, uint8_t note);
    void allNotesOff();

    bool isNoteActive(uint8_t channel, uint8_t note) const;

    void loadDefaults();

private:
    void removeNote(uint8_t channel, uint8_t note);
};


