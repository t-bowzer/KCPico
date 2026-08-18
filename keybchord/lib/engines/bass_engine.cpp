#include "bass_engine.h"

#include "bass.h"
#include "midi_router.h"
#include "rhythm.h"


BassEngine::BassEngine(StateManager& state, MidiRouter& router)
    : state_(state), router_(router) {}

void BassEngine::update(uint64_t now_us) {
    // Percussive note-off (FR-B4).
    if (noteActive_ && now_us >= offDeadlineUs_) {
        router_.noteOff(state_.pendingBass.channel, note_);
        noteActive_ = false;
    }

    bool running = state_.rhythmClock.running;
    uint32_t stepAbs = state_.rhythmClock.stepAbs;

    if (running) {
        if (!wasRunning_ || stepAbs != lastStepAbs_) {
            // Beat boundary: fire on steps 0, 4, 8, ... within the bar.
            uint32_t stepInBar = state_.rhythmClock.step;
            if (stepInBar % RHYTHM_STEPS_PER_BEAT == 0) {
                fireBeat(state_.rhythmClock.beat, now_us);
            }
        }
    }

    wasRunning_ = running;
    lastStepAbs_ = stepAbs;
}

void BassEngine::fireBeat(uint32_t beat, uint64_t now_us) {
    const BassParams& b = state_.pendingBass;
    if (!b.enabled) return;
    if (!state_.selectedChordValid) return;
    if (state_.pendingChord.play_mode == PlayMode::Silent) return;  // FR-B1

    // Monophonic walking line: release the previous note before the new one.
    if (noteActive_) {
        router_.noteOff(b.channel, note_);
        noteActive_ = false;
    }

    int beatsPerBar = state_.rhythmClock.stepsPerBar / RHYTHM_STEPS_PER_BEAT;
    if (beatsPerBar <= 0) beatsPerBar = 4;

    const ResolvedChord& chord = state_.selectedChord;
    int note = bassNote(chord.type, chord.rootPc, state_.config.base_root_midi,
                        b.octave, static_cast<int>(beat), beatsPerBar);
    if (note < 0) note = 0;
    if (note > 127) note = 127;

    router_.noteOn(b.channel, static_cast<uint8_t>(note), b.velocity);
    note_ = static_cast<uint8_t>(note);
    noteActive_ = true;
    offDeadlineUs_ = now_us + static_cast<uint64_t>(b.note_duration_ms) * 1000ULL;
}

void BassEngine::allNotesOff() {
    if (noteActive_) {
        router_.noteOff(state_.pendingBass.channel, note_);
        noteActive_ = false;
    }
}
