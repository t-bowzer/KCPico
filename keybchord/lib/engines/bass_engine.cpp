#include "bass_engine.h"

#include "bass.h"
#include "chord_engine.h"
#include "midi_router.h"
#include "rhythm.h"


namespace {

// Whole/half notes release slightly before the next note's attack so the
// articulation stays clean instead of smearing into the following note.
constexpr uint64_t SUSTAIN_RELEASE_EARLY_US = 30000;

bool isSustainedPattern(BassPattern p) {
    return p == BassPattern::Whole ||
           p == BassPattern::Half ||
           p == BassPattern::HalfAlt;
}

} // namespace


BassEngine::BassEngine(StateManager& state, MidiRouter& router)
    : state_(state), router_(router) {}

void BassEngine::setChordEngine(const ChordEngine* chord) {
    chord_ = chord;
}

void BassEngine::update(uint64_t now_us) {
    const BassParams& b = state_.pendingBass;

    // Release a note whose deadline has passed (percussive note-off FR-B4, or
    // the end of a sustained whole/half note).
    if (noteActive_ && now_us >= offDeadlineUs_) {
        releaseNote();
    }

    if (!b.enabled) {
        if (noteActive_) releaseNote();
        lastStepAbs_ = state_.rhythmClock.stepAbs;
        return;
    }

    bool chordChanged = detectChordChange();

    // The Hold pattern is not beat-driven: it tracks the chord's sounding state.
    if (b.pattern == BassPattern::Hold) {
        updateHold(now_us);
        lastStepAbs_ = state_.rhythmClock.stepAbs;
        return;
    }

    // A new chord re-articulates a sustained whole/half note immediately (so the
    // bass follows the new root mid-measure). Percussive patterns already pick
    // up the new root on the next beat edge.
    if (chordChanged && noteActive_ && isSustainedPattern(b.pattern)) {
        refireRoot(now_us);
    }

    bool running = state_.rhythmClock.running;
    uint32_t stepAbs = state_.rhythmClock.stepAbs;

    if (running && stepAbs != lastStepAbs_) {
        // `stepAbs` is incremented after each fired step, so a beat-boundary
        // step (0,4,8,12...) has *just* fired when stepAbs % 4 == 1. Reading
        // the published `step`/`beat` fields instead made the bass sound one
        // 16th note early relative to the drums.
        if (stepAbs % RHYTHM_STEPS_PER_BEAT == 1) {
            uint32_t spb = state_.rhythmClock.stepsPerBar;
            uint32_t beatsPerBar = (spb > 0) ? spb / RHYTHM_STEPS_PER_BEAT : 4;
            uint32_t beat = ((stepAbs - 1) / RHYTHM_STEPS_PER_BEAT) % beatsPerBar;
            fireBeat(beat, now_us, beatsPerBar);
        }
    }

    lastStepAbs_ = stepAbs;
}

void BassEngine::fireBeat(uint32_t beat, uint64_t now_us, uint32_t beatsPerBar) {
    const BassParams& b = state_.pendingBass;
    if (state_.pendingChord.play_mode == PlayMode::Silent) return;  // FR-B1
    if (!state_.selectedChordValid) return;

    int offset = bassOffsetForPattern(b.pattern, state_.selectedChord.type,
                                      static_cast<int>(beat), beatsPerBar);
    if (offset < 0) return;  // rest on this beat

    // Monophonic: release the previous note before the new one.
    if (noteActive_) releaseNote();

    const ResolvedChord& chord = state_.selectedChord;
    int note = rootMidi(chord.rootPc, state_.config.base_root_midi, b.octave) + offset;
    if (note < 0) note = 0;
    if (note > 127) note = 127;

    router_.noteOn(b.channel, static_cast<uint8_t>(note), b.velocity);
    note_ = static_cast<uint8_t>(note);
    noteActive_ = true;
    offDeadlineUs_ = sustainDeadline(now_us, beat, beatsPerBar);
}

void BassEngine::updateHold(uint64_t now_us) {
    const BassParams& b = state_.pendingBass;

    bool chordHeld = state_.selectedChordValid &&
                     state_.pendingChord.play_mode != PlayMode::Silent &&
                     chord_ != nullptr && chord_->isSounding();

    if (chordHeld) {
        int note = rootNote();
        if (!noteActive_) {
            router_.noteOn(b.channel, static_cast<uint8_t>(note), b.velocity);
            note_ = static_cast<uint8_t>(note);
            noteActive_ = true;
            offDeadlineUs_ = UINT64_MAX;  // sustains until the chord is released
        } else if (note_ != static_cast<uint8_t>(note)) {
            // Chord changed while still sounding: follow the new root.
            releaseNote();
            router_.noteOn(b.channel, static_cast<uint8_t>(note), b.velocity);
            note_ = static_cast<uint8_t>(note);
            noteActive_ = true;
            offDeadlineUs_ = UINT64_MAX;
        }
    } else if (noteActive_) {
        releaseNote();
    }
}

bool BassEngine::detectChordChange() {
    if (!state_.selectedChordValid) {
        lastChordValid_ = false;
        return false;
    }
    const ResolvedChord& c = state_.selectedChord;
    bool changed = !lastChordValid_ || c.rootPc != lastRootPc_ || c.type != lastType_;
    lastRootPc_ = c.rootPc;
    lastType_ = c.type;
    lastChordValid_ = true;
    return changed;
}

void BassEngine::refireRoot(uint64_t now_us) {
    const BassParams& b = state_.pendingBass;
    if (noteActive_) releaseNote();

    int note = rootNote();
    router_.noteOn(b.channel, static_cast<uint8_t>(note), b.velocity);
    note_ = static_cast<uint8_t>(note);
    noteActive_ = true;

    int spb = state_.rhythmClock.stepsPerBar.load();
    uint32_t beatsPerBar = (spb > 0) ? spb / RHYTHM_STEPS_PER_BEAT : 4;
    offDeadlineUs_ = sustainDeadline(now_us, 0, beatsPerBar);
}

int BassEngine::rootNote() const {
    const BassParams& b = state_.pendingBass;
    const ResolvedChord& chord = state_.selectedChord;
    int note = rootMidi(chord.rootPc, state_.config.base_root_midi, b.octave);
    if (note < 0) note = 0;
    if (note > 127) note = 127;
    return note;
}

uint64_t BassEngine::sustainDeadline(uint64_t now_us, uint32_t beat, uint32_t beatsPerBar) const {
    const BassParams& b = state_.pendingBass;
    int sustain = bassSustainBeats(b.pattern, static_cast<int>(beat),
                                   static_cast<int>(beatsPerBar));
    if (sustain > 0) {
        uint64_t beatUs = static_cast<uint64_t>(stepUs(state_.pendingRhythm.tempo))
                          * RHYTHM_STEPS_PER_BEAT;
        uint64_t total = static_cast<uint64_t>(sustain) * beatUs;
        if (total > SUSTAIN_RELEASE_EARLY_US) {
            total -= SUSTAIN_RELEASE_EARLY_US;
        } else {
            total = 0;
        }
        return now_us + total;
    }
    return now_us + static_cast<uint64_t>(b.note_duration_ms) * 1000ULL;
}

void BassEngine::releaseNote() {
    router_.noteOff(state_.pendingBass.channel, note_);
    noteActive_ = false;
}

void BassEngine::allNotesOff() {
    if (noteActive_) {
        releaseNote();
    }
}
