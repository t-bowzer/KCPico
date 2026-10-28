#include "midi_router.h"
#include "midimsg.h"
#include "debug_log.h"


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

void MidiRouter::panic() {
    for (uint8_t ch = 1; ch <= 16; ch++) {
        allSoundOff(ch);
        allNotesOff(ch);
    }
    state_.allNotesOff();
}

void MidiRouter::flush() {
    midiOut_.flush();
    if (usbOut_ && state_.config.usb_midi_enabled) {
        usbOut_->flush();
    }
}

void MidiRouter::sendRaw(const MidiMessage& msg) {
    if (msg.status != midi::SYSTEM_CLOCK) {
        logMidiOut(msg.status, msg.data1, msg.data2);
    }
    midiOut_.send(msg);
    if (usbOut_ && state_.config.usb_midi_enabled) {
        usbOut_->send(msg);
    }
}

void MidiRouter::sendBytes(const uint8_t* data, size_t length) {
    if (!data || length == 0) return;

    if (length <= 3) {
        logMidiOut(data[0],
                   length > 1 ? data[1] : 0,
                   length > 2 ? data[2] : 0);
    } else {
        logMidiOutBytes(data, length);
    }

    midiOut_.sendBytes(data, length);
    if (usbOut_ && state_.config.usb_midi_enabled) {
        usbOut_->sendBytes(data, length);
    }
}

void MidiRouter::sendTestNote() {
    const uint8_t channel  = 1;
    const uint8_t note     = 60; // C4
    const uint8_t velocity = 100;
    noteOn(channel, note, velocity);
}

void MidiRouter::setUsbOut(MidiOutAdapter* usbOut) {
    usbOut_ = usbOut;
}

void MidiRouter::send(const MidiMessage& msg) {
    logMidiOut(msg.status, msg.data1, msg.data2);
    midiOut_.send(msg);
    if (usbOut_ && state_.config.usb_midi_enabled) {
        usbOut_->send(msg);
    }
}


