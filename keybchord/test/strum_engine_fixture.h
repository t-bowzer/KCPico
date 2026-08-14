#pragma once

#include <gtest/gtest.h>

#include "chord_engine.h"
#include "midi_router.h"
#include "recording_midi_out.h"
#include "state.h"
#include "strum_engine.h"


// Fixture for strum-engine tests: recording MIDI out + router + both chord and
// strum engines (chord engine sets the selected chord the strum plate derives
// its pool from).
class StrumEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        midi_.begin();
        state_.loadDefaults();
        router_ = new MidiRouter(midi_, state_);
        chord_  = new ChordEngine(state_, *router_);
        strum_  = new StrumEngine(state_, *router_);
    }

    void TearDown() override {
        delete strum_;
        delete chord_;
        delete router_;
    }

    KeyEvent key(uint8_t usage, bool pressed, uint8_t mods = 0) {
        return {usage, pressed, mods};
    }

    // Note value of the last note-on on the given (1-indexed) channel, or -1.
    int lastNoteOnOnChannel(uint8_t channel) const {
        int note = -1;
        for (const auto& m : midi_.messages()) {
            if ((m.status & 0xF0) == midi::STATUS_NOTE_ON && m.data2 > 0
                && (m.status & 0x0F) == (channel - 1)) {
                note = m.data1;
            }
        }
        return note;
    }

    int noteOffCountOnChannel(uint8_t channel) const {
        int n = 0;
        for (const auto& m : midi_.messages()) {
            if ((m.status & 0xF0) == midi::STATUS_NOTE_OFF
                && (m.status & 0x0F) == (channel - 1)) {
                n++;
            }
        }
        return n;
    }

    RecordingMidiOutAdapter midi_;
    StateManager state_;
    MidiRouter* router_ = nullptr;
    ChordEngine* chord_ = nullptr;
    StrumEngine* strum_ = nullptr;
};
