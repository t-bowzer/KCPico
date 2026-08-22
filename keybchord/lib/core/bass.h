#pragma once

#include <cstdint>

#include "chords.h"
#include "params.h"


// Walking-bass interval blueprint (spec section 6.8). Returns the semitone
// offset above the chord root for the bass note on `beat` (0-based within the
// bar). The cycle length is the pattern's meter: a 4/4 bar walks
// root-3rd-5th-6th/7th; a 3/4 waltz uses only the first three offsets.
int bassOffsetForBeat(ChordType type, int beat, int beatsPerBar);

// Semitone offset above the root for a configurable bass pattern on `beat`
// (0-based), or -1 when the pattern rests on that beat. The `Walking` pattern
// delegates to bassOffsetForBeat; the others are root/5th patterns (Upgrade-Plan).
int bassOffsetForPattern(BassPattern pattern, ChordType type, int beat, int beatsPerBar);

// Number of beats a note sustains on `beat` for sustained patterns (Whole = the
// whole bar, Half/HalfAlt = 2 beats, WalkNoSixth = the root sustains a half note
// on beat 0). Returns 0 for percussive notes, meaning the note length is the
// configured `note_duration_ms`.
int bassSustainBeats(BassPattern pattern, int beat, int beatsPerBar);

// Absolute MIDI note of the walking bass on a given beat:
//   rootMidi(root_pc, base_root_midi, bass_octave) + bassOffsetForBeat(...)
int bassNote(ChordType type, int rootPc, uint8_t base_root_midi,
             int bassOctave, int beat, int beatsPerBar);
