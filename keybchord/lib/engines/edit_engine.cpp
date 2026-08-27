#include "edit_engine.h"

#include "display_manager.h"
#include "rhythm.h"


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

// True for drum *note* parameters (auditioned on change); velocities are not.
bool isDrumNoteParam(ParamId id) {
    switch (id) {
        case ParamId::DrumKickNote:
        case ParamId::DrumSnareNote:
        case ParamId::DrumHihatNote:
        case ParamId::DrumOpenHatNote:
        case ParamId::DrumRimshotNote:
        case ParamId::DrumClapNote:
        case ParamId::DrumCrashNote:
        case ParamId::DrumRideNote:
        case ParamId::DrumBongoNote:
        case ParamId::DrumCongaLoNote:
        case ParamId::DrumCongaHiNote:
        case ParamId::DrumClaveNote:
        case ParamId::DrumShakerNote:
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
        case ParamId::DrumRimshotNote: return state.pendingRhythm.drums.rimshot;
        case ParamId::DrumClapNote:    return state.pendingRhythm.drums.clap;
        case ParamId::DrumCrashNote:   return state.pendingRhythm.drums.crash;
        case ParamId::DrumRideNote:    return state.pendingRhythm.drums.ride;
        case ParamId::DrumBongoNote:   return state.pendingRhythm.drums.bongo;
        case ParamId::DrumCongaLoNote: return state.pendingRhythm.drums.conga_lo;
        case ParamId::DrumCongaHiNote: return state.pendingRhythm.drums.conga_hi;
        case ParamId::DrumClaveNote:   return state.pendingRhythm.drums.clave;
        case ParamId::DrumShakerNote:  return state.pendingRhythm.drums.shaker;
        default:                       return 0;
    }
}

} // namespace


EditEngine::EditEngine(StateManager& state, DisplayManager& display)
    : state_(state), display_(display), keymap_(&state.keymap) {}

void EditEngine::setModeChangedCallback(std::function<void()> cb) {
    modeChanged_ = std::move(cb);
}

