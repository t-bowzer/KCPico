#include "midi_out_usb.h"

#ifndef KEYBCHORD_NATIVE

#include <Arduino.h>
#include "arduino/midi/Adafruit_USBD_MIDI.h"

#include "debug_log.h"


namespace {
Adafruit_USBD_MIDI* g_usbMidi = nullptr;
} // namespace


bool MidiOutUsb::begin() {
    g_usbMidi = new Adafruit_USBD_MIDI(1);
    if (!g_usbMidi->begin()) {
        logError("USB MIDI: failed to register interface");
        ready_ = false;
        return false;
    }
    ready_ = true;
    logInfo("USB MIDI: interface registered");
    return true;
}

// Mirrors the DIN/UART framing (midi_out_uart.cpp): system realtime bytes are
// single-byte; program-change/channel-pressure carry one data byte; everything
// else carries two. Adafruit_USBD_MIDI::write() feeds TinyUSB's MIDI stream
// writer, which packets the raw byte stream (running status, system realtime).
void MidiOutUsb::send(const MidiMessage& msg) {
    if (!ready_ || !g_usbMidi) return;

    uint8_t status = msg.status;

    if (status >= 0xF8) {
        g_usbMidi->write(status);
        return;
    }

    g_usbMidi->write(status);
    g_usbMidi->write(msg.data1);

    uint8_t cmd = status & 0xF0;
    if (cmd != 0xC0 && cmd != 0xD0) {
        g_usbMidi->write(msg.data2);
    }
}

void MidiOutUsb::flush() {
    // TinyUSB MIDI streams are sent on the next USB frame; there is no
    // blocking flush for the device path.
}

#endif // !KEYBCHORD_NATIVE
