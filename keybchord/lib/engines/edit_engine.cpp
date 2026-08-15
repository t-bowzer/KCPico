#include "edit_engine.h"

#include "display_manager.h"


namespace {

constexpr uint8_t HID_USAGE_F10 = 0x43;

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

} // namespace


EditEngine::EditEngine(StateManager& state, DisplayManager& display)
    : state_(state), display_(display) {}

void EditEngine::setModeChangedCallback(std::function<void()> cb) {
    modeChanged_ = std::move(cb);
}

void EditEngine::setPatternChangedCallback(std::function<void()> cb) {
    patternChanged_ = std::move(cb);
}

ParamId EditEngine::currentParam() const {
    if (state_.editMenu == EditMenu::None) return ParamId::COUNT;
    return menuParamAt(state_.editMenu, state_.editParam);
}

void EditEngine::enterOrSwitch(EditMenu menu) {
    if (state_.editMenu == menu) {
        exitMenu();
        return;
    }
    state_.editMenu = menu;
    if (state_.editParam >= menuParamCount(menu)) state_.editParam = 0;
    display_.showMenu(menuTitle(menu), paramShortName(currentParam()));
}

void EditEngine::exitMenu() {
    state_.editMenu = EditMenu::None;
    state_.editParam = 0;
    display_.cancel();
}

void EditEngine::selectParam(int fIndex) {
    if (state_.editMenu == EditMenu::None) return;
    if (fIndex < 0 || fIndex >= menuParamCount(state_.editMenu)) return;
    state_.editParam = fIndex;
    display_.selectParam(paramShortName(currentParam()));
}

void EditEngine::stepParam(int delta, uint64_t now_us) {
    ParamId id = currentParam();
    if (id == ParamId::COUNT) return;
    paramStep(state_, id, delta);
    if (id == ParamId::ChordMode && modeChanged_) modeChanged_();
    if (id == ParamId::RhythmPattern && patternChanged_) patternChanged_();
    display_.showValue(paramFullName(id), paramValueString(state_, id), true, now_us);
}

void EditEngine::applyDirect(const KeyAction& a, uint64_t now_us) {
    auto show = [&](ParamId id) {
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
        case ActionType::ExtToggle9:
            paramCycle(state_, ParamId::ChordAdd9);
            show(ParamId::ChordAdd9);
            break;
        case ActionType::ExtToggle11:
            paramCycle(state_, ParamId::ChordAdd11);
            show(ParamId::ChordAdd11);
            break;
        case ActionType::ExtToggle13:
            paramCycle(state_, ParamId::ChordAdd13);
            show(ParamId::ChordAdd13);
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
            break;
        case ActionType::TempoDown:
            paramStep(state_, ParamId::RhythmTempo, -1);
            show(ParamId::RhythmTempo);
            break;
        case ActionType::RhythmToggle:
            paramCycle(state_, ParamId::RhythmEnable);
            show(ParamId::RhythmEnable);
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
        default:
            break;
    }
}

bool EditEngine::handleKeyEvent(const KeyEvent& ev, uint64_t now_us) {
    if (!ev.pressed) return false;

    KeyAction a = keymap_.resolve(ev.hid_usage, ev.modifiers);
    bool inMenu = state_.editMenu != EditMenu::None;

    switch (a.type) {
        case ActionType::MenuChord:  enterOrSwitch(EditMenu::Chord);  return true;
        case ActionType::MenuStrum:  enterOrSwitch(EditMenu::Strum);  return true;
        case ActionType::MenuRhythm: enterOrSwitch(EditMenu::Rhythm); return true;
        case ActionType::ClearEdit:   // Esc
            if (inMenu) exitMenu();
            return true;
        default:
            break;
    }

    // F10 is a fallback entry into Chord Edit (for keyboards without a Menu key).
    if (ev.hid_usage == HID_USAGE_F10 && !inMenu) {
        enterOrSwitch(EditMenu::Chord);
        return true;
    }

    if (inMenu) {
        int f = functionKeyIndex(ev.hid_usage);
        if (f >= 0) {
            selectParam(f);
            return true;
        }
        int dir = stepDir(a.type);
        if (dir != 0) {
            stepParam(dir, now_us);
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

    // Main menu: forward chord/strum keys; consume and apply everything else.
    switch (a.type) {
        case ActionType::ChordKey:
        case ActionType::Backtick:
        case ActionType::StrumKey:
            return false;
        default:
            applyDirect(a, now_us);
            return true;
    }
}
