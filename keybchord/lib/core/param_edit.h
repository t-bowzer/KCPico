#pragma once

#include <cstdint>
#include <string>

#include "params.h"
#include "state.h"


// Central registry of editable parameters (spec section 9). The edit engine and
// display manager use this single source of truth so new parameters can be added
// without reworking key mappings — just add a ParamId and a menu-table entry.
enum class ParamId : uint8_t {
    // Chord
    ChordOctave = 0,
    ChordMode,
    ChordVoicing,
    ChordDuration,
    ChordVelocity,
    ChordPan,
    ChordAdd9,
    ChordAdd11,
    ChordAdd13,
    // Strum
    StrumOctave,
    StrumDuration,
    StrumVelocity,
    StrumLayout,
    // Rhythm
    RhythmTempo,
    RhythmSwing,
    RhythmPattern,
    RhythmMute,
    RhythmEnable,
    RhythmClock,
    RhythmLed,
    COUNT
};

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
// Direct main-menu shortcuts (F1/F5/arrows/F7/F8/F9) use this.
void paramCycle(StateManager& state, ParamId id);
