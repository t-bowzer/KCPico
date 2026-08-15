#pragma once

#include <cstdint>
#include <string>

#include "base.h"
#include "state.h"


// Renders the LCD1602 (FR-D1..FR-D3). Event-driven: the EditEngine calls
// showMenu/selectParam/showValue/showPrompt; this class just renders the
// current screen and owns the revert/auto-cancel timers. Idle renders from the
// live StateManager.
class DisplayManager {
public:
    DisplayManager(StateManager& state, LcdAdapter& lcd);

    // Revert expired screens and render the current frame.
    void update(uint64_t now_us);

    // Show a menu: title on line 1, selected parameter name on line 2.
    void showMenu(const std::string& title, const std::string& paramName);

    // Update the selected-parameter line while staying in the menu.
    void selectParam(const std::string& paramName);

    // Transient value screen (line1 = full name, line2 = value). If inMenu,
    // reverts to the menu screen after display_revert_ms; otherwise to idle.
    void showValue(const std::string& fullName, const std::string& value,
                   bool inMenu, uint64_t now_us);

    // Prompt (FR-D3), auto-cancelling after display_prompt_ms (FR-P10).
    void showPrompt(const std::string& text, uint64_t now_us);

    // Return to idle (exiting a menu or cancelling a transient).
    void cancel();

private:
    enum class Screen : uint8_t { Idle, Menu, Edit, Prompt };

    StateManager& state_;
    LcdAdapter&   lcd_;

    Screen   screen_           = Screen::Idle;
    bool     revertToMenu_     = false;
    uint64_t revertDeadlineUs_ = 0;

    std::string menuTitle_;
    std::string menuParamName_;
    std::string editFullName_;
    std::string editValue_;
    std::string promptText_;

    std::string lastLine1_;
    std::string lastLine2_;

    void renderIdle(std::string& l1, std::string& l2) const;
    void render(std::string& l1, std::string& l2) const;
    void emit(const std::string& l1, const std::string& l2);
};
