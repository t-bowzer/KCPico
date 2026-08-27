#pragma once

#include <cstdint>
#include <string>

#include "params.h"
#include "state.h"


// Menu definition: title (LCD line 1) and the F-key -> parameter list.
const char* menuTitle(EditMenu menu);
int         menuParamCount(EditMenu menu);
ParamId     menuParamAt(EditMenu menu, int index);

// Display names. Short = menu select line ("Octave"); full = value line
// ("Chord Octave", "Rhythm Mute").
const char* paramShortName(ParamId id);
const char* paramFullName(ParamId id);

// Human-readable value of a parameter for the LCD (signed ints, enum names,
// On/Off, Full/Limited).
std::string paramValueString(const StateManager& state, ParamId id);

// +/- semantics: ints add `delta` and clamp; enums cycle by sign; bools are set
// on (+1) / off (-1). Menu +/- uses this.
void paramStep(StateManager& state, ParamId id, int delta);

// Single-key toggle/cycle semantics: bools flip; enums advance one step.
// Direct main-menu shortcuts (F1/F2/F3/F4/F5/F6/F7/F8) use this.
void paramCycle(StateManager& state, ParamId id);

// Set a parameter to an absolute value (an enum's int value, or an int/bool).
// Used by the "set" keymap action (e.g. PrtSc -> inversion First).
void paramSet(StateManager& state, ParamId id, int value);

// Toggle a parameter between two predetermined values: if it currently equals
// `valueA` it becomes `valueB`, otherwise it becomes `valueA`. Used by the
// "toggle" keymap action (e.g. chord roll 0 <-> 50).
void paramToggle(StateManager& state, ParamId id, int valueA, int valueB);

// True for parameters with a large value range that auto-repeat when the
// +/-/arrow step key is held (tempo, velocity, pan, durations, swing). Small
// ranges (octave, enums, toggles, pattern) step once per press only.
bool isAutoRepeatable(ParamId id);
