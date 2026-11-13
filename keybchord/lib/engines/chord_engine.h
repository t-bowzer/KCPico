#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "base.h"
#include "chords.h"
#include "chordpattern.h"
#include "keymap.h"
#include "state.h"

class MidiRouter;


class ChordEngine {
public:
    // Chord-key press grouping window (microseconds). Exposed for tests and
    // documentation; USB reports arrive every ~8-10 ms.
    static constexpr uint64_t PRESS_GROUP_US = 15000;

    // If an arpeggiated chord starts this close before the next arp step
    // boundary, defer its first note to the boundary instead of playing it
    // immediately and then again a few milliseconds later.
    static constexpr uint64_t ARP_EARLY_QUANTIZE_US = 30000;

    ChordEngine(StateManager& state, MidiRouter& router);

    void handleKeyEvent(const KeyEvent& ev, uint64_t now_us);
    void update(uint64_t now_us);
    void allNotesOff();

    // Called after the play-mode parameter changes (via the EditEngine): a
    // latched chord is released when leaving Held with no key held.
    void onModeChanged();

    // Called after the arp-mode parameter changes (via the EditEngine): rebuilds
    // the active arpeggio step sequence so a mode switch (e.g. out of Random)
    // never leaves a stale/empty sequence behind.
    void onArpModeChanged();

    // Called after a voicing-related parameter changes (voicing mode, inversion,
    // octave, min notes, min interval). The currently-sounding chord keeps its
    // snapshot (FR-C9), but the next trigger must apply the new voicing even if
    // it resolves to the same chord through an overlapped/release-buffer path.
    void onVoicingChanged();

    // Install the chord pattern library (indices align with the name list).
    void setPatterns(std::vector<ChordPattern> patterns);

    // The pattern at `index` (null if out of range).
    const ChordPattern* patternAt(int index) const;

    // Clears the voice-leading memory (prevVoicing_) so the next triggered chord
    // is voiced from root position at the configured octave. A currently-held or
    // sounding chord is left untouched.
    void resetVoicing();

    // True while a chord is currently sounding (Held/Press/Arp). Used by the
    // walking bass `Hold` pattern.
    bool isSounding() const { return sounding_; }

    // Called whenever a *live* chord is triggered (chord-grid press or
    // extension merge), never for sequencer playback. The sequencer uses this
    // to capture chord changes while recording.
    void setChordTriggeredCallback(
        std::function<void(const ResolvedChord&, uint64_t)> cb);

    // True while a live chord is held or latched. The sequencer skips its own
    // events while this is true so live playing takes priority.
    bool liveChordActive() const;

    // True while the chord currently sounding came from sequencer playback.
    bool sequenceChordSounding() const { return sequenceActive_ && sounding_; }

    // Sequencer playback: sound `chord` with an explicit parameter snapshot,
    // honoring its play mode (Held/Arp/Pattern/etc.), without changing the live
    // pending params. The sequence chord becomes state.selectedChord so the
    // strum plate follows playback. Stops any previous sequence chord first.
    void playSequenceChord(const ResolvedChord& chord, const ChordParams& params,
                           uint64_t now_us);

    // Release sequence playback notes only (no-op if a live chord has taken
    // over). Called when playback stops.
    void releaseSequenceChord();

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
    bool sequenceActive_ = false;
    std::function<void(const ResolvedChord&, uint64_t)> chordTriggered_;
    uint8_t activeChannel_ = 1;
    std::vector<uint8_t> voicing_;       // full chord note set (base + extensions)
    std::vector<uint8_t> activeNotes_;   // notes currently note-on'd (for release)
    std::vector<uint8_t> prevVoicing_;   // previous *base* voicing (voice-leading)

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

    // Press grouping: all chord-key presses within a short window are treated
    // as one chord event, so a combination played a few milliseconds apart (or
    // split across USB reports) sounds once instead of latching the first key's
    // chord and then the combination.
    bool pressBufferPending_ = false;
    uint64_t pressBufferDeadlineUs_ = 0;
    ResolvedChord pressBufferedChord_;
    bool pressBufferedValid_ = false;

    // Set when a key press lands on an invalid combination while a Held chord is
    // latched. The pending switch re-resolves once the stale key(s) are released,
    // so an overlapped chord change (new key pressed before the old is lifted)
    // still lands on the new chord.
    bool pendingSwitch_ = false;

    // A voicing parameter changed since the last trigger. Re-resolved presses
    // that land on the same chord must still re-voice the next trigger.
    bool voicingChangePending_ = false;

    // Arpeggio state.
    bool arpActive_ = false;
    std::vector<int> arpSeq_;    // index sequence for Up/Down/UpDown/Alternating
    size_t arpPos_ = 0;
    uint64_t lastArpTickBucket_ = 0;   // rate-quantised master-clock position
    bool     arpStartPending_ = false; // first note waits for the next boundary
    uint64_t arpStartDeadlineUs_ = 0;
    uint32_t rngState_ = 0x12345678u;   // seeded LCG for Random mode

    // Chord-pattern state (spec 6.10).
    std::vector<ChordPattern> patterns_;
    bool patternActive_ = false;
    bool patternStartPending_ = false;
    uint64_t patternStartDeadlineUs_ = 0;
    uint32_t patternStep_ = 0;        // next step index within the bar
    uint32_t lastPatternClockStep_ = 0;
    uint64_t patternNextUs_ = 0;      // free-run step deadline (rhythm stopped)
    uint64_t patternOffUs_ = 0;       // absolute deadline to release pattern notes

    // Chord roll (VR-9): staggered note-ons not yet fired.
    struct PendingNoteOn { uint8_t note; uint64_t deadline; };
    std::vector<PendingNoteOn> rollPending_;

    void addCell(const GridCell& cell);
    void removeCell(const GridCell& cell);
    void setExt(int which, bool held, uint64_t now_us);

    void onPress(uint64_t now_us);
    void onRelease(uint64_t now_us);
    void flushPressBuffer(uint64_t now_us);
    void resolveAndTriggerIfChanged(uint64_t now_us, bool force);
    void triggerChord(const ResolvedChord& chord, uint64_t now_us);
    void computeVoicing(const ResolvedChord& chord,
                        std::vector<uint8_t>& base,
                        std::vector<uint8_t>& combined);
    void applyExtensionMerge(const ResolvedChord& chord, uint64_t now_us);
    void applyArpExtension(const ResolvedChord& chord, uint64_t now_us);
    void releaseChord();
    void stopSound();

    void beginArp(uint64_t now_us);
    void playFirstArpNote(uint64_t now_us);
    uint64_t nextArpBoundaryUs(uint32_t ticksPerStep) const;
    void stepArpeggio(uint64_t now_us);
    void scheduleRoll(const std::vector<uint8_t>& notes, uint64_t now_us);
    bool followRhythmClock() const;
    bool chordChanged(const ResolvedChord& chord) const;

    const ChordPattern* currentPattern() const;
    void ensurePatternIndexValid();
    void beginPattern(uint64_t now_us);
    void firePatternStep(int stepInBar, uint64_t now_us, uint32_t effectiveSteps);
    void releasePatternNotes();

    static std::vector<int> arpSequence(ArpMode mode, size_t n);
};

// Loads the full chord pattern set from storage: the 9 built-in patterns first
// (in fixed order, self-healing missing/corrupt files from the embedded
// templates), then any additional user-provided /chordpatterns/*.json files
// (sorted by filename, appended at indices CHORD_PATTERN_COUNT..). Falls back to
// a single built-in pattern on total failure. `names` matches `patterns` in
// order.
ChordPatternLibrary loadChordPatterns(StorageAdapter& storage);
