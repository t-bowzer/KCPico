#include <gtest/gtest.h>

#include "midimsg.h"
#include "rhythm_engine.h"
#include "storage_stub.h"


static RhythmPattern rock1Pattern() {
    RhythmPattern p;
    p.name = "Rock 1";
    p.steps_per_bar = 16;
    p.swing = 0;
    p.tracks = {
        {36, "kick",  {1,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0}},
        {38, "snare", {0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0}},
        {42, "hihat", {1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0}},
    };
    return p;
}

static RhythmPattern waltzPattern() {
    RhythmPattern p;
    p.name = "Waltz";
    p.steps_per_bar = 12;
    p.swing = 0;
    p.tracks = {
        {36, "kick", {1,0,0,0, 0,0,0,0, 0,0,0,0}},
    };
    return p;
}


class RhythmEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        state_.loadDefaults();
        engine_ = new RhythmEngine(state_, queue_);
        engine_->setPatterns({rock1Pattern()});
    }

    void TearDown() override { delete engine_; }

    KeyEvent key(uint8_t usage, bool pressed, uint8_t mods = 0) {
        return {usage, pressed, mods};
    }

    std::vector<MidiMessage> drain() {
        std::vector<MidiMessage> out;
        MidiMessage m;
        while (queue_.pop(m)) out.push_back(m);
        return out;
    }

    int countNoteOn(const std::vector<MidiMessage>& msgs, uint8_t note,
                    uint8_t channel) {
        int n = 0;
        for (const auto& m : msgs) {
            if ((m.status & 0xF0) == midi::STATUS_NOTE_ON && m.data2 > 0 &&
                (m.status & 0x0F) == (channel - 1) && m.data1 == note) {
                n++;
            }
        }
        return n;
    }

    StateManager state_;
    MidiEventQueue queue_;
    RhythmEngine* engine_ = nullptr;
};


TEST_F(RhythmEngineTest, EnableFiresDownbeat) {
    state_.pendingRhythm.enabled = true;
    engine_->update(0);   // clock arms (tick 0); rhythm slaves to the beat grid
    // Downbeat (step 0) fires at the first beat boundary, tick #24.
    engine_->update(24 * clockTickUs(120));

    auto msgs = drain();
    EXPECT_EQ(countNoteOn(msgs, 36, 10), 1);  // kick on step 0
    EXPECT_EQ(countNoteOn(msgs, 38, 10), 0);  // snare silent on step 0
    EXPECT_EQ(countNoteOn(msgs, 42, 10), 1);  // hihat on step 0
}

TEST_F(RhythmEngineTest, DisabledProducesNothing) {
    engine_->update(0);
    engine_->update(1000000);
    EXPECT_TRUE(drain().empty());
    EXPECT_FALSE(state_.rhythmClock.running);
}

TEST_F(RhythmEngineTest, StepsAdvanceAtDeadlines) {
    state_.pendingRhythm.enabled = true;
    engine_->update(0);
    drain();

    uint32_t tick = clockTickUs(120);
    uint32_t base = tick * CLOCK_TICKS_PER_STEP;   // one 16th = 6 ticks
    // Steps 1..4 fire by step 4's deadline (beat 2); step 4 carries the snare.
    engine_->update(24 * tick + 4 * base);
    auto msgs = drain();
    EXPECT_EQ(countNoteOn(msgs, 38, 10), 1);  // snare fired once
}

TEST_F(RhythmEngineTest, MuteSuppressesDrumsButKeepsSync) {
    state_.pendingRhythm.enabled = true;
    state_.pendingRhythm.muted = true;
    engine_->update(0);
    engine_->update(8 * stepUs(120));

    auto msgs = drain();
    for (const auto& m : msgs) {
        EXPECT_NE(m.status & 0xF0, midi::STATUS_NOTE_ON);
    }
    EXPECT_TRUE(state_.rhythmClock.running);
}

TEST_F(RhythmEngineTest, MidiClockTicksAt24Ppqn) {
    state_.config.midi_clock_enabled = true;
    state_.pendingRhythm.enabled = true;
    engine_->update(0);
    drain();  // discard the downbeat drum events

    uint32_t tick = clockTickUs(120);
    engine_->update(3 * tick);
    auto msgs = drain();
    int clock = 0;
    for (const auto& m : msgs) {
        if (m.status == midi::SYSTEM_CLOCK) clock++;
        // No transport messages (Start/Stop/Continue) — clock-only sync.
        EXPECT_NE(m.status, midi::SYSTEM_START);
        EXPECT_NE(m.status, midi::SYSTEM_STOP);
        EXPECT_NE(m.status, midi::SYSTEM_CONTINUE);
    }
    EXPECT_EQ(clock, 3);
}

TEST_F(RhythmEngineTest, ClockStreamsIndependentOfRhythm) {
    state_.config.midi_clock_enabled = true;
    // No rhythm running, but the clock still streams (continuous tempo master).
    engine_->update(0);
    drain();

    engine_->update(4 * clockTickUs(120));
    int ticks = 0;
    for (const auto& m : drain()) {
        if (m.status == midi::SYSTEM_CLOCK) ticks++;
        EXPECT_NE(m.status, midi::SYSTEM_START);
        EXPECT_NE(m.status, midi::SYSTEM_STOP);
    }
    EXPECT_GT(ticks, 0);

    // Toggling the clock off stops the stream.
    state_.config.midi_clock_enabled = false;
    engine_->update(1000000);
    drain();
    engine_->update(2000000);
    for (const auto& m : drain()) {
        EXPECT_NE(m.status, midi::SYSTEM_CLOCK);
    }
}