void EditEngine::setArpModeChangedCallback(std::function<void()> cb) {
    arpModeChanged_ = std::move(cb);
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

void EditEngine::setPatternProvider(std::function<const RhythmPattern*(int)> cb) {
    patternProvider_ = std::move(cb);
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
    if (id == ParamId::ChordArpMode && arpModeChanged_) arpModeChanged_();
    if (id == ParamId::RhythmPattern && patternChanged_) patternChanged_();
    if (isDrumNoteParam(id) && drumAudition_) {
        drumAudition_(drumNoteValue(state_, id));
    }
    if (anyEdit_) anyEdit_();
    display_.showValue(paramFullName(id), paramValueString(state_, id), inMenu, now_us);
}

void EditEngine::applyParamAction(const KeyAction& a, uint8_t usage, bool inMenu, uint64_t now_us) {
    ParamId id = a.param;
    if (id == ParamId::COUNT) return;

    switch (a.cmd) {
        case KeyCmd::CycleParam:   paramCycle(state_, id); break;
        case KeyCmd::SetParam:     paramSet(state_, id, a.valueA); break;
        case KeyCmd::IncParam:     paramStep(state_, id, +1); break;
        case KeyCmd::DecParam:     paramStep(state_, id, -1); break;
        case KeyCmd::ToggleParam:  paramToggle(state_, id, a.valueA, a.valueB); break;
        default: return;
    }

    if (id == ParamId::ChordMode && modeChanged_) modeChanged_();
    if (id == ParamId::ChordArpMode && arpModeChanged_) arpModeChanged_();
    if (id == ParamId::RhythmPattern && patternChanged_) patternChanged_();
    if (anyEdit_) anyEdit_();
    display_.showValue(paramFullName(id), paramValueString(state_, id), inMenu, now_us);

    if (a.cmd == KeyCmd::IncParam || a.cmd == KeyCmd::DecParam) {
        int dir = (a.cmd == KeyCmd::IncParam) ? +1 : -1;
        armRepeat(usage, dir, id, inMenu, now_us);
    }
}

void EditEngine::toggleDrumMute(int slot, uint64_t now_us) {
    const RhythmPattern* p = patternProvider_
        ? patternProvider_(state_.pendingRhythm.pattern) : nullptr;
    if (!p) return;
    if (slot < 0 || slot >= static_cast<int>(p->tracks.size())) return;

    const RhythmTrack& t = p->tracks[slot];
    uint8_t note = t.note;

    uint8_t* field = drumVelocityField(state_.pendingRhythm.drums, note);
    std::string name = drumNameForNote(note);
    if (name.empty()) name = t.name.empty() ? ("Drum " + std::to_string(slot + 1)) : t.name;

    if (!field) {
        // Non-standard (pass-through) drum has no velocity override: can't mute.
        display_.showValue(name, "No mute", false, now_us);
        return;
    }

    int idx = drumIndexForNote(note);
    bool nowMuted = false;

    if (idx >= 0 && drumMuteCache_[idx] != 0xFF) {
        *field = drumMuteCache_[idx];      // unmute -> restore pre-mute value
        drumMuteCache_[idx] = 0xFF;
    } else if (*field == param_bounds::DRUM_VELOCITY_OFF) {
        *field = 0;                         // muted with no cache -> unmute to follow
    } else {
        if (idx >= 0) drumMuteCache_[idx] = *field;
        *field = param_bounds::DRUM_VELOCITY_OFF;
        nowMuted = true;
    }

    if (anyEdit_) anyEdit_();
    display_.showValue(name + " Mute", nowMuted ? "On" : "Off", false, now_us);
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

void EditEngine::handleTapTempo(uint64_t now_us) {
    constexpr uint64_t TAP_RESET_US = 2000000ULL;  // 2 s gap starts a fresh set
    constexpr int MAX_TAPS = 8;

    // A gap longer than 2 s between presses starts a new tap sequence.
    if (tapCount_ > 0 && now_us - tapTimes_[tapCount_ - 1] > TAP_RESET_US) {
        tapCount_ = 0;
    }

    if (tapCount_ < MAX_TAPS) {
        tapTimes_[tapCount_++] = now_us;
    } else {
        for (int i = 1; i < MAX_TAPS; i++) tapTimes_[i - 1] = tapTimes_[i];
        tapTimes_[MAX_TAPS - 1] = now_us;
    }

    if (tapCount_ >= 2) {
        uint64_t sum = 0;
        for (int i = 1; i < tapCount_; i++) {
            sum += tapTimes_[i] - tapTimes_[i - 1];
        }
        uint64_t avgUs = sum / static_cast<uint64_t>(tapCount_ - 1);
        if (avgUs > 0) {
            uint64_t bpm = 60000000ULL / avgUs;
            bpm = clamp<uint64_t>(bpm,
                static_cast<uint64_t>(param_bounds::TEMPO_MIN),
                static_cast<uint64_t>(param_bounds::TEMPO_MAX));
            state_.pendingRhythm.tempo = static_cast<uint16_t>(bpm);
        }
    }

    if (anyEdit_) anyEdit_();
    display_.showValue("Tempo", std::to_string(state_.pendingRhythm.tempo),
                       false, now_us);
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

    switch (a.cmd) {
        case KeyCmd::OpenMenu:
            enterOrSwitch(a.menu, now_us);
            return true;
        case KeyCmd::ClearEdit:   // Esc
            if (inMenu) exitMenu();
            return true;
        default:
            break;
    }

    if (inMenu) {
        uint8_t combo = keymapComboMask(ev.modifiers);
        int f = functionKeyIndex(ev.hid_usage);
        if (f >= 0 && combo == 0) {
            // F8 inside the Rhythm menu opens the Drum sub-menu.
            if (state_.editMenu == EditMenu::Rhythm && f == 7) {
                enterOrSwitch(EditMenu::Drum, now_us);
                return true;
            }
            selectParam(f);
            return true;
        }
        // Arrow keys: Up/Down navigate parameters, Left/Right change the value.
        if (ev.hid_usage == HID_USAGE_UP)   { navigateParam(-1); return true; }
        if (ev.hid_usage == HID_USAGE_DOWN) { navigateParam(+1); return true; }
        if (ev.hid_usage == HID_USAGE_LEFT || ev.hid_usage == HID_USAGE_RIGHT) {
            int dir = (ev.hid_usage == HID_USAGE_RIGHT) ? +1 : -1;
            applyParamStep(currentParam(), dir, true, now_us);
            armRepeat(ev.hid_usage, dir, currentParam(), true, now_us);
            return true;
        }
        // Inc/dec shortcuts also step the currently-selected parameter.
        if (a.cmd == KeyCmd::IncParam || a.cmd == KeyCmd::DecParam) {
            int dir = (a.cmd == KeyCmd::IncParam) ? +1 : -1;
            applyParamStep(currentParam(), dir, true, now_us);
            armRepeat(ev.hid_usage, dir, currentParam(), true, now_us);
            return true;
        }
        // Keep chord/strum keys live so edits give immediate audible feedback.
        switch (a.cmd) {
            case KeyCmd::ChordKey:
            case KeyCmd::Backtick:
            case KeyCmd::StrumKey:
                return false;   // forward to chord/strum engines
            case KeyCmd::CycleParam:
            case KeyCmd::SetParam:
            case KeyCmd::ToggleParam:
            case KeyCmd::DrumMute:
                applyParamAction(a, ev.hid_usage, true, now_us);
                return true;
            default:
                return true;   // everything else is still consumed while in a menu
        }
    }

    // Main menu: forward chord/strum/held-extension keys; consume the rest.
    switch (a.cmd) {
        case KeyCmd::ChordKey:
        case KeyCmd::Backtick:
        case KeyCmd::StrumKey:
        case KeyCmd::Ext9:
        case KeyCmd::Ext11:
        case KeyCmd::Ext13:
            return false;
        case KeyCmd::DrumMute:
            toggleDrumMute(a.slot, now_us);
            return true;
        case KeyCmd::TapTempo:
            handleTapTempo(now_us);
            return true;
        case KeyCmd::CycleParam:
        case KeyCmd::SetParam:
        case KeyCmd::IncParam:
        case KeyCmd::DecParam:
        case KeyCmd::ToggleParam:
            applyParamAction(a, ev.hid_usage, false, now_us);
            return true;
        default:
            return true;
    }
}
