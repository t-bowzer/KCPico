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

    // Called after the play-mode parameter changes (via the EditEngine): a
    // latched chord is released when leaving Held with no key held.
    void onModeChanged();

private:
    StateManager& state_;
    MidiRouter&   router_;
    KeymapResolver keymap_;

    std::vector<GridCell> heldCells_;
    bool backtickHeld_ = false;
    bool ext9Held_  = false;   // Left  arrow (held add9, FR-C7)
    bool ext11Held_ = false;   // Down  arrow (held add11, FR-C7)
    bool ext13Held_ = false;   // Right arrow (held add13, FR-C7)

    bool sounding_ = false;
    uint8_t activeChannel_ = 1;
    std::vector<uint8_t> voicing_;       // full chord note set (for arp stepping)
    std::vector<uint8_t> activeNotes_;   // notes currently note-on'd (for release)
    std::vector<uint8_t> prevVoicing_;

    // Base chord resolved from held grid keys + backtick (no held ext flags);
    // currentChord_ is the merged (base + held extensions) sounding chord.
    ResolvedChord baseChord_;
    bool           baseChordValid_ = false;
    ResolvedChord currentChord_;
    bool           currentChordValid_ = false;

    // Release debounce: re-resolution after a key release is deferred briefly
    // so a multi-key chord released "at once" sustains instead of glitching.
    bool releaseBufferPending_ = false;
    uint64_t releaseBufferDeadlineUs_ = 0;

    // Arpeggio state.
    bool arpActive_ = false;
    std::vector<int> arpSeq_;    // index sequence for Up/Down/UpDown/Alternating
    size_t arpPos_ = 0;
    uint64_t arpNext_us_ = 0;
    uint32_t lastRhythmStep_ = 0;
    uint32_t rngState_ = 0x12345678u;   // seeded LCG for Random mode

    // Chord roll (VR-9): staggered note-ons not yet fired.
    struct PendingNoteOn { uint8_t note; uint64_t deadline; };
    std::vector<PendingNoteOn> rollPending_;

    void addCell(const GridCell& cell);
    void removeCell(const GridCell& cell);
    void setExt(int which, bool held, uint64_t now_us);

    void onPress(uint64_t now_us);
    void onRelease(uint64_t now_us);
    void resolveAndTriggerIfChanged(uint64_t now_us, bool force);
    void triggerChord(const ResolvedChord& chord, uint64_t now_us);
    void releaseChord();
    void stopSound();

    void beginArp(uint64_t now_us);
    void stepArpeggio(uint64_t now_us);
    void scheduleRoll(const std::vector<uint8_t>& notes, uint64_t now_us);
    bool followRhythmClock() const;
    bool chordChanged(const ResolvedChord& chord) const;

    static std::vector<int> arpSequence(ArpMode mode, size_t n);
};
