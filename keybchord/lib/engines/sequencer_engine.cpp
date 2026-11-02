#include "sequencer_engine.h"

#include <algorithm>

#include "chord_engine.h"
#include "display_manager.h"
#include "rhythm.h"

namespace {

constexpr uint8_t HID_USAGE_ENTER     = 0x28;
constexpr uint8_t HID_USAGE_ESC       = 0x29;
constexpr uint8_t HID_USAGE_BACKSPACE = 0x2A;

bool isEnterKey(const KeyAction& a, const KeyEvent& ev) {
    if (a.cmd == KeyCmd::Enter) return true;
    return ev.usage_page == hid_usage::PAGE_KEYBOARD &&
           ev.hid_usage == HID_USAGE_ENTER;
}

} // namespace


SequencerEngine::SequencerEngine(StateManager& state, StorageAdapter& storage,
                                 ChordEngine& chord, DisplayManager& display)
    : state_(state), storage_(storage), chord_(chord), display_(display),
      keymap_(&state.keymap) {}

Sequence& SequencerEngine::selected() {
    if (seqSlot_ < 0) seqSlot_ = 0;
    if (seqSlot_ >= SEQUENCE_SLOTS) seqSlot_ = SEQUENCE_SLOTS - 1;
    return lib_.slots[seqSlot_];
}

const Sequence& SequencerEngine::selected() const {
    int s = seqSlot_;
    if (s < 0) s = 0;
    if (s >= SEQUENCE_SLOTS) s = SEQUENCE_SLOTS - 1;
    return lib_.slots[s];
}

Sequence& SequencerEngine::playingSequence() {
    if (playSlot_ < 0) playSlot_ = 0;
    if (playSlot_ >= SEQUENCE_SLOTS) playSlot_ = SEQUENCE_SLOTS - 1;
    return lib_.slots[playSlot_];
}

const Sequence& SequencerEngine::playingSequence() const {
    int s = playSlot_;
    if (s < 0) s = 0;
    if (s >= SEQUENCE_SLOTS) s = SEQUENCE_SLOTS - 1;
    return lib_.slots[s];
}

void SequencerEngine::syncStateFromSequence() {
    const Sequence& seq = selected();
    state_.sequencer.slot = static_cast<uint8_t>(seqSlot_);
    state_.sequencer.bars = seq.bars;
    state_.sequencer.eventCount = static_cast<uint16_t>(seq.events.size());
    bars_ = seq.bars;
    syncActivityFlags();
}

void SequencerEngine::syncActivityFlags() {
    state_.sequencer.armed = armed_;
    state_.sequencer.recording = recording_;
    state_.sequencer.playing = playing_;
}

void SequencerEngine::loadForPreset(int bank, int slot) {
    stopAll(true);
    prompt_ = PromptKind::None;
    bank_ = bank;
    presetSlot_ = slot;
    lib_ = loadSequenceFile(storage_, bank_, presetSlot_);
    state_.sequencer.slot = 0;
    seqSlot_ = 0;
    playSlot_ = 0;
    syncStateFromSequence();
}

void SequencerEngine::onPresetCleared(int bank, int slot) {
    stopAll(false);
    removeSequenceFile(storage_, bank, slot);
    if (bank_ == bank && presetSlot_ == slot) {
        lib_ = SequencerLibrary();
        seqSlot_ = 0;
        playSlot_ = 0;
        state_.sequencer.slot = 0;
        syncStateFromSequence();
    }
}

void SequencerEngine::selectSlot(int slot, uint64_t now_us) {
    if (slot < 0) slot = 0;
    if (slot >= SEQUENCE_SLOTS) slot = SEQUENCE_SLOTS - 1;
    if (slot == seqSlot_) {
        syncStateFromSequence();
        return;
    }
    if (!playing_) stopAll(true);
    seqSlot_ = slot;
    syncStateFromSequence();
    // During playback the slot above takes effect when the current sequence
    // reaches its loop boundary; playback itself continues uninterrupted.
    showStatus("Seq " + std::to_string(seqSlot_ + 1), "", now_us);
}

