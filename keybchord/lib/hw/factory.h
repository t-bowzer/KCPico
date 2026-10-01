#pragma once

#include <memory>
#include "base.h"


struct Adapters {
    std::unique_ptr<InputAdapter>   input;
    std::unique_ptr<MidiOutAdapter> midiOut;
    std::unique_ptr<MidiOutAdapter> usbMidiOut;  // USB MIDI device (null on native)
    std::unique_ptr<LcdAdapter>     lcd;
    std::unique_ptr<StorageAdapter> storage;
};

Adapters createAdapters();


