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

    // Show a menu: title on line 1, highlighted parameter name on line 2.
    // `selected` adds the "> " marker for the parameter being value-edited.
    void showMenu(const std::string& title, const std::string& paramName,
                  bool selected = false);

    // Update the highlighted-parameter line while staying in the menu.
    void selectParam(const std::string& paramName, bool selected = false);

    // Transient value screen (line1 = full name, line2 = value). If inMenu,
    // reverts to the menu screen after display_revert_ms; otherwise to idle.
    void showValue(const std::string& fullName, const std::string& value,
                   bool inMenu, uint64_t now_us);

    // Prompt (FR-D3), auto-cancelling after display_prompt_ms (FR-P10).
    // The single-line form keeps the standard "Enter=Yes Bk=No" hint.
    void showPrompt(const std::string& text, uint64_t now_us);
    void showPrompt(const std::string& line1, const std::string& line2,
                    uint64_t now_us);

    // Persistent status screen (e.g. an in-progress sequence recording). It
    // stays until another screen replaces it or cancel() is called. It does not
    // clobber prompts, name editing, errors, or info screens.
    void showPersistent(const std::string& line1, const std::string& line2);

    // Persistent name-edit screen (preset naming). Shows "Rename" + the current
    // buffer (with a trailing cursor); cleared by cancel().
    void showNameEdit(const std::string& text);

    // Persistent error screen: stays until cancel() (used for boot-time config
    // errors, e.g. an invalid keymap.json).
    void showError(const std::string& line1, const std::string& line2);

    // Persistent informational screen (firmware version/debug). Stays until
    // cancel() is called, e.g. when the next key is pressed.
    void showInfo(const std::string& line1, const std::string& line2);
    bool infoVisible() const { return screen_ == Screen::Info; }

    // Return to idle (exiting a menu or cancelling a transient).
    void cancel();

private:
    enum class Screen : uint8_t {
        Idle, Menu, Edit, Prompt, NameEdit, Error, Info, Status,
    };

    StateManager& state_;
    LcdAdapter&   lcd_;

    Screen   screen_           = Screen::Idle;
    bool     revertToMenu_     = false;
    uint64_t revertDeadlineUs_ = 0;

    std::string menuTitle_;
    std::string menuParamName_;
    bool        menuParamSelected_ = false;
    std::string editFullName_;
    std::string editValue_;
    std::string promptLine1_;
    std::string promptLine2_;
    std::string nameEditText_;
    std::string errorLine1_;
    std::string errorLine2_;
    std::string infoLine1_;
    std::string infoLine2_;
    std::string statusLine1_;
    std::string statusLine2_;

    std::string lastLine1_;
    std::string lastLine2_;

    void renderIdle(std::string& l1, std::string& l2, uint64_t now_us) const;
    void render(std::string& l1, std::string& l2, uint64_t now_us) const;
    void emit(const std::string& l1, const std::string& l2);
};
