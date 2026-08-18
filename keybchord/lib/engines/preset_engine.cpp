#include "preset_engine.h"

#include "display_manager.h"
#include "presets.h"
#include "state.h"


namespace {

constexpr uint8_t HID_USAGE_ENTER     = 0x28;
constexpr uint8_t HID_USAGE_ESC       = 0x29;
constexpr uint8_t HID_USAGE_BACKSPACE = 0x2A;

} // namespace


PresetEngine::PresetEngine(StateManager& state, StorageAdapter& storage,
                           DisplayManager& display)
    : state_(state), storage_(storage), display_(display) {}

void PresetEngine::setModeChangedCallback(std::function<void()> cb) {
    modeChanged_ = std::move(cb);
}

std::string PresetEngine::locationString(int bank, int slot) const {
    return "B" + std::to_string(bank + 1) + ":P" + std::to_string(slot + 1);
}

void PresetEngine::recomputeDirty() {
    PresetSlot stored = loadPresetOrDefault(storage_, state_.currentBank,
                                            state_.currentSlot);
    PresetSlot cur = makePreset(state_.pendingChord, state_.pendingStrum,
                                state_.pendingBass, state_.pendingRhythm, stored.name);
    state_.dirty = !cur.sameParams(stored);
}

void PresetEngine::enterCursor() {
    state_.cursorActive = true;
    state_.cursorBank   = state_.currentBank;
    state_.cursorSlot   = state_.currentSlot;
    state_.editMenu     = EditMenu::None;  // leave any edit menu
    state_.editParam    = 0;
    display_.cancel();                     // back to idle (shows cursor marker)
}

void PresetEngine::rearmCursor(uint64_t now_us) {
    cursorDeadlineUs_ = now_us +
        static_cast<uint64_t>(state_.config.cursor_timeout_ms) * 1000ULL;
}

void PresetEngine::exitCursor() {
    state_.cursorActive = false;
    display_.cancel();
}

void PresetEngine::moveSlot(int dir, uint64_t now_us) {
    if (!state_.cursorActive) enterCursor();
    int idx = state_.cursorBank * NUM_SLOTS + state_.cursorSlot;
    idx = (idx + dir + NUM_PRESETS) % NUM_PRESETS;
    state_.cursorBank = idx / NUM_SLOTS;
    state_.cursorSlot = idx % NUM_SLOTS;
    rearmCursor(now_us);
}

void PresetEngine::moveBank(int dir, uint64_t now_us) {
    if (!state_.cursorActive) enterCursor();
    state_.cursorBank = (state_.cursorBank + dir + NUM_BANKS) % NUM_BANKS;
    rearmCursor(now_us);
}

void PresetEngine::loadCursor(uint64_t now_us) {
    int bank = state_.cursorBank;
    int slot = state_.cursorSlot;

    PresetSlot stored = loadPresetOrDefault(storage_, bank, slot);
    PlayMode   oldMode = state_.pendingChord.play_mode;

    state_.pendingChord  = stored.chord;
    state_.pendingStrum  = stored.strum;
    state_.pendingBass   = stored.bass;
    state_.pendingRhythm = stored.rhythm;

    state_.currentBank  = bank;
    state_.currentSlot  = slot;
    state_.dirty        = false;
    state_.cursorActive = false;
    state_.editMenu     = EditMenu::None;
    state_.editParam    = 0;

    // Mirror edit behavior: a mode change releases a latched held chord.
    if (oldMode != stored.chord.play_mode && modeChanged_) modeChanged_();

    display_.showValue("Loaded", locationString(bank, slot), false, now_us);
}

void PresetEngine::loadSlot(int slotIdx, uint64_t now_us) {
    // Super+1..8: load slot N in the current (active) bank (FR-P5).
    state_.cursorBank = state_.currentBank;
    state_.cursorSlot = slotIdx;
    loadCursor(now_us);
}

void PresetEngine::beginPrompt(PendingOp op, uint64_t now_us) {
    op_ = op;
    state_.cursorActive = false;  // leaving cursor mode to prompt

    std::string loc = locationString(state_.currentBank, state_.currentSlot);
    promptText_ = (op == PendingOp::Save) ? ("Save " + loc + "?")
                                          : ("Clear " + loc + "?");
    display_.showPrompt(promptText_, now_us);
    promptDeadlineUs_ = now_us +
        static_cast<uint64_t>(state_.config.display_prompt_ms) * 1000ULL;
}

