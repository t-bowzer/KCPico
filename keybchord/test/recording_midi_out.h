#pragma once

#include <vector>

#include "base.h"
#include "midimsg.h"


// Test spy: records every MIDI message sent through a MidiOutAdapter.
class RecordingMidiOutAdapter : public MidiOutAdapter {
public:
    bool begin() override { return true; }
    void send(const MidiMessage& msg) override { messages_.push_back(msg); }
    void sendBytes(const uint8_t* data, size_t length) override {
        if (!data) return;
        rawMessages_.push_back(std::vector<uint8_t>(data, data + length));
    }
    void flush() override {}

    const std::vector<MidiMessage>& messages() const { return messages_; }
    const std::vector<std::vector<uint8_t>>& rawMessages() const {
        return rawMessages_;
    }
    void clear() { messages_.clear(); rawMessages_.clear(); }

    int noteOnCount() const {
        int n = 0;
        for (const auto& m : messages_) {
            if ((m.status & 0xF0) == midi::STATUS_NOTE_ON && m.data2 > 0) n++;
        }
        return n;
    }

    int noteOffCount() const {
        int n = 0;
        for (const auto& m : messages_) {
            if ((m.status & 0xF0) == midi::STATUS_NOTE_OFF) n++;
        }
        return n;
    }

    int ccCount() const {
        int n = 0;
        for (const auto& m : messages_) {
            if ((m.status & 0xF0) == midi::STATUS_CONTROL_CHANGE) n++;
        }
        return n;
    }

private:
    std::vector<MidiMessage> messages_;
    std::vector<std::vector<uint8_t>> rawMessages_;
};

