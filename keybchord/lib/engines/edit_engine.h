#pragma once

#include <cstdint>
#include <functional>

#include "base.h"
#include "keymap.h"
#include "param_edit.h"
#include "state.h"

class DisplayManager;


// Owns the parameter-edit menus (Chord/Strum/Rhythm/Bass/Drum) and all
// parameter mutation. F-keys select a parameter inside a menu; +/- (and
// Page Up/Down / arrows) change its value; Esc returns to the main menu. In the
// main menu, the direct single-key shortcuts (F1-F8, inversions, +/-, Page
// Up/Down) edit their parameter directly. Chord-grid, backtick, held-extension
// arrows, and strum keys are not consumed and fall through to the engines.
class EditEngine {
public:
    EditEngine(StateManager& state, DisplayManager& display);

    void setModeChangedCallback(std::function<void()> cb);
    void setPatternChangedCallback(std::function<void()> cb);
    void setAnyEditCallback(std::function<void()> cb);

    // Audition callback: fired with a drum note code when a drum note parameter
    // changes in the Drum menu, so the user can hear the selected drum sound.
    void setDrumAuditionCallback(std::function<void(uint8_t note)> cb);

    // Returns true if the event was consumed (edit/menu key); false if it
    // should be forwarded to the chord/strum engines.
    bool handleKeyEvent(const KeyEvent& ev, uint64_t now_us);

    void update(uint64_t now_us);

private:
    StateManager& state_;
    DisplayManager& display_;
    KeymapResolver keymap_;
    std::function<void()> modeChanged_;
    std::function<void()> patternChanged_;
    std::function<void()> anyEdit_;
    std::function<void(uint8_t)> drumAudition_;

    uint8_t  repeatUsage_ = 0;
    int      repeatDelta_ = 0;
    ParamId  repeatParam_ = ParamId::COUNT;
    bool     repeatInMenu_ = false;
    uint64_t repeatDeadlineUs_ = 0;

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
