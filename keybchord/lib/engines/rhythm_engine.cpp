#include "rhythm_engine.h"

#include "midimsg.h"
#include "params.h"


namespace {

// Built-in fallback pattern (spec 7.1 "Rock 1"): kick on 1 & 3, snare on 2 & 4,
// eighth-note hats. Used when no pattern JSON can be loaded from storage.
RhythmPattern builtinRock1() {
    RhythmPattern p;
    p.name = "Rock 1";
    p.steps_per_bar = 16;
    p.swing = 0;
    p.tracks = {
        {36, "kick",  {1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,0}},
        {38, "snare", {0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0}},
        {42, "hihat", {1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0}},
    };
    return p;
}

} // namespace


RhythmEngine::RhythmEngine(StateManager& state, MidiEventQueue& out)
    : state_(state), out_(out) {}

void RhythmEngine::setPatterns(std::vector<RhythmPattern> patterns) {
    patterns_ = std::move(patterns);
    // Clamp the selected pattern index into the new set (typically called once
    // at startup, before the rhythm is running).
    if (!patterns_.empty() && state_.pendingRhythm.pattern >= patterns_.size()) {
        state_.pendingRhythm.pattern = 0;
    }
}

void RhythmEngine::setPattern(const RhythmPattern& p, int index) {
    if (index < 0) return;
    if (index >= static_cast<int>(patterns_.size())) {
        patterns_.resize(static_cast<size_t>(index) + 1);
    }
    patterns_[index] = p;
}

const RhythmPattern* RhythmEngine::currentPattern() const {
    int idx = state_.pendingRhythm.pattern;
    if (idx < 0 || idx >= static_cast<int>(patterns_.size())) return nullptr;
    return &patterns_[idx];
}

void RhythmEngine::onPatternChanged() {
    if (patterns_.empty()) return;
    int idx = state_.pendingRhythm.pattern;
    if (idx >= 0 && idx < static_cast<int>(patterns_.size())) {
        // Adopt the pattern's authored swing default (spec 7.2).
        state_.pendingRhythm.swing = patterns_[idx].swing;
    }
}

// Master MIDI clock (24 PPQN). Runs continuously from boot as a background
// timebase: the tick counter and phase never reset, so nothing (rhythm toggles,
// clock-out toggles) can make it drift or fall out of sync. midi_clock_enabled
// only gates whether the 0xF8 byte is emitted; the beat LED flashes on every
// 24th tick regardless.
void RhythmEngine::advanceClock(uint64_t now_us) {
    uint32_t tick = clockTickUs(state_.pendingRhythm.tempo);

    if (!clockArmed_) {
        clockArmed_ = true;
        nextClockUs_ = now_us;   // tick #0 = boot time (the epoch)
        clockTickIndex_ = 0;
        // fall through so tick #0 is emitted immediately
    }

    while (nextClockUs_ <= now_us) {
        // Record the tick lateness (NFR-2). The clock runs continuously from
        // boot, so this is measured even when the rhythm is stopped — it is the
        // timebase the beat LED and drums slave to.
        jitter_.record(static_cast<uint32_t>(now_us - nextClockUs_));

        if (state_.config.midi_clock_enabled) {
            out_.push(midi::makeSystem(midi::SYSTEM_CLOCK));
        }
        // Beat boundary every 24 ticks: the beat LED follows the master clock.
        if (clockTickIndex_ % 24 == 0 && state_.config.bpm_indicator) {
            state_.ledIndicator.flash = true;
        }
        clockTickIndex_++;
        nextClockUs_ += tick;
    }
}

void RhythmEngine::update(uint64_t now_us) {
    // 1. Master clock first: it is never preempted or skipped.
    advanceClock(now_us);

    // 2. Rhythm start/stop (slaved to the clock in start()).
    const RhythmParams& rp = state_.pendingRhythm;
    bool wantRun = rp.enabled && !patterns_.empty();
    if (wantRun && !running_) {
        start(now_us);
    } else if (!wantRun && running_) {
        stop();
    }

    if (!running_) return;

    const RhythmPattern* pat = currentPattern();
    if (!pat) return;

    // Steps are exactly CLOCK_TICKS_PER_STEP (=6) clock ticks, so the drums stay
    // phase-locked to the clock (using stepUs() directly would drift by the
    // integer-division remainder each step).
    uint32_t base = clockTickUs(rp.tempo) * CLOCK_TICKS_PER_STEP;
    int spb = pat->steps_per_bar;

    // Fire every step whose deadline has passed, then schedule the next.
    while (stepDeadlineUs_ <= now_us) {
        fireStep(now_us);
        step_ = (step_ + 1) % static_cast<uint32_t>(spb);
        stepAbs_++;
        if (step_ == 0) barStartUs_ += static_cast<uint64_t>(spb) * base;
        stepDeadlineUs_ = barStartUs_ + stepOffsetUs(static_cast<int>(step_), base, rp.swing);
    }

    // Publish the clock snapshot for Core 0 (chord arp/rhythm sync).
    state_.rhythmClock.running = true;
    state_.rhythmClock.stepAbs = stepAbs_;
    state_.rhythmClock.step = step_;
    state_.rhythmClock.stepsPerBar = spb;
    state_.rhythmClock.beat = step_ / RHYTHM_STEPS_PER_BEAT;
    state_.rhythmClock.nextStepUs = stepDeadlineUs_;
}

void RhythmEngine::start(uint64_t now_us) {
    running_ = true;
    step_ = 0;
    stepAbs_ = 0;

    // Slave the downbeat to the clock's beat grid (every 24 ticks), so the
    // drums, the beat LED, and the clock share one timebase. The clock is always
    // armed by advanceClock() before this is called.
    uint32_t tick = clockTickUs(state_.pendingRhythm.tempo);
    uint64_t ticksToBeat = (24 - (clockTickIndex_ % 24)) % 24;
    uint64_t downbeat = nextClockUs_ + ticksToBeat * tick;
    barStartUs_ = downbeat;
    stepDeadlineUs_ = downbeat;
}

void RhythmEngine::stop() {
    running_ = false;
    // NOTE: the master clock keeps running (its phase is the master timebase
    // and must never reset when the rhythm is toggled off).
    state_.rhythmClock.running = false;
    state_.rhythmClock.nextStepUs = 0;
}

void RhythmEngine::fireStep(uint64_t now_us) {
    const RhythmParams& rp = state_.pendingRhythm;
    const RhythmPattern* pat = currentPattern();
    if (!pat) return;

    if (!rp.muted) {
        auto events = stepEvents(*pat, static_cast<int>(step_));
        for (const auto& e : events) {
            out_.push(midi::makeNoteOn(rp.channel, mapDrumNote(e.note, rp.drums), e.velocity));
        }
    }
}

void RhythmEngine::allNotesOff() {
    stop();
}

void RhythmEngine::jitterStats(PerfStats& out, bool reset) {
    out = jitter_;
    if (reset) jitter_.reset();
}

std::vector<RhythmPattern> loadRhythmPatterns(StorageAdapter& storage) {
    std::vector<RhythmPattern> patterns;
    patterns.reserve(RHYTHM_COUNT);

    for (int i = 0; i < RHYTHM_COUNT; i++) {
        std::string path = "/rhythms/" + std::string(rhythmFileName(i));
        if (!storage.exists(path)) continue;
        std::string raw = storage.readFile(path);
        RhythmPattern p;
        if (parseRhythmPattern(raw, p)) {
            patterns.push_back(p);
        }
    }

    if (patterns.empty()) {
        patterns.push_back(builtinRock1());
    }
    return patterns;
}
