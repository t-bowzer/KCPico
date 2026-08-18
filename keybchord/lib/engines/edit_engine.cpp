#include "edit_engine.h"

#include "display_manager.h"


namespace {

constexpr uint8_t HID_USAGE_LEFT  = 0x50;
constexpr uint8_t HID_USAGE_RIGHT = 0x4F;
constexpr uint8_t HID_USAGE_UP    = 0x52;
constexpr uint8_t HID_USAGE_DOWN  = 0x51;

// Key-hold auto-repeat timing (large-range parameters only).
constexpr uint64_t REPEAT_INITIAL_US = 500000;   // 500 ms before first repeat
constexpr uint64_t REPEAT_INTERVAL_US = 80000;   // 80 ms between repeats

int functionKeyIndex(uint8_t usage) {
    if (usage >= 0x3A && usage <= 0x45) return static_cast<int>(usage - 0x3A);
    return -1;
}

int stepDir(ActionType t) {
    switch (t) {
        case ActionType::ChordOctaveUp:
        case ActionType::StrumOctaveUp:
        case ActionType::TempoUp:          return +1;
        case ActionType::ChordOctaveDown:
        case ActionType::StrumOctaveDown:
        case ActionType::TempoDown:        return -1;
        default:                           return 0;
    }
}

// True for drum *note* parameters (auditioned on change); velocities are not.
bool isDrumNoteParam(ParamId id) {
    switch (id) {
        case ParamId::DrumKickNote:
        case ParamId::DrumSnareNote:
        case ParamId::DrumHihatNote:
        case ParamId::DrumOpenHatNote:
            return true;
        default:
            return false;
    }
}

uint8_t drumNoteValue(const StateManager& state, ParamId id) {
    switch (id) {
        case ParamId::DrumKickNote:    return state.pendingRhythm.drums.kick;
        case ParamId::DrumSnareNote:   return state.pendingRhythm.drums.snare;
        case ParamId::DrumHihatNote:   return state.pendingRhythm.drums.hihat;
        case ParamId::DrumOpenHatNote: return state.pendingRhythm.drums.open_hat;
        default:                       return 0;
    }
}

} // namespace


EditEngine::EditEngine(StateManager& state, DisplayManager& display)
    : state_(state), display_(display) {}

void EditEngine::setModeChangedCallback(std::function<void()> cb) {
    modeChanged_ = std::move(cb);
}

void EditEngine::setPatternChangedCallback(std::function<void()> cb) {
    patternChanged_ = std::move(cb);
}

void EditEngine::setAnyEditCallback(std::function<void()> cb) {
    anyEdit_ = std::move(cb);
}

void EditEngine::setDrumAuditionCallback(std::function<void(uint8_t)> cb) {
    drumAudition_ = std::move(cb);
}

ParamId EditEngine::currentParam() const {
    if (state_.editMenu == EditMenu::None) return ParamId::COUNT;
    return menuParamAt(state_.editMenu, state_.editParam);
}

void EditEngine::enterOrSwitch(EditMenu menu, uint64_t now_us) {
    if (state_.editMenu == menu) {
        exitMenu();
        return;
    }
    state_.editMenu = menu;
    state_.cursorActive = false;  // entering an edit menu leaves cursor mode
    if (state_.editParam >= menuParamCount(menu)) state_.editParam = 0;
    menuDeadlineUs_ = now_us +
        static_cast<uint64_t>(state_.config.menu_timeout_ms) * 1000ULL;
    display_.showMenu(menuTitle(menu), paramShortName(currentParam()));
}

void EditEngine::exitMenu() {
    state_.editMenu = EditMenu::None;
    state_.editParam = 0;
    clearRepeat();
    display_.cancel();
}

void EditEngine::selectParam(int fIndex) {
    if (state_.editMenu == EditMenu::None) return;
    if (fIndex < 0 || fIndex >= menuParamCount(state_.editMenu)) return;
    state_.editParam = fIndex;
    display_.selectParam(paramShortName(currentParam()));
}

void EditEngine::navigateParam(int delta) {
    if (state_.editMenu == EditMenu::None) return;
    int count = menuParamCount(state_.editMenu);
    if (count <= 0) return;
    state_.editParam = (state_.editParam + delta + count) % count;
    display_.selectParam(paramShortName(currentParam()));
}

