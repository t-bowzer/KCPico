#pragma once

#include <atomic>
#include <cstdint>
#include <vector>
#include "params.h"
#include "presets.h"
#include "config.h"
#include "chords.h"
#include "keymap_config.h"


struct ActiveNote {
    uint8_t note;
    uint8_t channel;
};

// Snapshot of the Core 1 rhythm scheduler, read by Core 0 (chord arp/rhythm
// sync, FR-R4 / AC-6). Written only by the RhythmEngine (Core 1); read
// best-effort by Core 0. Fields are std::atomic so a cross-core read never
// observes a torn value.
struct RhythmClock {
    std::atomic<bool>     running{false};
    std::atomic<uint32_t> stepAbs{0};        // monotonic step counter
    std::atomic<uint64_t> tickAbs{0};        // most recently emitted 24-PPQN tick
    std::atomic<uint64_t> nextTickUs{0};     // absolute deadline of next tick
    std::atomic<uint32_t> step{0};           // step index within the bar
    std::atomic<int32_t>  stepsPerBar{16};
    std::atomic<uint32_t> beat{0};           // beat index within the bar
    std::atomic<uint64_t> nextStepUs{0};     // absolute deadline of the next step
};

// Beat-flash request (FR-R8) from the Core 1 rhythm scheduler to Core 0. Core 1
// sets `flash` once per beat; Core 0 owns the actual keyboard-LED on/off state
// (it turns the LED on for led_flash_ms) so there is no shared deadline and no
// cross-core torn read that could leave the LED stuck on.
struct LedIndicator {
    std::atomic<bool> flash{false};
};

// Runtime view of the per-preset chord sequencer (M14). Owned by Core 0; the
// SequencerEngine writes it and the edit menu/LCD read it. The selected slot
// and its bar count are mirrored here so the parameter menu can edit them.
struct SequencerState {
    uint8_t  slot = 0;        // 0..9 (Ctrl+1..0)
    uint8_t  bars = 4;        // 1/2/4/8, mirrors the selected sequence
    bool     useRecorded = true;  // global Recorded/Live settings switch
    bool     armed = false;       // waiting for the next bar
    bool     recording = false;
    bool     playing = false;
    uint16_t eventCount = 0;
};

class StateManager {
public:
    // Pending params are edited live on Core 0 (EditEngine/presets); Core 1's
    // RhythmEngine reads pendingRhythm best-effort. Every field is a scalar of
    // <= 32 bits, so each individual read is atomic on the RP2040 — a "torn"
    // snapshot can only shift one field by one edit, which is harmless for a
    // timing hint. No mutex is used on the Core 1 deadline path (NFR-2).
    ChordParams  pendingChord;
    StrumParams  pendingStrum;
    RhythmParams pendingRhythm;
    BassParams   pendingBass;
    MacroConfig  pendingMacros;

    ChordParams  activeChord;
    StrumParams  activeStrum;
    RhythmParams activeRhythm;
    BassParams   activeBass;

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

    // Core 0 chord sequencer runtime state (M14).
    SequencerState sequencer;

    // While `recordGuideActive` is true, the RhythmEngine suppresses the
    // normal pattern. `recordGuideHatsActive` additionally enables the
    // quarter-note closed-hi-hat guide (count-in/recording).
    std::atomic<bool> recordGuideActive{false};
    std::atomic<bool> recordGuideHatsActive{false};
    std::atomic<uint32_t> recordGuideHatsStartAbs{0};

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
    KeymapConfig keymap;

    // Preset display state: the active slot's name and the cursor slot's name
    // (peeked while browsing), plus the timestamp of the last chord trigger
    // (used to switch the LCD's top-left from chord name back to preset name
    // after 3s of idleness).
    std::string currentPresetName;
    std::string cursorPresetName;
    uint64_t    lastChordUs = 0;

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


