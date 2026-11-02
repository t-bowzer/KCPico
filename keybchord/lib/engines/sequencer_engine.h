#pragma once

#include <cstdint>
#include <string>

#include "base.h"
#include "keymap.h"
#include "sequence.h"
#include "state.h"

class ChordEngine;
class DisplayManager;
class StorageAdapter;


// Per-preset chord sequencer (M14). Records chord changes on the rhythm clock,
// loops them back, and mixes the recorded chord settings with live ones.
//
// Recording/playback require the rhythm engine to be enabled (it may be muted).
// The sequencer runs on Core 0 and reads the RhythmClock snapshot published by
// the Core 1 scheduler. Live chord input always wins: while a live chord is
// held or latched, sequence events are skipped and playback resumes at the next
// event once the live chord ends.
class SequencerEngine {
public:
    SequencerEngine(StateManager& state, StorageAdapter& storage,
                    ChordEngine& chord, DisplayManager& display);

    // Load the sequences belonging to a preset (called at startup and whenever
    // the active preset changes). Stops any in-flight recording/playback first.
    void loadForPreset(int bank, int slot);

    // The active preset was cleared: stop and remove its sequence file.
    void onPresetCleared(int bank, int slot);

    // Returns true when the event was consumed (sequence controls, slot select,
    // or the clear-confirmation prompt).
    bool handleKeyEvent(const KeyEvent& ev, uint64_t now_us);

    void update(uint64_t now_us);

    // Stop recording (saving a partial take when requested) and playback.
    void stopAll(bool saveRecording);

    // ChordEngine callback entry point (live chord pressed/latched).
    void onLiveChordTriggered(const ResolvedChord& chord, uint64_t now_us) {
        onLiveChord(chord, now_us);
    }

    // Confirmation prompts. While either is active, sequence controls are
    // routed to the prompt (Enter/Mute confirms, Backspace/Esc cancels).
    bool clearPromptActive() const { return prompt_ == PromptKind::Clear; }
    bool overwritePromptActive() const {
        return prompt_ == PromptKind::Overwrite;
    }
    bool armed() const { return armed_; }
    bool recording() const { return recording_; }
    bool playing() const { return playing_; }

    int currentBank() const { return bank_; }
    int currentSlot() const { return seqSlot_; }

    // Exposed for tests / the edit menu.
    const SequencerLibrary& library() const { return lib_; }

private:
    StateManager&   state_;
    StorageAdapter& storage_;
    ChordEngine&    chord_;
    DisplayManager& display_;
    KeymapResolver  keymap_;

    int bank_ = 0;
    int seqSlot_ = 0;   // user-selected slot (takes effect immediately)
    int playSlot_ = 0;  // slot currently looping; switches at the loop boundary
    int presetSlot_ = 0;
    uint8_t bars_ = SEQUENCE_DEFAULT_BARS;
    SequencerLibrary lib_;

    bool     armed_ = false;
    bool     armIsRecord_ = false;
    uint32_t armStepAbs_ = 0;

    bool     recording_ = false;
    uint32_t countInStartAbs_ = 0;
    uint32_t recordStartAbs_ = 0;
    uint8_t  recordBars_ = SEQUENCE_DEFAULT_BARS;
    uint8_t  recordStepsPerBar_ = SEQUENCE_DEFAULT_STEPS;
    uint8_t  countInStepsPerBar_ = SEQUENCE_DEFAULT_STEPS;
    bool     countInPlanned_ = false;
    uint8_t  lastRecordBar_ = 0;
    ChordParams recordSettings_ = ChordParams::defaults();

    bool     playing_ = false;
    bool     playFirstPending_ = false;
    uint32_t playStartAbs_ = 0;
    uint32_t lastProcessedAbs_ = 0;

    enum class PromptKind : uint8_t { None, Clear, Overwrite };
    PromptKind prompt_ = PromptKind::None;
    uint64_t   promptDeadlineUs_ = 0;

    Sequence& selected();
    const Sequence& selected() const;
    Sequence& playingSequence();
    const Sequence& playingSequence() const;

    void syncFromState(uint64_t now_us);
    void selectSlot(int slot, uint64_t now_us);
    void setBars(int bars, uint64_t now_us);

    void planCountIn(uint32_t stepAbs);
    void enableRhythmMuted();
    void startRecordArm(uint64_t now_us);
    void armRecord(uint64_t now_us);
    void armPlay(uint64_t now_us);
    void toggleRecord(uint64_t now_us);
    void togglePlay(uint64_t now_us);
    void startRecording(uint64_t now_us);
    void startPlayback(uint64_t now_us);
    void stopRecording(bool save, uint64_t now_us, const char* message);
    void stopPlayback();

    void processStep(uint32_t step, uint64_t now_us);
    void onLiveChord(const ResolvedChord& chord, uint64_t now_us);

    void beginClearPrompt(uint64_t now_us);
    void confirmClear(uint64_t now_us);
    void beginOverwritePrompt(uint64_t now_us);
    void confirmOverwrite(uint64_t now_us);
    void cancelPrompt();
    bool handlePrompt(const KeyEvent& ev, uint64_t now_us);

    void showStatus(const std::string& line1, const std::string& line2,
                    uint64_t now_us);
    void showPersistentStatus(const std::string& line1,
                              const std::string& line2, uint64_t now_us);
    void syncStateFromSequence();
    void syncActivityFlags();
};