void EditEngine::applyParamStep(ParamId id, int delta, bool inMenu, uint64_t now_us) {
    if (id == ParamId::COUNT) return;
    paramStep(state_, id, delta);
    if (id == ParamId::ChordMode && modeChanged_) modeChanged_();
    if (id == ParamId::RhythmPattern && patternChanged_) patternChanged_();
    if (isDrumNoteParam(id) && drumAudition_) {
        drumAudition_(drumNoteValue(state_, id));
    }
    if (anyEdit_) anyEdit_();
    display_.showValue(paramFullName(id), paramValueString(state_, id), inMenu, now_us);
}

void EditEngine::armRepeat(uint8_t usage, int delta, ParamId id, bool inMenu, uint64_t now_us) {
    if (delta == 0) return;
    if (!isAutoRepeatable(id)) return;
    repeatUsage_ = usage;
    repeatDelta_ = delta;
    repeatParam_ = id;
    repeatInMenu_ = inMenu;
    repeatDeadlineUs_ = now_us + REPEAT_INITIAL_US;
}

void EditEngine::clearRepeat() {
    repeatUsage_ = 0;
    repeatDelta_ = 0;
    repeatParam_ = ParamId::COUNT;
}

void EditEngine::update(uint64_t now_us) {
    if (repeatDelta_ != 0 && now_us >= repeatDeadlineUs_) {
        applyParamStep(repeatParam_, repeatDelta_, repeatInMenu_, now_us);
        repeatDeadlineUs_ = now_us + REPEAT_INTERVAL_US;
        menuDeadlineUs_ = now_us +
            static_cast<uint64_t>(state_.config.menu_timeout_ms) * 1000ULL;
    }
    if (state_.editMenu != EditMenu::None && now_us >= menuDeadlineUs_) {
        exitMenu();
    }
}

void EditEngine::applyDirect(uint8_t usage, const KeyAction& a, uint64_t now_us) {
    auto show = [&](ParamId id) {
        if (anyEdit_) anyEdit_();
        display_.showValue(paramFullName(id), paramValueString(state_, id), false, now_us);
    };

    switch (a.type) {
        case ActionType::PlayModeCycle:
            paramCycle(state_, ParamId::ChordMode);
            if (modeChanged_) modeChanged_();
            show(ParamId::ChordMode);
            break;
        case ActionType::VoicingToggle:
            paramCycle(state_, ParamId::ChordVoicing);
            show(ParamId::ChordVoicing);
            break;
        case ActionType::BassToggle:
            paramCycle(state_, ParamId::BassEnable);
            show(ParamId::BassEnable);
            break;
        case ActionType::RhythmLedToggle:
            paramCycle(state_, ParamId::RhythmLed);
            show(ParamId::RhythmLed);
            break;
        case ActionType::RhythmToggle:
            paramCycle(state_, ParamId::RhythmEnable);
            show(ParamId::RhythmEnable);
            break;
        case ActionType::RhythmClockToggle:
            paramCycle(state_, ParamId::RhythmClock);
            show(ParamId::RhythmClock);
            break;
        case ActionType::RhythmPatternCycle:
            paramCycle(state_, ParamId::RhythmPattern);
            if (patternChanged_) patternChanged_();
            show(ParamId::RhythmPattern);
            break;
        case ActionType::RhythmMute:
            paramCycle(state_, ParamId::RhythmMute);
            show(ParamId::RhythmMute);
            break;
        case ActionType::Inversion1:
            state_.pendingChord.inversion = InversionMode::First;
            show(ParamId::ChordInversion);
            break;
        case ActionType::Inversion2:
            state_.pendingChord.inversion = InversionMode::Second;
            show(ParamId::ChordInversion);
            break;
        case ActionType::Inversion3:
            state_.pendingChord.inversion = InversionMode::Third;
            show(ParamId::ChordInversion);
            break;
        case ActionType::ChordOctaveUp:
            paramStep(state_, ParamId::ChordOctave, +1);
            show(ParamId::ChordOctave);
            break;
        case ActionType::ChordOctaveDown:
            paramStep(state_, ParamId::ChordOctave, -1);
            show(ParamId::ChordOctave);
            break;
        case ActionType::StrumOctaveUp:
            paramStep(state_, ParamId::StrumOctave, +1);
            show(ParamId::StrumOctave);
            break;
        case ActionType::StrumOctaveDown:
            paramStep(state_, ParamId::StrumOctave, -1);
            show(ParamId::StrumOctave);
            break;
        case ActionType::TempoUp:
            paramStep(state_, ParamId::RhythmTempo, +1);
            show(ParamId::RhythmTempo);
            armRepeat(usage, +1, ParamId::RhythmTempo, false, now_us);
            break;
        case ActionType::TempoDown:
            paramStep(state_, ParamId::RhythmTempo, -1);
            show(ParamId::RhythmTempo);
            armRepeat(usage, -1, ParamId::RhythmTempo, false, now_us);
            break;
        default:
            break;
    }
}

