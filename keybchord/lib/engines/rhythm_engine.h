#pragma once

#include <cstdint>
#include <vector>

#include "base.h"
#include "midi_event_queue.h"
#include "perf.h"
#include "rhythm.h"
#include "state.h"

class StorageAdapter;


// Core 1 rhythm scheduler/clock. Produces GM drum note-ons, MIDI clock, and
// the keyboard-LED beat callback; publishes a RhythmClock snapshot for Core 0.
//
// All MIDI output is enqueued (never sent) — Core 0 drains the queue so the
// UART stays single-threaded. The `now_us` parameter to update() is injectable
// so the scheduling logic is deterministic and testable on the host.
class RhythmEngine {
public:
    RhythmEngine(StateManager& state, MidiEventQueue& out);

    // Install the full rhythm set (indices must align with the rhythm list).
    void setPatterns(std::vector<RhythmPattern> patterns);
    void setPattern(const RhythmPattern& p, int index);

    // Called after the pattern parameter changes (via the EditEngine): adopts
    // the new pattern's authored swing default.
    void onPatternChanged();

    void update(uint64_t now_us);
    void allNotesOff();

    // Copies the master-clock tick-lateness jitter statistics (NFR-2) to `out`
    // and, when `reset` is true, clears the accumulator. Called from Core 0 for
    // logging; a best-effort read is safe since fields are 32-bit aligned.
    void jitterStats(PerfStats& out, bool reset);

private:
    StateManager& state_;
    MidiEventQueue& out_;

    std::vector<RhythmPattern> patterns_;
    bool running_ = false;
    uint32_t step_ = 0;          // step index within the bar
    uint32_t stepAbs_ = 0;       // monotonic step counter
    uint64_t barStartUs_ = 0;    // absolute time of the bar's step 0
    uint64_t stepDeadlineUs_ = 0;

    // Master MIDI clock (24 PPQN). Always advances from boot; the phase never
    // resets, so it is a stable timebase that the beat LED and the rhythm both
    // slave to. midi_clock_enabled only gates whether 0xF8 is emitted.
    uint64_t nextClockUs_ = 0;    // absolute deadline of the next tick
    bool     clockArmed_ = false; // phase initialized (once, at first update)
    uint64_t clockTickIndex_ = 0; // total ticks emitted (monotonic)

    const RhythmPattern* currentPattern() const;

    PerfStats jitter_;   // clock tick-lateness stats, written only by Core 1

    void advanceClock(uint64_t now_us);
    void start(uint64_t now_us);
    void stop();
    void fireStep(uint64_t now_us);
};

// Loads all shipped rhythm patterns from storage (best-effort); on total
// failure, falls back to a built-in pattern so the rhythm still plays.
std::vector<RhythmPattern> loadRhythmPatterns(StorageAdapter& storage);