void PresetEngine::confirmPrompt(uint64_t now_us) {
    PendingOp op = op_;
    op_ = PendingOp::None;

    int bank = state_.currentBank;
    int slot = state_.currentSlot;

    if (op == PendingOp::Save) {
        PresetSlot existing = loadPresetOrDefault(storage_, bank, slot);
        PresetSlot out = makePreset(state_.pendingChord, state_.pendingStrum,
                                    state_.pendingBass, state_.pendingRhythm,
                                    existing.name);
        savePreset(storage_, bank, slot, out);
        state_.dirty = false;
        display_.showValue("Saved", locationString(bank, slot), false, now_us);
    } else if (op == PendingOp::Clear) {
        PlayMode oldMode = state_.pendingChord.play_mode;
        state_.pendingChord  = ChordParams::defaults();
        state_.pendingStrum  = StrumParams::defaults();
        state_.pendingBass   = BassParams::defaults();
        state_.pendingRhythm = RhythmParams::defaults();
        savePreset(storage_, bank, slot, PresetSlot::defaults());
        state_.dirty = false;
        if (oldMode != state_.pendingChord.play_mode && modeChanged_) {
            modeChanged_();
        }
        display_.showValue("Cleared", locationString(bank, slot), false, now_us);
    }
}

void PresetEngine::cancelPrompt() {
    op_ = PendingOp::None;
    display_.cancel();
}

void PresetEngine::loadStartupPreset() {
    int bank = 0, slot = 0;
    if (!parsePresetLocation(state_.config.startup_preset, bank, slot)) {
        bank = 0;
        slot = 0;
    }

    state_.currentBank = bank;
    state_.currentSlot = slot;

    PresetSlot stored = loadPresetOrDefault(storage_, bank, slot);
    state_.pendingChord  = stored.chord;
    state_.pendingStrum  = stored.strum;
    state_.pendingBass   = stored.bass;
    state_.pendingRhythm = stored.rhythm;
    state_.dirty = false;
}

bool PresetEngine::handleKeyEvent(const KeyEvent& ev, uint64_t now_us) {
    if (!ev.pressed) return false;

    KeyAction a = keymap_.resolve(ev.hid_usage, ev.modifiers);

    // --- Save/clear prompt active ---
    if (op_ != PendingOp::None) {
        if (ev.hid_usage == HID_USAGE_ENTER) { confirmPrompt(now_us); return true; }
        if (ev.hid_usage == HID_USAGE_BACKSPACE || ev.hid_usage == HID_USAGE_ESC) {
            cancelPrompt();
            return true;
        }
        switch (a.type) {
            case ActionType::ChordKey:
            case ActionType::Backtick:
            case ActionType::StrumKey:
                cancelPrompt();   // play-key cancel (FR-P10)
                return false;     // forward to chord/strum engines
            default:
                break;
        }
        // Any other key re-arms the auto-cancel (FR-P10 "no key press").
        beginPrompt(op_, now_us);
        return true;
    }

    // --- Cursor mode active ---
    if (state_.cursorActive) {
        switch (a.type) {
            case ActionType::PresetPrev:     moveSlot(-1, now_us); return true;
            case ActionType::PresetNext:     moveSlot(+1, now_us); return true;
            case ActionType::PresetBankPrev: moveBank(-1, now_us); return true;
            case ActionType::PresetBankNext: moveBank(+1, now_us); return true;
            case ActionType::PresetLoad:     loadSlot(a.index, now_us); return true;
            case ActionType::PresetSave:     beginPrompt(PendingOp::Save, now_us); return true;
            case ActionType::PresetClear:    beginPrompt(PendingOp::Clear, now_us); return true;
            case ActionType::MenuChord:
            case ActionType::MenuStrum:
            case ActionType::MenuRhythm:
            case ActionType::MenuBass:
                return false;   // switch to an edit menu (EditEngine clears cursor)
            default:
                break;
        }
        if (ev.hid_usage == HID_USAGE_ENTER) { loadCursor(now_us); return true; }
        if (ev.hid_usage == HID_USAGE_ESC)   { exitCursor(); return true; }
        switch (a.type) {
            case ActionType::ChordKey:
            case ActionType::Backtick:
            case ActionType::StrumKey:
                rearmCursor(now_us);   // play keys stay live, keep browsing
                return false;
            default:
                // Any other hotkey cancels browsing and is handled normally.
                exitCursor();
                return false;
        }
    }

    // --- Main state (no cursor, no prompt) ---
    switch (a.type) {
        case ActionType::PresetPrev:     moveSlot(-1, now_us); return true;
        case ActionType::PresetNext:     moveSlot(+1, now_us); return true;
        case ActionType::PresetBankPrev: moveBank(-1, now_us); return true;
        case ActionType::PresetBankNext: moveBank(+1, now_us); return true;
        case ActionType::PresetLoad:     loadSlot(a.index, now_us); return true;
        case ActionType::PresetSave:     beginPrompt(PendingOp::Save, now_us); return true;
        case ActionType::PresetClear:    beginPrompt(PendingOp::Clear, now_us); return true;
        default:
            return false;   // forward to edit/chord/strum engines
    }
}

void PresetEngine::update(uint64_t now_us) {
    if (op_ != PendingOp::None && now_us >= promptDeadlineUs_) {
        cancelPrompt();
    }
    if (state_.cursorActive && now_us >= cursorDeadlineUs_) {
        exitCursor();
    }
}
