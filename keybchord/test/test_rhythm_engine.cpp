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

    int countClock(const std::vector<MidiMessage>& msgs) {
        int n = 0;
        for (const auto& m : msgs) {
            if (m.status == midi::SYSTEM_CLOCK) n++;
        }
        return n;
    }

    int countSystem(const std::vector<MidiMessage>& msgs, uint8_t status) {
        int n = 0;
        for (const auto& m : msgs) {
            if (m.status == status) n++;
        }
        return n;
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

TEST_F(RhythmEngineTest, PublishesTickSnapshotWithoutRhythm) {
    state_.pendingRhythm.enabled = false;
    uint32_t tick = clockTickUs(120);

    engine_->update(0);
    // tickAbs is the last emitted tick: tick #0 fires at boot, and the next
    // tick is one interval away.
    EXPECT_EQ(state_.rhythmClock.tickAbs.load(), 0u);
    EXPECT_EQ(state_.rhythmClock.nextTickUs.load(), tick);

    uint64_t before = state_.rhythmClock.tickAbs.load();
    uint64_t nextBefore = state_.rhythmClock.nextTickUs.load();
    engine_->update(4 * tick);
    EXPECT_GT(state_.rhythmClock.tickAbs.load(), before);
    EXPECT_GT(state_.rhythmClock.nextTickUs.load(), nextBefore);
    EXPECT_FALSE(state_.rhythmClock.running);
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

TEST_F(RhythmEngineTest, ClockJitterIsRecordedEvenWithoutRhythm) {
    // Regression: jitter must come from the always-running master clock, so it
    // is nonzero even when the rhythm is disabled (previously it read n=0).
    state_.pendingRhythm.enabled = false;
    uint32_t tick = clockTickUs(120);

    engine_->update(0);
    for (int i = 1; i <= 10; i++) {
        engine_->update(static_cast<uint64_t>(i) * tick + 123);
    }

    PerfStats stats;
    engine_->jitterStats(stats, false);
    EXPECT_GT(stats.count(), 0u);
    EXPECT_LE(stats.minUs(), stats.maxUs());
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


TEST(RhythmLoader, LoadsBuiltinsAndSelfHealsMissingFiles) {
    StorageStub storage;
    storage.writeFile("/rhythms/rock1.json",
        "{\"name\":\"Rock 1\",\"steps_per_bar\":16,\"swing\":0,"
        "\"tracks\":[{\"note\":36,\"name\":\"kick\",\"pattern\":[1,0]}]}");

    auto lib = loadRhythmPatterns(storage);
    ASSERT_EQ(lib.patterns.size(), static_cast<size_t>(RHYTHM_COUNT));
    ASSERT_EQ(lib.names.size(), static_cast<size_t>(RHYTHM_COUNT));
    EXPECT_EQ(lib.patterns[0].name, "Rock 1");
    // Missing built-in files were re-provisioned.
    EXPECT_TRUE(storage.exists("/rhythms/foxtrot.json"));
}

TEST(RhythmLoader, FallsBackToBuiltinWhenEmpty) {
    StorageStub storage;
    auto lib = loadRhythmPatterns(storage);
    ASSERT_EQ(lib.patterns.size(), static_cast<size_t>(RHYTHM_COUNT));
    EXPECT_EQ(lib.patterns[0].name, "Rock 1");
    EXPECT_FALSE(lib.patterns[0].tracks.empty());
}

TEST(RhythmLoader, AppendsUserRhythmsSortedByFilename) {
    StorageStub storage;
    loadRhythmPatterns(storage);  // self-heal the 12 built-ins

    storage.writeFile("/rhythms/zz_user.json",
        "{\"name\":\"Z Groove\",\"steps_per_bar\":8,\"swing\":0,"
        "\"tracks\":[{\"note\":36,\"name\":\"kick\",\"pattern\":[1,0,0,0,1,0,0,0]}]}");
    storage.writeFile("/rhythms/aa_user.json",
        "{\"name\":\"A Groove\",\"steps_per_bar\":8,\"swing\":0,"
        "\"tracks\":[{\"note\":38,\"name\":\"snare\",\"pattern\":[0,0,1,0,0,0,1,0]}]}");

    auto lib = loadRhythmPatterns(storage);
    ASSERT_EQ(lib.patterns.size(), static_cast<size_t>(RHYTHM_COUNT + 2));
    EXPECT_EQ(lib.patterns[RHYTHM_COUNT].name, "A Groove");     // aa sorts first
    EXPECT_EQ(lib.patterns[RHYTHM_COUNT + 1].name, "Z Groove");
    EXPECT_EQ(lib.names[RHYTHM_COUNT], "A Groove");
}

TEST_F(RhythmEngineTest, ClockOutStartsOnBeatBoundaryAfterEnable) {
    uint32_t tick = clockTickUs(120);
    state_.config.midi_clock_enabled = false;

    engine_->update(0);          // tick 0 (clock off)
    drain();
    engine_->update(2 * tick);   // ticks 1..2
    drain();

    state_.config.midi_clock_enabled = true;
    engine_->update(3 * tick);   // tick 3 -> pending, no clock
    EXPECT_EQ(countClock(drain()), 0);

    engine_->update(23 * tick);  // ticks 4..23 -> still pending
    EXPECT_EQ(countClock(drain()), 0);

    engine_->update(24 * tick);  // tick 24 = beat boundary -> start
    EXPECT_EQ(countClock(drain()), 1);

    engine_->update(25 * tick);
    EXPECT_EQ(countClock(drain()), 1);
}

TEST_F(RhythmEngineTest, DisablingClockOutIsImmediateAndSendsNoStop) {
    uint32_t tick = clockTickUs(120);
    state_.config.midi_clock_enabled = true;
    engine_->update(0);
    drain();

    state_.config.midi_clock_enabled = false;
    engine_->update(3 * tick);
    auto stopped = drain();
    EXPECT_EQ(countClock(stopped), 0);
    EXPECT_EQ(countSystem(stopped, midi::SYSTEM_STOP), 0);
    EXPECT_EQ(countSystem(stopped, midi::SYSTEM_START), 0);
}

TEST_F(RhythmEngineTest, ReEnablingClockOutIsClockOnly) {
    uint32_t tick = clockTickUs(120);
    state_.config.midi_clock_enabled = true;
    engine_->update(0);
    drain();
    engine_->update(3 * tick);  // stream a few ticks, then stop mid-beat
    drain();

    state_.config.midi_clock_enabled = false;
    engine_->update(4 * tick);
    drain();

    state_.config.midi_clock_enabled = true;
    engine_->update(5 * tick);   // pending until the next 24-tick boundary
    EXPECT_EQ(countClock(drain()), 0);

    engine_->update(24 * tick);  // boundary: first clock only, no transport
    auto resumed = drain();
    ASSERT_FALSE(resumed.empty());
    EXPECT_EQ(resumed[0].status, midi::SYSTEM_CLOCK);
    EXPECT_EQ(countSystem(resumed, midi::SYSTEM_START), 0);
    EXPECT_EQ(countSystem(resumed, midi::SYSTEM_STOP), 0);
    EXPECT_EQ(countClock(resumed), 1);

    engine_->update(25 * tick);
    auto next = drain();
    EXPECT_EQ(countSystem(next, midi::SYSTEM_START), 0);
    EXPECT_EQ(countClock(next), 1);
}

TEST_F(RhythmEngineTest, ResyncClockSendsStartOnBeatBoundary) {
    uint32_t tick = clockTickUs(120);
    state_.config.midi_clock_enabled = true;
    engine_->update(0);
    drain();
    engine_->update(3 * tick);
    drain();

    engine_->resyncClock();      // armed mid-beat
    engine_->update(23 * tick);  // still before the boundary
    auto before = drain();
    EXPECT_EQ(countSystem(before, midi::SYSTEM_START), 0);

    engine_->update(24 * tick);
    auto resumed = drain();
    ASSERT_FALSE(resumed.empty());
    EXPECT_EQ(resumed[0].status, midi::SYSTEM_START);
    EXPECT_EQ(countSystem(resumed, midi::SYSTEM_START), 1);
    EXPECT_EQ(countClock(resumed), 1);

    engine_->update(25 * tick);
    auto next = drain();
    EXPECT_EQ(countSystem(next, midi::SYSTEM_START), 0);
    EXPECT_EQ(countClock(next), 1);
}

TEST_F(RhythmEngineTest, ResyncClockIgnoredWhenClockOutOff) {
    uint32_t tick = clockTickUs(120);
    engine_->update(0);
    drain();

    engine_->resyncClock();
    engine_->update(24 * tick);
    auto msgs = drain();
    EXPECT_EQ(countSystem(msgs, midi::SYSTEM_START), 0);
    EXPECT_EQ(countClock(msgs), 0);
}

TEST_F(RhythmEngineTest, RecordGuidePlaysQuarterHiHatsAndMutesPattern) {
    uint32_t tick = clockTickUs(120);
    uint32_t base = tick * CLOCK_TICKS_PER_STEP;
    state_.pendingRhythm.enabled = true;
    state_.recordGuideActive.store(true);
    state_.recordGuideHatsActive.store(true);
    state_.recordGuideHatsStartAbs.store(0);

    engine_->update(0);
    drain();
    engine_->update(24 * tick + 12 * base);  // steps 0..12 (beats 1..4)

    auto msgs = drain();
    EXPECT_EQ(countNoteOn(msgs, 36, 10), 0);  // kick suppressed
    EXPECT_EQ(countNoteOn(msgs, 38, 10), 0);  // snare suppressed
    EXPECT_EQ(countNoteOn(msgs, 42, 10), 4);  // quarter-note hi-hats
}

TEST_F(RhythmEngineTest, RecordGuideStopsWhenCleared) {
    uint32_t tick = clockTickUs(120);
    state_.pendingRhythm.enabled = true;
    state_.recordGuideActive.store(true);
    state_.recordGuideHatsActive.store(true);
    state_.recordGuideHatsStartAbs.store(0);
    engine_->update(0);
    drain();

    state_.recordGuideActive.store(false);
    state_.recordGuideHatsActive.store(false);
    state_.recordGuideHatsStartAbs.store(0);
    engine_->update(24 * tick);
    auto msgs = drain();
    EXPECT_EQ(countNoteOn(msgs, 36, 10), 1);  // normal pattern resumes
    EXPECT_EQ(countNoteOn(msgs, 38, 10), 0);
}

TEST_F(RhythmEngineTest, LedAndBarStayAlignedAfterTempoChange) {
    state_.pendingRhythm.enabled = true;
    state_.config.bpm_indicator = true;
    engine_->update(0);
    drain();
    state_.ledIndicator.flash = false;

    // Run two bars at 120 BPM.
    for (uint64_t t = 0; t <= 4 * stepUs(120); t += 1000) {
        engine_->update(t);
        state_.ledIndicator.flash = false;
        drain();
    }

    // Change tempo live, then run four bars at 90 BPM in 1 ms increments.
    state_.pendingRhythm.tempo = 90;
    int kickCount = 0;
    int unalignedKicks = 0;
    uint64_t end = 4 * stepUs(120) + 16 * stepUs(90);
    for (uint64_t t = 4 * stepUs(120); t <= end; t += 1000) {
        engine_->update(t);
        bool flash = state_.ledIndicator.flash;
        state_.ledIndicator.flash = false;
        auto msgs = drain();
        int kicks = countNoteOn(msgs, 36, 10);
        if (kicks > 0) {
            kickCount += kicks;
            if (!flash) unalignedKicks++;
        }
    }
    EXPECT_GT(kickCount, 0);
    EXPECT_EQ(unalignedKicks, 0);
}


TEST_F(RhythmEngineTest, RecordGuideMuteOnlyHasNoHats) {
    uint32_t tick = clockTickUs(120);
    uint32_t base = tick * CLOCK_TICKS_PER_STEP;
    state_.pendingRhythm.enabled = true;
    state_.recordGuideActive.store(true);
    state_.recordGuideHatsActive.store(false);

    engine_->update(0);
    drain();
    engine_->update(24 * tick + 12 * base);  // steps 0..12

    auto msgs = drain();
    EXPECT_EQ(countNoteOn(msgs, 36, 10), 0);  // pattern suppressed
    EXPECT_EQ(countNoteOn(msgs, 42, 10), 0);  // no hats before the count-in
}


TEST_F(RhythmEngineTest, RecordGuideHatsHonorAbsoluteStartStep) {
    uint32_t tick = clockTickUs(120);
    uint32_t base = tick * CLOCK_TICKS_PER_STEP;
    state_.pendingRhythm.enabled = true;
    state_.recordGuideActive.store(true);
    state_.recordGuideHatsActive.store(true);
    state_.recordGuideHatsStartAbs.store(24);  // no hats before absolute step 24

    engine_->update(0);
    drain();
    engine_->update(24 * tick + 23 * base);  // steps 0..23
    EXPECT_EQ(countNoteOn(drain(), 42, 10), 0);

    engine_->update(24 * tick + 24 * base);  // absolute step 24 (count-in)
    EXPECT_EQ(countNoteOn(drain(), 42, 10), 1);
}

TEST_F(RhythmEngineTest, StagePatternsAppliesOnUpdate) {
    RhythmPattern staged = rock1Pattern();
    staged.name = "Staged";
    engine_->stagePatterns({staged});
    EXPECT_TRUE(engine_->patternsPending());

    engine_->update(0);
    EXPECT_FALSE(engine_->patternsPending());

    const RhythmPattern* got = engine_->patternAt(0);
    ASSERT_NE(got, nullptr);
    EXPECT_EQ(got->name, "Staged");
}

TEST_F(RhythmEngineTest, InvalidPatternIndexRecoversAndPublishesClock) {
    state_.pendingRhythm.pattern = 9;  // out of range for one loaded pattern
    state_.pendingRhythm.enabled = true;
    engine_->update(0);
    engine_->update(24 * clockTickUs(120));

    EXPECT_EQ(state_.pendingRhythm.pattern, 0);
    EXPECT_TRUE(state_.rhythmClock.running);
    EXPECT_EQ(state_.rhythmClock.stepsPerBar, 16);
}

TEST_F(RhythmEngineTest, ZeroStepsPerBarDoesNotStallTheClock) {
    RhythmPattern broken = rock1Pattern();
    broken.steps_per_bar = 0;
    engine_->setPatterns({broken});
    state_.pendingRhythm.enabled = true;
    engine_->update(0);
    engine_->update(24 * clockTickUs(120));

    EXPECT_TRUE(state_.rhythmClock.running);
    EXPECT_EQ(state_.rhythmClock.stepsPerBar, 16);
}


TEST(RhythmQueue, TimestampAndOverflow) {
    MidiEventQueue q;
    ASSERT_TRUE(q.push(midi::makeNoteOn(10, 36, 100), 1234));
    QueuedMidi out;
    ASSERT_TRUE(q.pop(out));
    EXPECT_EQ(out.queued_us, 1234u);
    EXPECT_EQ(out.msg.data1, 36);

    for (size_t i = 0; i < MidiEventQueue::CAPACITY; i++) {
        ASSERT_TRUE(q.push(midi::makeSystem(midi::SYSTEM_CLOCK), 1));
    }
    EXPECT_FALSE(q.push(midi::makeSystem(midi::SYSTEM_CLOCK), 1));
    EXPECT_EQ(q.takeOverflowCount(), 1u);
}

TEST_F(RhythmEngineTest, RebaseSkipsMissedStepOutput) {
    uint32_t tick = clockTickUs(120);
    uint32_t base = tick * CLOCK_TICKS_PER_STEP;
    state_.pendingRhythm.enabled = true;

    engine_->update(0);
    engine_->update(24 * tick);  // downbeat
    drain();

    uint32_t before = state_.rhythmClock.stepAbs.load();
    engine_->requestRebase();
    engine_->update(24 * tick + 10 * base);  // ten missed steps

    EXPECT_GT(state_.rhythmClock.stepAbs.load(), before);
    auto msgs = drain();
    EXPECT_EQ(countNoteOn(msgs, 36, 10), 0);
    EXPECT_EQ(countNoteOn(msgs, 38, 10), 0);
    EXPECT_EQ(countNoteOn(msgs, 42, 10), 0);
}