bool EditEngine::handleKeyEvent(const KeyEvent& ev, uint64_t now_us) {
    if (!ev.pressed) {
        if (ev.hid_usage == repeatUsage_ && repeatDelta_ != 0) clearRepeat();
        return false;
    }

    KeyAction a = keymap_.resolve(ev.hid_usage, ev.modifiers);
    bool inMenu = state_.editMenu != EditMenu::None;

    if (inMenu) {
        menuDeadlineUs_ = now_us +
            static_cast<uint64_t>(state_.config.menu_timeout_ms) * 1000ULL;
    }

    switch (a.type) {
        case ActionType::MenuChord:  enterOrSwitch(EditMenu::Chord, now_us);  return true;
        case ActionType::MenuStrum:  enterOrSwitch(EditMenu::Strum, now_us);  return true;
        case ActionType::MenuRhythm: enterOrSwitch(EditMenu::Rhythm, now_us); return true;
        case ActionType::MenuBass:   enterOrSwitch(EditMenu::Bass, now_us);   return true;
        case ActionType::ClearEdit:   // Esc
            if (inMenu) exitMenu();
            return true;
        default:
            break;
    }

    if (inMenu) {
        int f = functionKeyIndex(ev.hid_usage);
        if (f >= 0) {
            // F8 inside the Rhythm menu opens the Drum sub-menu.
            if (state_.editMenu == EditMenu::Rhythm && f == 7) {
                enterOrSwitch(EditMenu::Drum, now_us);
                return true;
            }
            selectParam(f);
            return true;
        }
        // Arrow keys: Left/Right navigate parameters, Up/Down change the value.
        if (ev.hid_usage == HID_USAGE_LEFT)  { navigateParam(-1); return true; }
        if (ev.hid_usage == HID_USAGE_RIGHT) { navigateParam(+1); return true; }
        if (ev.hid_usage == HID_USAGE_UP || ev.hid_usage == HID_USAGE_DOWN) {
            int dir = (ev.hid_usage == HID_USAGE_UP) ? +1 : -1;
            applyParamStep(currentParam(), dir, true, now_us);
            armRepeat(ev.hid_usage, dir, currentParam(), true, now_us);
            return true;
        }
        int dir = stepDir(a.type);
        if (dir != 0) {
            applyParamStep(currentParam(), dir, true, now_us);
            armRepeat(ev.hid_usage, dir, currentParam(), true, now_us);
            return true;
        }
        // Keep chord/strum keys live so edits give immediate audible feedback;
        // everything else is still consumed while a menu is open.
        switch (a.type) {
            case ActionType::ChordKey:
            case ActionType::Backtick:
            case ActionType::StrumKey:
                return false;   // forward to chord/strum engines
            default:
                return true;
        }
    }

    // Main menu: forward chord/strum/held-extension keys; consume the rest.
    switch (a.type) {
        case ActionType::ChordKey:
        case ActionType::Backtick:
        case ActionType::StrumKey:
        case ActionType::Ext9:
        case ActionType::Ext11:
        case ActionType::Ext13:
            return false;
        default:
            applyDirect(ev.hid_usage, a, now_us);
            return true;
    }
}
