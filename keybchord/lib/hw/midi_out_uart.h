#pragma once

#include "base.h"


class MidiOutUart : public MidiOutAdapter {
public:
    bool begin() override;
    void send(const MidiMessage& msg) override;
    void flush() override;
};


