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

void RhythmEngine::handleKeyEvent(const KeyEvent& ev, uint64_t now_us) {
    KeyAction a = keymap_.resolve(ev.hid_usage, ev.modifiers);

    switch (a.type) {
        case ActionType::RhythmToggle:
            if (ev.pressed) {
                state_.pendingRhythm.enabled = !state_.pendingRhythm.enabled;
            }
            break;

        case ActionType::RhythmPatternCycle:
            if (ev.pressed && !patterns_.empty()) {
                int idx = (state_.pendingRhythm.pattern + 1)
                          % static_cast<int>(patterns_.size());
                state_.pendingRhythm.pattern = static_cast<uint8_t>(idx);
                // Adopt the pattern's authored swing default (spec 7.2).
                state_.pendingRhythm.swing = patterns_[idx].swing;
                if (running_) start(now_us);
            }
            break;

        case ActionType::RhythmMute:
            if (ev.pressed) state_.pendingRhythm.muted = !state_.pendingRhythm.muted;
            break;

        case ActionType::TempoUp:
            if (ev.pressed) {
                state_.pendingRhythm.tempo = clamp<uint16_t>(
                    state_.pendingRhythm.tempo + 1,
                    param_bounds::TEMPO_MIN, param_bounds::TEMPO_MAX);
            }
            break;
        case ActionType::TempoDown:
            if (ev.pressed) {
                state_.pendingRhythm.tempo = clamp<uint16_t>(
                    state_.pendingRhythm.tempo - 1,
                    param_bounds::TEMPO_MIN, param_bounds::TEMPO_MAX);
            }
            break;

        case ActionType::SwingUp:
            if (ev.pressed) {
                state_.pendingRhythm.swing = static_cast<int8_t>(
                    clamp<int>(static_cast<int>(state_.pendingRhythm.swing) + 5,
                               param_bounds::SWING_MIN, param_bounds::SWING_MAX));
            }
            break;
        case ActionType::SwingDown:
            if (ev.pressed) {
                state_.pendingRhythm.swing = static_cast<int8_t>(
                    clamp<int>(static_cast<int>(state_.pendingRhythm.swing) - 5,
                               param_bounds::SWING_MIN, param_bounds::SWING_MAX));
            }
            break;

        case ActionType::ClockToggle:
            if (ev.pressed) {
                state_.config.midi_clock_enabled = !state_.config.midi_clock_enabled;
            }
            break;
        case ActionType::LedToggle:
            if (ev.pressed) {
                state_.config.bpm_indicator = !state_.config.bpm_indicator;
            }
            break;

        default:
            break;
    }
}

void RhythmEngine::update(uint64_t now_us) {
    const RhythmParams& rp = state_.pendingRhythm;
    bool wantRun = rp.enabled && !patterns_.empty();

    if (wantRun && !running_) {
        start(now_us);
    } else if (!wantRun && running_) {
        stop();
    }

    // MIDI clock streams whenever the clock toggle is on (FR-R7), independent
    // of whether the drums are running — KeybChord is a continuous tempo master.
    handleClock(now_us);

    if (!running_) return;

    const RhythmPattern* pat = currentPattern();
    if (!pat) return;

    uint32_t base = stepUs(rp.tempo);
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
    barStartUs_ = now_us;
    stepDeadlineUs_ = now_us;  // fire the downbeat immediately
}

void RhythmEngine::stop() {
    running_ = false;
    clockOn_ = false;
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

    // LED BPM indicator (FR-R8): flash on each beat, accented on beat 1.
    if (step_ % RHYTHM_STEPS_PER_BEAT == 0) {
        const AppConfig& cfg = state_.config;
        if (cfg.bpm_indicator) {
            bool downbeat = (step_ == 0);
            uint16_t ms = (downbeat && cfg.accent_downbeat)
                              ? cfg.accent_flash_ms
                              : cfg.led_flash_ms;
            state_.ledIndicator.on = true;
            state_.ledIndicator.untilUs = now_us + static_cast<uint64_t>(ms) * 1000ULL;
            state_.ledIndicator.dirty = true;
        }
    }
}

// Streams only the 24 PPQN clock byte (0xF8) whenever the clock toggle is on.
// No Start/Stop/Continue: KeybChord is a pure tempo master (slaves sync their
// BPM; nothing starts their transport).
void RhythmEngine::handleClock(uint64_t now_us) {
    if (!state_.config.midi_clock_enabled) {
        clockOn_ = false;
        nextClockUs_ = 0;
        return;
    }

    if (!clockOn_) {
        clockOn_ = true;
        nextClockUs_ = now_us + clockTickUs(state_.pendingRhythm.tempo);
        return;
    }

    uint32_t tick = clockTickUs(state_.pendingRhythm.tempo);
    while (nextClockUs_ <= now_us) {
        out_.push(midi::makeSystem(midi::SYSTEM_CLOCK));
        nextClockUs_ += tick;
    }
}

void RhythmEngine::allNotesOff() {
    stop();
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
