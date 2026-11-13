#pragma once

#include <atomic>
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

    // Stage a new rhythm set from Core 0 (pattern editor save/delete). Core 1
    // swaps it in at the top of update(); the editor must not stage again until
    // the pending flag clears.
    void stagePatterns(std::vector<RhythmPattern> patterns);
    bool patternsPending() const { return patternsPending_.load(); }

    // The pattern at `index` (null if out of range). Used by the edit engine to
    // resolve the Alt+F1..F12 drum-mute track list.
    const RhythmPattern* patternAt(int index) const;

    // Called after the pattern parameter changes (via the EditEngine): adopts
    // the new pattern's authored swing default.
    void onPatternChanged();

    void update(uint64_t now_us);
    void allNotesOff();

    // Copies the master-clock tick-lateness jitter statistics (NFR-2) to `out`
    // and, when `reset` is true, clears the accumulator. Called from Core 0 for
    // logging; a best-effort read is safe since fields are 32-bit aligned.
    void jitterStats(PerfStats& out, bool reset);

    // Arm a MIDI Start (0xFA) at the next 24-tick beat boundary. Used by the
    // explicit Clock Resync action (F8 double-tap); Clock Out itself is a plain
    // continuous clock and never sends transport automatically.
    void resyncClock();

    // After a blocking UI/storage operation, skip any missed step output and
    // rebase the step deadline to the current clock phase. Safe to call from
    // Core 0; Core 1 consumes it at the top of its next update.
    void requestRebase() { rebasePending_.store(true); }

private:
    StateManager& state_;
    MidiEventQueue& out_;

    std::vector<RhythmPattern> patterns_;
    std::vector<RhythmPattern> pendingPatterns_;
    std::atomic<bool> patternsPending_{false};
    mutable std::atomic<bool> patternsBusy_{false};
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

    // Clock Out is clock-only: enabling waits for the next 24-tick beat
    // boundary so the slave's beat division starts on the LED's boundary;
    // disabling simply stops the 0xF8 stream (no MIDI Stop). An explicit
    // resync request sends MIDI Start at a later beat boundary.
    bool     clockOutWasEnabled_ = false;
    bool     clockOutPending_ = false;
    bool     clockOutActive_ = false;
    bool     clockResyncPending_ = false;
    std::atomic<bool> rebasePending_{false};

    uint16_t lastTempo_ = 0;

    const RhythmPattern* currentPattern() const;

    PerfStats jitter_;   // clock tick-lateness stats, written only by Core 1

    void advanceClock(uint64_t now_us);
    void start(uint64_t now_us);
    void restartAtNextBeat();
    void stop();
    void fireStep(uint64_t now_us);
};

// Loads the full rhythm set from storage: the 12 built-in patterns first (in
// fixed order, self-healing missing/corrupt files from the embedded templates),
// then any additional user-provided /rhythms/*.json files (sorted by filename,
// appended at indices RHYTHM_COUNT..). Falls back to a single built-in pattern
// on total failure. `names` matches `patterns` in order.
RhythmLibrary loadRhythmPatterns(StorageAdapter& storage);
