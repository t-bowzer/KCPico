#pragma once

#include <cstdint>
#include <vector>

#include "base.h"
#include "bass.h"
#include "state.h"

class ChordEngine;
class MidiRouter;
class StorageAdapter;


// Walking bass (FR-B1..B4) + configurable patterns (spec 6.8/6.9): fires on
// each rhythm 16th-note step edge as a short percussive note (or a sustained
// note for whole/half patterns) on its own channel. Patterns are loaded from
// /bass/*.json (built-ins first, then user files). The `Hold` pattern sustains
// the root while the chord is sounding. Silent when the rhythm is not running
// (except Hold, which tracks the chord). Runs on Core 0, observing the Core 1
// rhythm clock snapshot.
class BassEngine {
public:
    BassEngine(StateManager& state, MidiRouter& router);

    // Install the full bass pattern set (indices must align with the bass list).
    void setPatterns(std::vector<BassPattern> patterns);

    // The pattern at `index` (null if out of range).
    const BassPattern* patternAt(int index) const;

    void update(uint64_t now_us);
    void allNotesOff();

    // Used by the Hold pattern to know whether the chord is currently sounding.
    void setChordEngine(const ChordEngine* chord);

private:
    StateManager& state_;
    MidiRouter&   router_;
    const ChordEngine* chord_ = nullptr;

    std::vector<BassPattern> patterns_;

    uint32_t lastStepAbs_ = 0;

    bool     noteActive_ = false;
    uint8_t  note_ = 0;
    uint64_t offDeadlineUs_ = 0;
    int      sustainSteps_ = 0;  // sustain length of the active note (0 = percussive)

    // Last-seen chord identity, to re-articulate sustained/hold notes when the
    // user plays a new chord mid-note.
    int       lastRootPc_ = 0;
    ChordType lastType_   = ChordType::Major;
    bool      lastChordValid_ = false;

    const BassPattern* currentPattern() const;

    void fireStep(uint32_t stepInBar, uint64_t now_us, uint32_t effectiveSteps);
    void updateHold(uint64_t now_us);
    bool detectChordChange();
    void refireRoot(uint64_t now_us);
    void releaseNote();
    int  rootNote() const;
    uint64_t sustainDeadline(uint64_t now_us, int sustainSteps) const;
};

// Loads the full bass pattern set from storage: the 9 built-in patterns first
// (in fixed order, self-healing missing/corrupt files from the embedded
// templates), then any additional user-provided /bass/*.json files (sorted by
// filename, appended at indices BASS_COUNT..). Falls back to a single built-in
// pattern on total failure. `names` matches `patterns` in order.
BassLibrary loadBassPatterns(StorageAdapter& storage);
