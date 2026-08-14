#pragma once

#include <cstdint>
#include <vector>
#include "params.h"
#include "presets.h"
#include "config.h"
#include "chords.h"


struct ActiveNote {
    uint8_t note;
    uint8_t channel;
};

// Which parameter the +/- keys currently edit (context-sensitive). None means
// +/- acts on chord octave. Cleared by Esc.
enum class EditTarget : uint8_t {
    None = 0,
    StrumOctave,
    StrumDuration,
    StrumVelocity,
};

class StateManager {
public:
    ChordParams  pendingChord;
    StrumParams  pendingStrum;
    RhythmParams pendingRhythm;

    ChordParams  activeChord;
    StrumParams  activeStrum;
    RhythmParams activeRhythm;

    // Currently selected chord (root/type/extensions from the latest resolve),
    // independent of Held-mode latching. This is what the strum plate derives
    // its note pool from (FR-S4 / FR-S5), including in Silent mode.
    ResolvedChord selectedChord;
    bool           selectedChordValid = false;

    EditTarget editTarget = EditTarget::None;

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


