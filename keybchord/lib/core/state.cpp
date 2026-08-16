#include "state.h"
#include <algorithm>


StateManager::StateManager() {
    loadDefaults();
}

void StateManager::loadDefaults() {
    pendingChord  = ChordParams::defaults();
    pendingStrum  = StrumParams::defaults();
    pendingRhythm = RhythmParams::defaults();
    snapshotActive();
    config = AppConfig::defaults();
    activeNotes.clear();
    currentBank = 0;
    currentSlot = 0;
    dirty = false;
    cursorActive = false;
    cursorBank = 0;
    cursorSlot = 0;
    selectedChord = ResolvedChord{};
    selectedChordValid = false;
    editMenu = EditMenu::None;
    editParam = 0;
    rhythmClock = RhythmClock{};
    ledIndicator = LedIndicator{};
}

void StateManager::snapshotActive() {
    activeChord  = pendingChord;
    activeStrum  = pendingStrum;
    activeRhythm = pendingRhythm;
}

void StateManager::snapshotChord() {
    activeChord = pendingChord;
}

void StateManager::noteOn(uint8_t channel, uint8_t note) {
    removeNote(channel, note);
    activeNotes.push_back({note, channel});
}

void StateManager::noteOff(uint8_t channel, uint8_t note) {
    removeNote(channel, note);
}

void StateManager::allNotesOff() {
    activeNotes.clear();
}

bool StateManager::isNoteActive(uint8_t channel, uint8_t note) const {
    return std::any_of(activeNotes.begin(), activeNotes.end(),
        [channel, note](const ActiveNote& an) {
            return an.channel == channel && an.note == note;
        });
}

void StateManager::removeNote(uint8_t channel, uint8_t note) {
    activeNotes.erase(
        std::remove_if(activeNotes.begin(), activeNotes.end(),
            [channel, note](const ActiveNote& an) {
                return an.channel == channel && an.note == note;
            }),
        activeNotes.end());
}


