#pragma once

#include <cstdint>
#include "base.h"
#include "state.h"


class MidiRouter {
public:
    explicit MidiRouter(MidiOutAdapter& out, StateManager& state);

    void noteOn(uint8_t channel, uint8_t note, uint8_t velocity);
    void noteOff(uint8_t channel, uint8_t note);
    void cc(uint8_t channel, uint8_t control, uint8_t value);
    void allNotesOff(uint8_t channel);
    void allSoundOff(uint8_t channel);

    void sendTestNote();

private:
    MidiOutAdapter& midiOut_;
    StateManager& state_;

    void send(const MidiMessage& msg);
};


