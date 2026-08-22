#pragma once

#include <cstdint>

#include "base.h"
#include "state.h"

class ChordEngine;
class MidiRouter;


// Walking bass (FR-B1..B4) + configurable patterns (Upgrade-Plan): fires on
// each rhythm beat edge as a short percussive note (or a sustained note for
// whole/half patterns) on its own channel. The `Hold` pattern sustains the root
// while the chord is sounding. Silent when the rhythm is not running (except
// Hold, which tracks the chord). Runs on Core 0, observing the Core 1 rhythm
// clock snapshot.
class BassEngine {
public:
    BassEngine(StateManager& state, MidiRouter& router);

    void update(uint64_t now_us);
    void allNotesOff();

    // Used by the Hold pattern to know whether the chord is currently sounding.
    void setChordEngine(const ChordEngine* chord);

private:
    StateManager& state_;
    MidiRouter&   router_;
    const ChordEngine* chord_ = nullptr;

    uint32_t lastStepAbs_ = 0;

    bool     noteActive_ = false;
    uint8_t  note_ = 0;
    uint64_t offDeadlineUs_ = 0;

    // Last-seen chord identity, to re-articulate sustained/hold notes when the
    // user plays a new chord mid-note.
    int       lastRootPc_ = 0;
    ChordType lastType_   = ChordType::Major;
    bool      lastChordValid_ = false;

    void fireBeat(uint32_t beat, uint64_t now_us, uint32_t beatsPerBar);
    void updateHold(uint64_t now_us);
    bool detectChordChange();
    void refireRoot(uint64_t now_us);
    void releaseNote();
    int  rootNote() const;
    uint64_t sustainDeadline(uint64_t now_us, uint32_t beat, uint32_t beatsPerBar) const;
};