void SequencerEngine::setBars(int bars, uint64_t now_us) {
    bars = clampSequenceBars(bars);
    if (recording_ || playing_ || armed_) stopAll(true);

    Sequence& seq = selected();
    seq.bars = static_cast<uint8_t>(bars);
    int loop = seq.loopSteps();
    if (loop > 0) {
        seq.events.erase(
            std::remove_if(seq.events.begin(), seq.events.end(),
                           [loop](const SequenceEvent& ev) {
                               return static_cast<int>(ev.step) >= loop;
                           }),
            seq.events.end());
    }

    state_.sequencer.bars = static_cast<uint8_t>(bars);
    bars_ = static_cast<uint8_t>(bars);
    saveSequenceFile(storage_, bank_, presetSlot_, lib_);
    syncStateFromSequence();
    showStatus("Bars", std::to_string(bars), now_us);
}

void SequencerEngine::syncFromState(uint64_t now_us) {
    int requestedSlot = state_.sequencer.slot;
    if (requestedSlot != seqSlot_) selectSlot(requestedSlot, now_us);

    int requestedBars = state_.sequencer.bars;
    if (requestedBars != bars_) setBars(requestedBars, now_us);
}

void SequencerEngine::planCountIn(uint32_t stepAbs) {
    int spb = state_.rhythmClock.stepsPerBar.load();
    if (spb <= 0) spb = SEQUENCE_DEFAULT_STEPS;
    countInStepsPerBar_ = static_cast<uint8_t>(clamp<int>(
        spb, SEQUENCE_MIN_STEPS_PER_BAR, SEQUENCE_MAX_STEPS_PER_BAR));
    uint32_t spb32 = countInStepsPerBar_;
    uint32_t toBar = (spb32 - (stepAbs % spb32)) % spb32;
    countInStartAbs_ = stepAbs + toBar;
    // Exactly one full count-in bar.
    recordStartAbs_ = countInStartAbs_ + spb32;
    countInPlanned_ = true;
}

void SequencerEngine::enableRhythmMuted() {
    if (state_.pendingRhythm.enabled) return;
    state_.pendingRhythm.enabled = true;
    state_.pendingRhythm.muted = true;
}

void SequencerEngine::startRecordArm(uint64_t now_us) {
    enableRhythmMuted();

    // Allow switching from a play-arm to record-arm.
    armed_ = true;
    armIsRecord_ = true;
    countInPlanned_ = false;

    // Mute the pattern now. The RhythmEngine gates the hats by absolute step so
    // the first count-in hat can never be missed or played early. Plan now if
    // the rhythm is already running; otherwise plan when it starts.
    state_.recordGuideActive.store(true);
    state_.recordGuideHatsActive.store(true);
    state_.recordGuideHatsStartAbs.store(0);
    if (state_.rhythmClock.running.load()) {
        uint32_t stepAbs = state_.rhythmClock.stepAbs.load();
        planCountIn(stepAbs);
        state_.recordGuideHatsStartAbs.store(countInStartAbs_);
    }
    syncActivityFlags();
    showPersistentStatus("Rec arm", "Wait bar", now_us);
}

void SequencerEngine::armRecord(uint64_t now_us) {
    if (recording_) {
        stopRecording(true, now_us, "Seq saved");
        return;
    }
    if (playing_) stopPlayback();
    if (armed_ && armIsRecord_) {
        armed_ = false;
        countInPlanned_ = false;
        state_.recordGuideActive.store(false);
        state_.recordGuideHatsActive.store(false);
        state_.recordGuideHatsStartAbs.store(0);
        syncActivityFlags();
        showStatus("Rec cancel", "", now_us);
        return;
    }

    if (!selected().empty()) {
        // Replacing an existing take is destructive: confirm first. Cancel a
        // pending play-arm so it cannot fire while the prompt is showing.
        if (armed_) {
            armed_ = false;
            armIsRecord_ = false;
            syncActivityFlags();
        }
        beginOverwritePrompt(now_us);
        return;
    }

    startRecordArm(now_us);
}

