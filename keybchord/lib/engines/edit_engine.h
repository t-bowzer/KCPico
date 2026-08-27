#pragma once

#include <cstdint>
#include <functional>

#include "base.h"
#include "keymap.h"
#include "param_edit.h"
#include "state.h"

class DisplayManager;
struct RhythmPattern;


// Owns the parameter-edit menus (Chord/Strum/Rhythm/Bass/Drum) and all
// parameter mutation. F-keys select a parameter inside a menu; Up/Down navigate
// parameters; Left/Right (and the inc/dec shortcuts) change the value; Esc
// returns to the main menu. In the main menu, the configurable keymap actions
// (cycle/set/inc/dec/toggle/drum-mute) edit their parameter directly.
// Chord-grid, backtick, held-extension arrows, and strum keys are not consumed
// and fall through to the engines.
class EditEngine {
public:
    EditEngine(StateManager& state, DisplayManager& display);

    void setModeChangedCallback(std::function<void()> cb);
    void setArpModeChangedCallback(std::function<void()> cb);
    void setPatternChangedCallback(std::function<void()> cb);
    void setAnyEditCallback(std::function<void()> cb);

    // Audition callback: fired with a drum note code when a drum note parameter
    // changes in the Drum menu, so the user can hear the selected drum sound.
    void setDrumAuditionCallback(std::function<void(uint8_t note)> cb);

    // Provider for the rhythm-pattern list (index -> pattern). Used by the
    // Alt+F1..F12 drum-mute shortcuts to resolve the Nth track of the current
    // pattern. May be unset (drum mute then no-ops).
    void setPatternProvider(std::function<const RhythmPattern*(int index)> cb);

    // Returns true if the event was consumed (edit/menu key); false if it
    // should be forwarded to the chord/strum engines.
    bool handleKeyEvent(const KeyEvent& ev, uint64_t now_us);

    void update(uint64_t now_us);

private:
    StateManager& state_;
    DisplayManager& display_;
    KeymapResolver keymap_;
    std::function<void()> modeChanged_;
    std::function<void()> arpModeChanged_;
    std::function<void()> patternChanged_;
    std::function<void()> anyEdit_;
    std::function<void(uint8_t)> drumAudition_;
    std::function<const RhythmPattern*(int)> patternProvider_;

    uint8_t  repeatUsage_ = 0;
    int      repeatDelta_ = 0;
    ParamId  repeatParam_ = ParamId::COUNT;
    bool     repeatInMenu_ = false;
    uint64_t repeatDeadlineUs_ = 0;

    uint64_t menuDeadlineUs_ = 0;

    // Tap-tempo state (Space key): average the intervals of the most recent taps
    // (up to 8) to set the rhythm tempo.
    uint64_t tapTimes_[8] = {0};
    int      tapCount_ = 0;

    // Per-drum mute cache: the pre-mute velocity for each of the 13 standard
    // drums (0xFF = not muted). Keyed by drumIndexForNote().
    uint8_t drumMuteCache_[13] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    };

    ParamId currentParam() const;
    void enterOrSwitch(EditMenu menu, uint64_t now_us);
    void exitMenu();
    void selectParam(int fIndex);
    void navigateParam(int delta);
    void applyParamStep(ParamId id, int delta, bool inMenu, uint64_t now_us);
    void applyParamAction(const KeyAction& a, uint8_t usage, bool inMenu, uint64_t now_us);
    void toggleDrumMute(int slot, uint64_t now_us);
    void armRepeat(uint8_t usage, int delta, ParamId id, bool inMenu, uint64_t now_us);
    void clearRepeat();
    void handleTapTempo(uint64_t now_us);
};
