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

    // Panic (FR-C11): All-Sound-Off (CC120) + All-Notes-Off (CC123) on every
    // channel, then clears the active-note state.
    void panic();

    void flush();

    // Send a raw MIDI message without mutating note state (used for rhythm
    // events drained from the Core 1 queue). Logs everything except the
    // high-rate system clock byte.
    void sendRaw(const MidiMessage& msg);

    void sendTestNote();

private:
    MidiOutAdapter& midiOut_;
    StateManager& state_;

    void send(const MidiMessage& msg);
};