void SequencerEngine::armPlay(uint64_t now_us) {
    if (recording_) {
        // The persistent recording screen is already showing; don't replace it
        // with a transient message.
        return;
    }
    if (playing_) {
        stopPlayback();
        showStatus("Seq stop", "", now_us);
        return;
    }
    if (selected().empty()) {
        showStatus("Seq empty", "", now_us);
        return;
    }
    if (armed_ && !armIsRecord_) {
        armed_ = false;
        syncActivityFlags();
        showStatus("Play cancel", "", now_us);
        return;
    }
    enableRhythmMuted();
    // Switching from a record count-in to playback cancels the guide.
    state_.recordGuideActive.store(false);
    state_.recordGuideHatsActive.store(false);
    state_.recordGuideHatsStartAbs.store(0);
    countInPlanned_ = false;
    armed_ = true;
    armIsRecord_ = false;
    armStepAbs_ = state_.rhythmClock.stepAbs.load();
    syncActivityFlags();
    showStatus("Play arm", "Wait bar", now_us);
}

void SequencerEngine::toggleRecord(uint64_t now_us) {
    armRecord(now_us);
}

void SequencerEngine::togglePlay(uint64_t now_us) {
    armPlay(now_us);
}

void SequencerEngine::startRecording(uint64_t now_us) {
    armed_ = false;
    playing_ = false;
    recording_ = true;

    // recordStartAbs_ was planned when the count-in began, so the first
    // recorded step is the bar boundary even if Core 0 noticed it a loop late.
    recordBars_ = state_.sequencer.bars;
    if (recordBars_ == 0) recordBars_ = SEQUENCE_DEFAULT_BARS;
    recordBars_ = clampSequenceBars(recordBars_);
    recordStepsPerBar_ = countInStepsPerBar_;
    recordSettings_ = state_.pendingChord;
    lastRecordBar_ = 1;

    Sequence& seq = selected();
    uint16_t liveMask = seq.live_mask;
    seq = Sequence();
    seq.bars = recordBars_;
    seq.steps_per_bar = recordStepsPerBar_;
    seq.recorded = recordSettings_;
    seq.live_mask = liveMask;

    // A chord already held/latched at the downbeat becomes the first event.
    if (chord_.liveChordActive() && state_.selectedChordValid) {
        seq.setEvent(0, state_.selectedChord);
    }

    syncStateFromSequence();
    syncActivityFlags();
    showPersistentStatus("Recording 1/" + std::to_string(recordBars_), "",
                         now_us);
}

void SequencerEngine::startPlayback(uint64_t now_us) {
    armed_ = false;
    recording_ = false;
    playing_ = true;
    playSlot_ = seqSlot_;
    state_.recordGuideActive.store(false);
    // stepAbs is the *next* step index. The step we want (0) has not fired yet;
    // process it when the snapshot advances past playStartAbs_.
    playStartAbs_ = state_.rhythmClock.stepAbs.load();
    lastProcessedAbs_ = playStartAbs_;
    playFirstPending_ = true;
    syncActivityFlags();
    showStatus("Seq " + std::to_string(seqSlot_ + 1), "play", now_us);
}

void SequencerEngine::stopRecording(bool save, uint64_t now_us,
                                    const char* message) {
    if (!recording_) {
        armed_ = false;
        return;
    }
    recording_ = false;
    state_.recordGuideActive.store(false);
    state_.recordGuideHatsActive.store(false);
    state_.recordGuideHatsStartAbs.store(0);
    syncActivityFlags();

    if (save) {
        bool ok = saveSequenceFile(storage_, bank_, presetSlot_, lib_);
        if (message && now_us != 0) {
            showStatus(ok ? message : "Seq save fail", "", now_us);
        }
    } else {
        // Discard the in-progress take by reloading the last saved file.
        lib_ = loadSequenceFile(storage_, bank_, presetSlot_);
    }
    syncStateFromSequence();
}

