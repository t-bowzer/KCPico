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

    // Returns true if the event was consumed (edit/menu key); false if it
    // should be forwarded to the chord/strum engines.
    bool handleKeyEvent(const KeyEvent& ev, uint64_t now_us);

private:
    StateManager& state_;
    DisplayManager& display_;
    KeymapResolver keymap_;
    std::function<void()> modeChanged_;
    std::function<void()> patternChanged_;

    ParamId currentParam() const;
    void enterOrSwitch(EditMenu menu);
    void exitMenu();
    void selectParam(int fIndex);
    void stepParam(int delta, uint64_t now_us);
    void applyDirect(const KeyAction& a, uint64_t now_us);
};
