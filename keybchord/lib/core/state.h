#pragma once

#include <cstdint>
#include <vector>
#include "params.h"
#include "presets.h"
#include "config.h"
#include "chords.h"


struct ActiveNote {
    uint8_t note;
    uint8_t channel;
};

// Which parameter the +/- keys currently edit (context-sensitive). None means
// +/- acts on chord octave. Cleared by Esc.
enum class EditTarget : uint8_t {
    None = 0,
    StrumOctave,
    StrumDuration,
    StrumVelocity,
};

// Snapshot of the Core 1 rhythm scheduler, read by Core 0 (chord arp/rhythm
// sync, FR-R4 / AC-6). Written only by the RhythmEngine; read best-effort.
// Individual 32-bit accesses are atomic on the RP2040; a torn snapshot only
// ever shifts one step, which is harmless for a timing hint.
struct RhythmClock {
    bool     running = false;
    uint32_t stepAbs = 0;        // monotonic step counter (increments per step)
    uint32_t step = 0;           // step index within the bar
    int      stepsPerBar = 16;
    uint32_t beat = 0;           // beat index within the bar
    uint64_t nextStepUs = 0;     // absolute deadline of the next step
};

// Desired keyboard-LED state (Scroll Lock BPM indicator, FR-R8). Computed on
// Core 1 by the rhythm scheduler, applied on Core 0 (single-threaded TinyUSB).
struct LedIndicator {
    bool     on = false;
    uint64_t untilUs = 0;        // absolute time the flash should end
    bool     dirty = false;      // changed since last apply
};

class StateManager {
public:
    ChordParams  pendingChord;
    StrumParams  pendingStrum;
    RhythmParams pendingRhythm;

    ChordParams  activeChord;
    StrumParams  activeStrum;
    RhythmParams activeRhythm;

    // Currently selected chord (root/type/extensions from the latest resolve),
    // independent of Held-mode latching. This is what the strum plate derives
    // its note pool from (FR-S4 / FR-S5), including in Silent mode.
    ResolvedChord selectedChord;
    bool           selectedChordValid = false;

    EditTarget editTarget = EditTarget::None;

    // Core 1 rhythm scheduler snapshot + LED indicator (see structs above).
    RhythmClock  rhythmClock;
    LedIndicator ledIndicator;

    std::vector<ActiveNote> activeNotes;

    int  currentBank = 0;
    int  currentSlot = 0;
    bool dirty       = false;

    AppConfig config;

    StateManager();

    void snapshotActive();
    void snapshotChord();

    void noteOn(uint8_t channel, uint8_t note);
    void noteOff(uint8_t channel, uint8_t note);
    void allNotesOff();

    bool isNoteActive(uint8_t channel, uint8_t note) const;

    void loadDefaults();

private:
    void removeNote(uint8_t channel, uint8_t note);
};


