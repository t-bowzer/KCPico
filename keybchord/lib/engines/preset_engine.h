#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "base.h"
#include "keymap.h"

class StateManager;
class StorageAdapter;
class DisplayManager;


// Preset navigation, load/save/clear, and the save/clear confirmation prompts
// (spec 4.4 / 5.7). Owns "cursor mode": Home/End and Super+Home/End move a
// transient browsing cursor (leaving any edit menu); Enter loads the cursor;
// Super+1..8 loads directly; the cursor resets after cursor_timeout_ms of
// idleness. Also recomputes the dirty marker (FR-P11) against the active slot.
class PresetEngine {
public:
    PresetEngine(StateManager& state, StorageAdapter& storage, DisplayManager& display);

    // Fired after a load/clear changes the chord play-mode (chord engine
    // releases a latched chord when leaving Held/Rhythm with no key held).
    void setModeChangedCallback(std::function<void()> cb);

    // Returns true if the event was consumed.
    bool handleKeyEvent(const KeyEvent& ev, uint64_t now_us);

    // Auto-cancel the prompt after display_prompt_ms and reset the cursor after
    // cursor_timeout_ms of idleness.
    void update(uint64_t now_us);

    // Recompute state.dirty from the active slot's stored values (FR-P11).
    void recomputeDirty();

    // Load the startup preset from config (B<bank>:P<slot>).
    void loadStartupPreset();

private:
    enum class PendingOp : uint8_t { None, Save, Clear };

    StateManager&  state_;
    StorageAdapter& storage_;
    DisplayManager& display_;
    KeymapResolver keymap_;
    std::function<void()> modeChanged_;

    PendingOp   op_ = PendingOp::None;
    uint64_t    promptDeadlineUs_ = 0;
    std::string promptText_;

    uint64_t cursorDeadlineUs_ = 0;

    void enterCursor();
    void rearmCursor(uint64_t now_us);
    void exitCursor();
    void moveSlot(int dir, uint64_t now_us);
    void moveBank(int dir, uint64_t now_us);
    void loadCursor(uint64_t now_us);
    void loadSlot(int slotIdx, uint64_t now_us);
    void beginPrompt(PendingOp op, uint64_t now_us);
    void confirmPrompt(uint64_t now_us);
    void cancelPrompt();

    std::string locationString(int bank, int slot) const;
};
