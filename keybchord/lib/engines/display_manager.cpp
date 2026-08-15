#include "display_manager.h"

#include "chords.h"
#include "naming.h"


namespace {

constexpr int kCols = 16;

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
    render(l1, l2);
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

void DisplayManager::cancel() {
    screen_       = Screen::Idle;
    revertToMenu_ = false;
}

void DisplayManager::renderIdle(std::string& l1, std::string& l2) const {
    std::string chord = state_.selectedChordValid
        ? chordName(state_.selectedChord) : "--";
    chord = padTrunc(chord, 9);

    std::string loc = "B" + std::to_string(state_.currentBank + 1) +
                      ":P" + std::to_string(state_.currentSlot + 1);
    std::string marker = state_.dirty ? "*" : " ";
    l1 = chord + marker + loc;

    l2 = "q=" + std::to_string(state_.pendingRhythm.tempo) +
         " " + rhythmShortCode(state_.pendingRhythm.pattern) +
         " >" + playModeShort(state_.pendingChord.play_mode);
}

void DisplayManager::render(std::string& l1, std::string& l2) const {
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
        case Screen::Idle:
        default:
            renderIdle(l1, l2);
            break;
    }
}

void DisplayManager::emit(const std::string& l1, const std::string& l2) {
    if (l1 == lastLine1_ && l2 == lastLine2_) return;
    lastLine1_ = l1;
    lastLine2_ = l2;
    lcd_.write(l1, l2);
}