void SequencerEngine::stopPlayback() {
    if (!playing_) return;
    playing_ = false;
    playFirstPending_ = false;
    chord_.releaseSequenceChord();
    syncActivityFlags();
}

void SequencerEngine::stopAll(bool saveRecording) {
    bool hadGuide = state_.recordGuideActive.load();
    state_.recordGuideActive.store(false);
    state_.recordGuideHatsActive.store(false);
    state_.recordGuideHatsStartAbs.store(0);
    countInPlanned_ = false;
    if (recording_) stopRecording(saveRecording, 0, nullptr);
    armed_ = false;
    stopPlayback();
    prompt_ = PromptKind::None;
    syncActivityFlags();
    if (hadGuide && state_.editMenu == EditMenu::None) {
        display_.cancel();
    }
}

void SequencerEngine::processStep(uint32_t step, uint64_t now_us) {
    if (chord_.liveChordActive()) return;

    const Sequence& seq = playingSequence();
    const SequenceEvent* ev = seq.eventAt(static_cast<uint16_t>(step));
    if (!ev) return;

    ChordParams p = seq.mergedParams(state_.pendingChord,
                                     state_.sequencer.useRecorded);
    chord_.playSequenceChord(ev->chord, p, now_us);
}

void SequencerEngine::onLiveChord(const ResolvedChord& chord, uint64_t now_us) {
    if (!recording_ || prompt_ != PromptKind::None) return;
    if (!state_.rhythmClock.running.load()) return;

    uint32_t nextAbs = state_.rhythmClock.stepAbs.load();
    uint32_t loop = static_cast<uint32_t>(recordBars_) * recordStepsPerBar_;
    if (loop == 0 || nextAbs == 0) return;

    // stepAbs is the upcoming step. Decide whether the press is nearest the
    // step that just fired or the upcoming step.
    uint32_t stepDur = stepUs(state_.pendingRhythm.tempo);
    uint64_t next = state_.rhythmClock.nextStepUs.load();
    uint32_t chosenAbs = nextAbs - 1;
    if (next > 0 && next > now_us && (next - now_us) < stepDur / 2) {
        chosenAbs = nextAbs;
    }
    if (chosenAbs < recordStartAbs_) return;
    uint32_t rel = chosenAbs - recordStartAbs_;
    if (rel >= loop) return;

    Sequence& seq = selected();
    if (!seq.setEvent(static_cast<uint16_t>(rel), chord)) {
        stopRecording(true, now_us, "Seq full");
        return;
    }
    state_.sequencer.eventCount =
        static_cast<uint16_t>(seq.events.size());
}

void SequencerEngine::beginClearPrompt(uint64_t now_us) {
    if (recording_ || playing_ || armed_) stopAll(true);
    prompt_ = PromptKind::Clear;
    promptDeadlineUs_ = now_us +
        static_cast<uint64_t>(state_.config.display_prompt_ms) * 1000ULL;
    display_.showPrompt("Clear Seq " + std::to_string(seqSlot_ + 1) + "?", now_us);
}

void SequencerEngine::confirmClear(uint64_t now_us) {
    prompt_ = PromptKind::None;
    selected() = Sequence();
    // Keep the other slots in this preset's file; saveSequenceFile removes the
    // file only when every slot is empty/default.
    bool ok = saveSequenceFile(storage_, bank_, presetSlot_, lib_);
    syncStateFromSequence();
    showStatus(ok ? "Seq cleared" : "Seq save fail", "", now_us);
}

void SequencerEngine::beginOverwritePrompt(uint64_t now_us) {
    prompt_ = PromptKind::Overwrite;
    promptDeadlineUs_ = now_us +
        static_cast<uint64_t>(state_.config.display_prompt_ms) * 1000ULL;
    display_.showPrompt("Overwrite Seq in",
                        "slot " + std::to_string(seqSlot_ + 1) + "? Ent/Bk",
                        now_us);
}

