#include "midi_router.h"
#include "midimsg.h"


MidiRouter::MidiRouter(MidiOutAdapter& out, StateManager& state)
    : midiOut_(out), state_(state) {}

void MidiRouter::noteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    auto msg = midi::makeNoteOn(channel, note, velocity);
    send(msg);
    state_.noteOn(channel, note);
}

void MidiRouter::noteOff(uint8_t channel, uint8_t note) {
    auto msg = midi::makeNoteOff(channel, note);
    send(msg);
    state_.noteOff(channel, note);
}

void MidiRouter::cc(uint8_t channel, uint8_t control, uint8_t value) {
    auto msg = midi::makeCC(channel, control, value);
    send(msg);
}

void MidiRouter::allNotesOff(uint8_t channel) {
    cc(channel, midi::CC_ALL_NOTES_OFF, 0);
}

void MidiRouter::allSoundOff(uint8_t channel) {
    cc(channel, midi::CC_ALL_SOUND_OFF, 0);
}

void MidiRouter::sendTestNote() {
    const uint8_t channel  = 1;
    const uint8_t note     = 60; // C4
    const uint8_t velocity = 100;
    noteOn(channel, note, velocity);
}

void MidiRouter::send(const MidiMessage& msg) {
    midiOut_.send(msg);
}


