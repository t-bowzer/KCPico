#include "display_manager.h"

#include "chords.h"
#include "naming.h"
#include "presets.h"


namespace {

constexpr int kCols = 16;
constexpr uint64_t PRESET_NAME_IDLE_US = 3000000ULL;  // 3 s after last chord

std::string padTrunc(const std::string& s, int width) {
    if (static_cast<int>(s.size()) >= width) return s.substr(0, width);
    std::string out = s;
    out.append(static_cast<size_t>(width - s.size()), ' ');
    return out;
}

} // namespace


DisplayManager::DisplayManager(StateManager& state, LcdAdapter& lcd)
    : state_(state), lcd_(lcd) {}

void DisplayManager::update(uint64_t now_us) {
    if (screen_ == Screen::Edit || screen_ == Screen::Prompt) {
        if (now_us >= revertDeadlineUs_) {
            screen_ = revertToMenu_ ? Screen::Menu : Screen::Idle;
        }
    }

    std::string l1, l2;
    render(l1, l2, now_us);
    emit(l1, l2);
}

void DisplayManager::showMenu(const std::string& title, const std::string& paramName) {
    screen_       = Screen::Menu;
    revertToMenu_ = false;
    menuTitle_    = title;
    menuParamName_ = paramName;
}

void DisplayManager::selectParam(const std::string& paramName) {
    menuParamName_ = paramName;
    if (screen_ != Screen::Menu) screen_ = Screen::Menu;
}

void DisplayManager::showValue(const std::string& fullName, const std::string& value,
                               bool inMenu, uint64_t now_us) {
    screen_       = Screen::Edit;
    revertToMenu_ = inMenu;
    editFullName_ = fullName;
    editValue_    = value;
    revertDeadlineUs_ = now_us + static_cast<uint64_t>(state_.config.display_revert_ms) * 1000ULL;
}

void DisplayManager::showPrompt(const std::string& text, uint64_t now_us) {
    screen_       = Screen::Prompt;
    revertToMenu_ = false;
    promptText_   = text;
    revertDeadlineUs_ = now_us + static_cast<uint64_t>(state_.config.display_prompt_ms) * 1000ULL;
}

void DisplayManager::showNameEdit(const std::string& text) {
    screen_       = Screen::NameEdit;
    revertToMenu_ = false;
    nameEditText_ = text;
}

void DisplayManager::showError(const std::string& line1, const std::string& line2) {
    screen_      = Screen::Error;
    revertToMenu_ = false;
    errorLine1_  = line1;
    errorLine2_  = line2;
}

void DisplayManager::cancel() {
    screen_       = Screen::Idle;
    revertToMenu_ = false;
}

void DisplayManager::renderIdle(std::string& l1, std::string& l2, uint64_t now_us) const {
    int  bank, slot;
    std::string marker;

    // Top-left slot (9 chars): the preset name while browsing the cursor, or in
    // the main view once no chord has been played for 3 s; otherwise the chord.
    std::string top;
    if (state_.cursorActive) {
        bank   = state_.cursorBank;
        slot   = state_.cursorSlot;
        marker = ">";
        top    = presetDisplayName(state_.cursorPresetName, bank, slot);
    } else {
        bank   = state_.currentBank;
        slot   = state_.currentSlot;
        marker = state_.dirty ? "*" : " ";
        bool idle = (now_us - state_.lastChordUs) >= PRESET_NAME_IDLE_US;
        if (idle) {
            top = presetDisplayName(state_.currentPresetName, bank, slot);
        } else {
            top = state_.selectedChordValid ? chordName(state_.selectedChord) : "--";
        }
    }
    top = padTrunc(top, 9);

    std::string loc = "B" + std::to_string(bank + 1) +
                      ":P" + std::to_string(slot + 1);
    l1 = top + marker + loc;

    l2 = "q=" + std::to_string(state_.pendingRhythm.tempo) +
         " " + rhythmShortCode(state_.pendingRhythm.pattern) +
         " >" + playModeShort(state_.pendingChord.play_mode);
}

void DisplayManager::render(std::string& l1, std::string& l2, uint64_t now_us) const {
    switch (screen_) {
        case Screen::Menu:
            l1 = padTrunc(menuTitle_, kCols);
            l2 = padTrunc(menuParamName_, kCols);
            break;
        case Screen::Edit:
            l1 = padTrunc(editFullName_, kCols);
            l2 = padTrunc(editValue_, kCols);
            break;
        case Screen::Prompt:
            l1 = padTrunc(promptText_, kCols);
            l2 = "Enter=Yes Bk=No";
            break;
        case Screen::NameEdit:
            l1 = padTrunc("Rename", kCols);
            l2 = padTrunc(nameEditText_ + "_", kCols);
            break;
        case Screen::Error:
            l1 = padTrunc(errorLine1_, kCols);
            l2 = padTrunc(errorLine2_, kCols);
            break;
        case Screen::Idle:
        default:
            renderIdle(l1, l2, now_us);
            break;
    }
}

void DisplayManager::emit(const std::string& l1, const std::string& l2) {
    // Pad both lines to the full 16 columns so a shorter line never leaves a
    // stale trailing character from the previous frame on the LCD.
    std::string p1 = padTrunc(l1, kCols);
    std::string p2 = padTrunc(l2, kCols);
    if (p1 == lastLine1_ && p2 == lastLine2_) return;
    lastLine1_ = p1;
    lastLine2_ = p2;
    lcd_.write(p1, p2);
}