void SequencerEngine::confirmOverwrite(uint64_t now_us) {
    prompt_ = PromptKind::None;
    display_.cancel();
    startRecordArm(now_us);
}

void SequencerEngine::cancelPrompt() {
    prompt_ = PromptKind::None;
    display_.cancel();
}

bool SequencerEngine::handlePrompt(const KeyEvent& ev, uint64_t now_us) {
    if (!ev.pressed) return true;
    KeyAction a = keymap_.resolve(ev.hid_usage, ev.modifiers, ev.usage_page);
    if (isEnterKey(a, ev)) {
        if (prompt_ == PromptKind::Clear) {
            confirmClear(now_us);
        } else if (prompt_ == PromptKind::Overwrite) {
            confirmOverwrite(now_us);
        }
        return true;
    }
    if (ev.usage_page == hid_usage::PAGE_KEYBOARD &&
        (ev.hid_usage == HID_USAGE_ESC ||
         ev.hid_usage == HID_USAGE_BACKSPACE)) {
        cancelPrompt();
        return true;
    }
    // Any other key cancels the prompt and is consumed.
    cancelPrompt();
    return true;
}

bool SequencerEngine::handleKeyEvent(const KeyEvent& ev, uint64_t now_us) {
    if (prompt_ != PromptKind::None) return handlePrompt(ev, now_us);

    KeyAction a = keymap_.resolve(ev.hid_usage, ev.modifiers, ev.usage_page);
    if (!ev.pressed) {
        // Consume Ctrl+number key-up so a strum held before Ctrl is released
        // cleanly by the main loop.
        return a.cmd == KeyCmd::SequenceSelect;
    }

    switch (a.cmd) {
        case KeyCmd::SequenceSelect:
            selectSlot(a.slot, now_us);
            return true;
        case KeyCmd::SequenceRecord:
            toggleRecord(now_us);
            return true;
        case KeyCmd::SequencePlay:
            togglePlay(now_us);
            return true;
        case KeyCmd::SequenceClear:
            beginClearPrompt(now_us);
            return true;
        default:
            return false;
    }
}

