#pragma once

#include <cstdint>

#include "base.h"
#include "state.h"

class MidiRouter;


// Walking bass (FR-B1..B4): fires the interval-cycle blueprint on each rhythm
// beat edge, as a short percussive note on its own channel. Silent when the
// rhythm is not running. Runs on Core 0, observing the Core 1 rhythm clock
// snapshot.
class BassEngine {
public:
    BassEngine(StateManager& state, MidiRouter& router);

    void update(uint64_t now_us);
    void allNotesOff();

private:
    StateManager& state_;
    MidiRouter&   router_;

    bool     wasRunning_ = false;
    uint32_t lastStepAbs_ = 0;

    bool     noteActive_ = false;
    uint8_t  note_ = 0;
    uint64_t offDeadlineUs_ = 0;

    void fireBeat(uint32_t beat, uint64_t now_us);
};
