#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "chords.h"
#include "params.h"


class StorageAdapter;

// Per-preset chord sequencer (M14, spec 6.11).
//
// A preset owns SEQUENCE_SLOTS sequences, stored together in one compact JSON
// file at /sequences/b<bank+1>p<slot+1>.json. Each sequence is a loop over
// `bars` bars of `steps_per_bar` 16th-note steps. Events are chord changes:
// each event sustains until the next event (or the loop end).
constexpr int SEQUENCE_SLOTS              = 10;
constexpr int MAX_SEQUENCE_EVENTS         = 64;
constexpr int SEQUENCE_MIN_BARS           = 1;
constexpr int SEQUENCE_MAX_BARS           = 8;
constexpr int SEQUENCE_MIN_STEPS_PER_BAR  = 4;
constexpr int SEQUENCE_MAX_STEPS_PER_BAR  = 64;
constexpr uint8_t SEQUENCE_DEFAULT_BARS   = 4;
constexpr uint8_t SEQUENCE_DEFAULT_STEPS  = 16;

// Recorded chord settings are stored in this fixed order as a compact JSON
// array. `live_mask` bit i means "use the live value for setting i" during
// playback; bit clear means "use the recorded snapshot". Play mode and MIDI
// channel are never captured (playback always forces Held; channel stays live).
enum class SequenceSetting : uint8_t {
    Octave = 0,
    NoteDuration,
    Velocity,
    Pan,
    Voicing,
    ChordRoll,
    MinNotes,
    MinInterval,
    Inversion,
    ArpMode,
    Pattern,
    COUNT
};

constexpr int SEQUENCE_SETTING_COUNT =
    static_cast<int>(SequenceSetting::COUNT);
constexpr uint16_t SEQUENCE_LIVE_MASK_ALL =
    static_cast<uint16_t>((1u << SEQUENCE_SETTING_COUNT) - 1u);

struct SequenceEvent {
    uint16_t step = 0;
    ResolvedChord chord;
};

struct Sequence {
    uint8_t bars = SEQUENCE_DEFAULT_BARS;
    uint8_t steps_per_bar = SEQUENCE_DEFAULT_STEPS;
    ChordParams recorded = ChordParams::defaults();
    uint16_t live_mask = 0;
    std::vector<SequenceEvent> events;

    bool empty() const { return events.empty(); }
    int loopSteps() const {
        return static_cast<int>(bars) * static_cast<int>(steps_per_bar);
    }

    // True when this slot carries no user data (empty, default bars/meter,
    // default recorded settings, no live overrides).
    bool isDefault() const;

    // Adds/replaces the event at `step` (last write wins) and keeps the events
    // sorted. Returns false when the cap is reached and `step` is not already
    // present.
    bool setEvent(uint16_t step, const ResolvedChord& chord);

    // Removes the event at `step` (if any). Returns true when one was removed.
    bool removeEvent(uint16_t step);

    const SequenceEvent* eventAt(uint16_t step) const;

    // Builds the chord parameters used for playback. Starts from `live` (the
    // current pending chord params) and, when `use_recorded` is true, replaces
    // every setting whose live bit is clear with the recorded snapshot.
    // Play mode is forced to Held; the channel is always live.
    ChordParams mergedParams(const ChordParams& live, bool use_recorded) const;
};

struct SequencerLibrary {
    Sequence slots[SEQUENCE_SLOTS];
};

// True when every slot is pristine (no file needs to be written).
bool sequencerLibraryEmpty(const SequencerLibrary& lib);

// Compact chord identity stored in sequence files:
//   id = rootPc * 128 + ChordType * 8 + extensionMask
// extensionMask bit 0 = add9, bit 1 = add11, bit 2 = add13.
uint16_t sequenceChordId(const ResolvedChord& chord);
bool sequenceChordFromId(uint16_t id, ResolvedChord& out);

// JSON parse/serialize. Parsing is lenient: malformed slots/events are skipped;
// a completely malformed document yields an empty library. Serialization is
// compact by default (the flash budget assumes no pretty whitespace).
bool parseSequenceFile(const std::string& json, SequencerLibrary& out);
std::string serializeSequenceFile(const SequencerLibrary& lib,
                                  bool pretty = false);

// Storage helpers. A missing/corrupt file loads as an empty library.
SequencerLibrary loadSequenceFile(StorageAdapter& storage, int bank, int slot);
bool saveSequenceFile(StorageAdapter& storage, int bank, int slot,
                      const SequencerLibrary& lib);
bool removeSequenceFile(StorageAdapter& storage, int bank, int slot);

// /sequences/b<bank+1>p<slot+1>.json
std::string sequenceFileName(int bank, int slot);

// Clamps a requested bar count to the supported 1/2/4/8 choices.
uint8_t clampSequenceBars(int bars);