TEST_F(RhythmEngineTest, RhythmAlignsToClockBeatGrid) {
    state_.config.midi_clock_enabled = true;
    uint32_t tick = clockTickUs(120);

    engine_->update(0);         // start the clock (master timebase)
    drain();
    engine_->update(7 * tick);  // stream 7 ticks (not a beat boundary)
    drain();

    state_.pendingRhythm.enabled = true;
    engine_->update(7 * tick);  // enable rhythm -> downbeat deferred
    EXPECT_EQ(countNoteOn(drain(), 36, 10), 0);

    // The next 24-tick beat boundary (tick #24) fires at (24+1)*tick.
    engine_->update(25 * tick);
    EXPECT_EQ(countNoteOn(drain(), 36, 10), 1);
}

TEST_F(RhythmEngineTest, ClockPhasePersistsAcrossRhythmToggle) {
    state_.config.midi_clock_enabled = true;
    uint32_t tick = clockTickUs(120);

    state_.pendingRhythm.enabled = true;
    engine_->update(0);      // rhythm downbeat at tick 0 (clock just started)
    drain();

    engine_->update(10 * tick);  // stream 10 ticks
    drain();

    // Toggle rhythm off, then back on: the clock keeps its phase, so the next
    // downbeat still lands on the 24-tick grid (tick #24), not a fresh phase.
    state_.pendingRhythm.enabled = false;
    engine_->update(10 * tick);
    drain();

    state_.pendingRhythm.enabled = true;
    engine_->update(10 * tick);
    EXPECT_EQ(countNoteOn(drain(), 36, 10), 0);  // deferred to beat boundary

    engine_->update(25 * tick);
    EXPECT_EQ(countNoteOn(drain(), 36, 10), 1);
}

TEST_F(RhythmEngineTest, LedFlashesOnBeat) {
    state_.config.bpm_indicator = true;
    state_.pendingRhythm.enabled = true;
    engine_->update(0);  // first beat fires immediately

    EXPECT_TRUE(state_.ledIndicator.flash);
}

TEST_F(RhythmEngineTest, LedFlashesWithoutRhythm) {
    state_.config.bpm_indicator = true;
    state_.pendingRhythm.enabled = false;  // rhythm off — LED still pulses
    engine_->update(0);

    EXPECT_TRUE(state_.ledIndicator.flash);
}

TEST_F(RhythmEngineTest, LedDisabledWhenIndicatorOff) {
    state_.config.bpm_indicator = false;
    state_.pendingRhythm.enabled = true;
    engine_->update(0);
    EXPECT_FALSE(state_.ledIndicator.flash);
}

TEST_F(RhythmEngineTest, PublishesClockSnapshot) {
    state_.pendingRhythm.enabled = true;
    engine_->update(0);
    engine_->update(24 * clockTickUs(120));   // step 0 fires at beat boundary
    EXPECT_TRUE(state_.rhythmClock.running);
    EXPECT_EQ(state_.rhythmClock.step, 1u);     // step 0 fired, now on step 1
    EXPECT_EQ(state_.rhythmClock.stepAbs, 1u);
    EXPECT_EQ(state_.rhythmClock.stepsPerBar, 16);
    EXPECT_EQ(state_.rhythmClock.beat, 0u);
}

TEST_F(RhythmEngineTest, DrumMapRemapsSnareNote) {
    state_.pendingRhythm.drums.snare = 40;  // Electric Snare
    state_.pendingRhythm.enabled = true;
    engine_->update(0);
    drain();

    uint32_t tick = clockTickUs(120);
    uint32_t base = tick * CLOCK_TICKS_PER_STEP;
    // Step 4 (beat 2) carries the snare; it must use the mapped code 40.
    engine_->update(24 * tick + 4 * base);
    auto msgs = drain();
    EXPECT_EQ(countNoteOn(msgs, 40, 10), 1);
    EXPECT_EQ(countNoteOn(msgs, 38, 10), 0);
}


TEST_F(RhythmEngineTest, OnPatternChangedAdoptsSwing) {
    RhythmPattern a = rock1Pattern();
    RhythmPattern b = waltzPattern();
    b.swing = 5;
    engine_->setPatterns({a, b});

    state_.pendingRhythm.pattern = 1;
    state_.pendingRhythm.swing = 0;

    engine_->onPatternChanged();
    EXPECT_EQ(state_.pendingRhythm.swing, 5);
}


TEST(RhythmLoader, LoadsPatternsFromStorage) {
    StorageStub storage;
    storage.writeFile("/rhythms/rock1.json",
        "{\"name\":\"Rock 1\",\"steps_per_bar\":16,\"swing\":0,"
        "\"tracks\":[{\"note\":36,\"name\":\"kick\",\"pattern\":[1,0]}]}");

    auto patterns = loadRhythmPatterns(storage);
    ASSERT_EQ(patterns.size(), 1u);
    EXPECT_EQ(patterns[0].name, "Rock 1");
}

TEST(RhythmLoader, FallsBackToBuiltinWhenEmpty) {
    StorageStub storage;
    auto patterns = loadRhythmPatterns(storage);
    ASSERT_EQ(patterns.size(), 1u);
    EXPECT_EQ(patterns[0].name, "Rock 1");
    EXPECT_FALSE(patterns[0].tracks.empty());
}