void SequencerEngine::update(uint64_t now_us) {
    syncFromState(now_us);

    if (prompt_ != PromptKind::None && now_us >= promptDeadlineUs_) {
        cancelPrompt();
    }

    if (!state_.pendingRhythm.enabled) {
        if (recording_) {
            stopRecording(true, now_us, "Rhythm off");
        } else if (playing_ || armed_) {
            armed_ = false;
            countInPlanned_ = false;
            state_.recordGuideActive.store(false);
            state_.recordGuideHatsActive.store(false);
            state_.recordGuideHatsStartAbs.store(0);
            stopPlayback();
            syncActivityFlags();
            showStatus("Rhythm off", "", now_us);
        }
        return;
    }

    const bool running = state_.rhythmClock.running.load();
    const uint32_t stepAbs = state_.rhythmClock.stepAbs.load();

    if (armed_) {
        if (!running) return;
        if (armIsRecord_) {
            if (!countInPlanned_) {
                planCountIn(stepAbs);
                state_.recordGuideHatsStartAbs.store(countInStartAbs_);
            }
            if (stepAbs < countInStartAbs_) {
                // Pattern is muted; the RhythmEngine's absolute-step gate keeps
                // the hats silent until the count-in bar.
                showPersistentStatus("Rec arm", "Wait bar", now_us);
            } else if (stepAbs < recordStartAbs_) {
                showPersistentStatus("Count-in", "1 bar", now_us);
            } else {
                startRecording(now_us);
            }
        } else if (stepAbs != armStepAbs_ &&
                   state_.rhythmClock.step.load() == 0) {
            startPlayback(now_us);
        }
        return;
    }

    if (recording_) {
        if (!running) {
            stopRecording(true, now_us, "Rhythm off");
            return;
        }
        uint32_t loop = static_cast<uint32_t>(recordBars_) * recordStepsPerBar_;
        if (loop > 0) {
            if (stepAbs <= recordStartAbs_) return;   // no step fired yet
            uint32_t rel = (stepAbs - 1) - recordStartAbs_;
            if (rel >= loop) {
                stopRecording(true, now_us, "Seq saved");
                return;
            }
            int bar = static_cast<int>(rel / recordStepsPerBar_) + 1;
            if (bar != lastRecordBar_) {
                lastRecordBar_ = bar;
                showPersistentStatus("Recording " + std::to_string(bar) + "/" +
                                         std::to_string(recordBars_),
                                     "", now_us);
            }
            // Stop on the next step after the last recorded step, giving the
            // final press a chance to arrive.
            if ((stepAbs - recordStartAbs_) >= loop) {
                stopRecording(true, now_us, "Seq saved");
                return;
            }
        }
        return;
    }

    if (!playing_) return;

    int loop = playingSequence().loopSteps();
    if (loop <= 0) {
        stopPlayback();
        return;
    }
    if (!running) {
        stopPlayback();
        showStatus("Rhythm off", "", now_us);
        return;
    }
    if (playFirstPending_) {
        if (stepAbs <= playStartAbs_) return;   // downbeat has not fired yet
        processStep(0, now_us);
        lastProcessedAbs_ = playStartAbs_;
        playFirstPending_ = false;
    }

    // stepAbs is the next step; the step that just fired is stepAbs-1.
    if (stepAbs <= lastProcessedAbs_ + 1) return;
    uint32_t firedAbs = stepAbs - 1;
    uint32_t delta = firedAbs - lastProcessedAbs_;
    if (delta > 4) {
        // After a long stall, skip intermediate steps rather than firing a
        // burst of chord changes in one loop iteration.
        lastProcessedAbs_ = firedAbs - 1;
        delta = 1;
    }
    for (uint32_t i = 1; i <= delta; i++) {
        uint32_t abs = lastProcessedAbs_ + i;
        loop = playingSequence().loopSteps();
        if (loop <= 0) {
            stopPlayback();
            return;
        }
        uint32_t step = (abs - playStartAbs_) % static_cast<uint32_t>(loop);

        // A slot selected during playback takes over when the current loop
        // wraps to step 0. Selection itself is immediate, but the old sequence
        // always finishes its current pass.
        if (step == 0 && abs != playStartAbs_ && playSlot_ != seqSlot_) {
            Sequence& next = selected();
            if (next.empty()) {
                stopPlayback();
                showStatus("Seq empty", "", now_us);
                return;
            }
            playSlot_ = seqSlot_;
            playStartAbs_ = abs;
            // Do not let the old sequence's held chord bleed into the new one
            // when the new sequence has no event on its downbeat.
            if (!next.eventAt(0)) chord_.releaseSequenceChord();
            loop = next.loopSteps();
            if (loop <= 0) {
                stopPlayback();
                return;
            }
            step = 0;
            showStatus("Seq " + std::to_string(playSlot_ + 1), "play", now_us);
        }
        processStep(step, now_us);
    }
    lastProcessedAbs_ = firedAbs;
}

void SequencerEngine::showStatus(const std::string& line1,
                                 const std::string& line2,
                                 uint64_t now_us) {
    bool inMenu = state_.editMenu != EditMenu::None;
    display_.showValue(line1, line2, inMenu, now_us);
}

void SequencerEngine::showPersistentStatus(const std::string& line1,
                                           const std::string& line2,
                                           uint64_t now_us) {
    (void)now_us;
    // Keep menus/dialogs in front; the next bar update will restore the
    // recording status after they close.
    if (state_.editMenu != EditMenu::None) return;
    display_.showPersistent(line1, line2);
}
