#pragma once

#include <cstdint>
#include <vector>

#include "base.h"
#include "chords.h"
#include "keymap.h"
#include "state.h"

class MidiRouter;


class ChordEngine {
public:
    ChordEngine(StateManager& state, MidiRouter& router);

    void handleKeyEvent(const KeyEvent& ev, uint64_t now_us);
    void update(uint64_t now_us);
    void allNotesOff();

private:
    StateManager& state_;
    MidiRouter&   router_;
    KeymapResolver keymap_;

    std::vector<GridCell> heldCells_;
    bool backtickHeld_ = false;

    bool sounding_ = false;
    uint8_t activeChannel_ = 1;
    std::vector<uint8_t> activeNotes_;
    std::vector<uint8_t> prevVoicing_;

    // Currently-sounding chord (for change detection / avoiding re-triggers).
    ResolvedChord currentChord_;
    bool currentChordValid_ = false;

    // Release debounce: re-resolution after a key release is deferred briefly
    // so a multi-key chord released "at once" sustains instead of glitching.
    bool releaseBufferPending_ = false;
    uint64_t releaseBufferDeadlineUs_ = 0;

    bool arpActive_ = false;
    size_t arpIndex_ = 0;
    uint64_t arpNext_us_ = 0;

    void addCell(const GridCell& cell);
    void removeCell(const GridCell& cell);

    void onPress(uint64_t now_us);
    void onRelease(uint64_t now_us);
    void resolveAndTriggerIfChanged(uint64_t now_us, bool force);
    void triggerChord(const ResolvedChord& chord, uint64_t now_us);
    void releaseChord();
    void stepArpeggio(uint64_t now_us);
    void stopSound();

    bool chordChanged(const ResolvedChord& chord) const;
};

