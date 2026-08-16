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

// Beat-flash request (FR-R8) from the Core 1 rhythm scheduler to Core 0. Core 1
// sets `flash` once per beat; Core 0 owns the actual keyboard-LED on/off state
// (it turns the LED on for led_flash_ms) so there is no shared deadline and no
// cross-core torn read that could leave the LED stuck on.
struct LedIndicator {
    bool flash = false;
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

    EditMenu editMenu  = EditMenu::None;  // active edit menu (None = main)
    int      editParam = 0;               // selected F-key index within editMenu

    // Core 1 rhythm scheduler snapshot + LED indicator (see structs above).
    RhythmClock  rhythmClock;
    LedIndicator ledIndicator;

    std::vector<ActiveNote> activeNotes;

    int  currentBank = 0;
    int  currentSlot = 0;
    bool dirty       = false;

    // Cursor-mode navigation (M7): a transient browsing position entered by
    // Home/End/Super+Home/End. `currentBank`/`currentSlot` remain the active
    // (last-loaded) slot; the cursor is only meaningful while cursorActive.
    bool cursorActive = false;
    int  cursorBank   = 0;
    int  cursorSlot   = 0;

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


