#pragma once

#include "base.h"


class MidiOutUart : public MidiOutAdapter {
public:
    bool begin() override;
    void send(const MidiMessage& msg) override;
    void sendBytes(const uint8_t* data, size_t length) override;
    void flush() override;
};


