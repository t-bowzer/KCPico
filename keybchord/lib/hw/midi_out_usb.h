#pragma once

#include "base.h"


// USB MIDI device output on the native USB port (TinyUSB + Adafruit_USBD_MIDI).
// The keyboard host stays on the PIO-USB port, so the native port is free to
// present a composite CDC (debug log) + MIDI device. The interface is registered
// at begin(); whether bytes are actually sent is gated by the caller (the MIDI
// router consults config.midi.usb_enabled).
class MidiOutUsb : public MidiOutAdapter {
public:
    bool begin() override;
    void send(const MidiMessage& msg) override;
    void flush() override;

private:
    bool ready_ = false;
};
