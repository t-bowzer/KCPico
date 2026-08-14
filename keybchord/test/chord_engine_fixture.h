#pragma once

#include <gtest/gtest.h>

#include "chord_engine.h"
#include "midi_router.h"
#include "recording_midi_out.h"
#include "state.h"


// Common fixture for chord-engine tests: recording MIDI out + router + engine.
class ChordEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        midi_.begin();
        state_.loadDefaults();
        router_ = new MidiRouter(midi_, state_);
        engine_ = new ChordEngine(state_, *router_);
    }

    void TearDown() override {
        delete engine_;
        delete router_;
    }

    KeyEvent key(uint8_t usage, bool pressed, uint8_t mods = 0) {
        return {usage, pressed, mods};
    }

    // Note-on values (notes with velocity > 0), in emission order.
    std::vector<uint8_t> noteOnNotes() const {
        std::vector<uint8_t> out;
        for (const auto& m : midi_.messages()) {
            if ((m.status & 0xF0) == midi::STATUS_NOTE_ON && m.data2 > 0) {
                out.push_back(m.data1);
            }
        }
        return out;
    }

    RecordingMidiOutAdapter midi_;
    StateManager state_;
    MidiRouter* router_ = nullptr;
    ChordEngine* engine_ = nullptr;
};

