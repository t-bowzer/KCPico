#pragma once

#include <cstdint>
#include <functional>

#include "base.h"
#include "keymap.h"
#include "param_edit.h"
#include "state.h"

class DisplayManager;


// Owns the parameter-edit menus (Chord/Strum/Rhythm) and all parameter
// mutation. F-keys select a parameter inside a menu; +/- (and Page Up/Down)
// change its value; Esc returns to the main menu. In the main menu, the direct
// single-key shortcuts (F1/F5/arrows/F7/F8/F9, +/-, Page Up/Down) edit their
// parameter directly. Chord-grid, backtick, and strum keys are not consumed and
// fall through to the chord/strum engines.
class EditEngine {
public:
    EditEngine(StateManager& state, DisplayManager& display);

    // Fired after the chord play-mode changes (chord engine releases a latched
    // chord when leaving Held/Rhythm with no keys held).
    void setModeChangedCallback(std::function<void()> cb);

    // Fired after the rhythm pattern changes (rhythm engine adopts the
    // pattern's authored swing default).
    void setPatternChangedCallback(std::function<void()> cb);

    // Fired after any parameter mutation (used by the preset engine to refresh
    // the dirty-state marker, FR-P11).
    void setAnyEditCallback(std::function<void()> cb);

    // Returns true if the event was consumed (edit/menu key); false if it
    // should be forwarded to the chord/strum engines.
    bool handleKeyEvent(const KeyEvent& ev, uint64_t now_us);

    // Drives key-hold auto-repeat for large-range parameters.
    void update(uint64_t now_us);

private:
    StateManager& state_;
    DisplayManager& display_;
    KeymapResolver keymap_;
    std::function<void()> modeChanged_;
    std::function<void()> patternChanged_;
    std::function<void()> anyEdit_;

    // Key-hold auto-repeat state (large-range params only).
    uint8_t  repeatUsage_ = 0;
    int      repeatDelta_ = 0;
    ParamId  repeatParam_ = ParamId::COUNT;
    bool     repeatInMenu_ = false;
    uint64_t repeatDeadlineUs_ = 0;

    // Idle timeout: leave an open edit menu after menu_timeout_ms (FR-D2).
    uint64_t menuDeadlineUs_ = 0;

    ParamId currentParam() const;
    void enterOrSwitch(EditMenu menu, uint64_t now_us);
    void exitMenu();
    void selectParam(int fIndex);
    void navigateParam(int delta);
    void applyParamStep(ParamId id, int delta, bool inMenu, uint64_t now_us);
    void armRepeat(uint8_t usage, int delta, ParamId id, bool inMenu, uint64_t now_us);
    void clearRepeat();
    void applyDirect(uint8_t usage, const KeyAction& a, uint64_t now_us);
};
