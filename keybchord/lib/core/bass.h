#pragma once

#include <cstdint>

#include "chords.h"


// Walking-bass interval blueprint (spec section 6.8). Returns the semitone
// offset above the chord root for the bass note on `beat` (0-based within the
// bar). The cycle length is the pattern's meter: a 4/4 bar walks
// root-3rd-5th-6th/7th; a 3/4 waltz uses only the first three offsets.
int bassOffsetForBeat(ChordType type, int beat, int beatsPerBar);

// Absolute MIDI note of the walking bass on a given beat:
//   rootMidi(root_pc, base_root_midi, bass_octave) + bassOffsetForBeat(...)
int bassNote(ChordType type, int rootPc, uint8_t base_root_midi,
             int bassOctave, int beat, int beatsPerBar);
