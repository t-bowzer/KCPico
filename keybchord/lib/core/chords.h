#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "keymap.h"


enum class ChordType : uint8_t {
    Major = 0,
    Minor,
    Dom7,
    Maj7,
    Min7,
    Dim,
    Dim7,
    Aug,
    Sus4,
    Sus2,
    Min7b5,
    COUNT
};

struct ResolvedChord {
    int       rootPc = 0;               // semitone offset 0..11 (C = 0)
    ChordType type   = ChordType::Major;
    bool      add9   = false;
    bool      add11  = false;
    bool      add13  = false;
};

// Interval formula (semitones from root) for each chord type (spec section 6.2).
const uint8_t* chordIntervals(ChordType type);
int            chordIntervalCount(ChordType type);

// Root order (circle of fifths, left->right), spec section 5.1.
int         rootPcForColumn(int column);       // semitone offset 0..11
const char* rootNameForColumn(int column);     // flat spelling: "Db","Ab",...

// Root MIDI note = base_root_midi + pitch class + 12 * octave (no clamp here).
int rootMidi(int rootPc, uint8_t base_root_midi, int octave);

// Same-column combination resolution (spec section 6.3).
// Returns false if the quality combination does not resolve.
bool resolveSameColumn(bool major, bool minor, bool seventh, ChordType& out);

// Full resolution from held chord-grid cells + backtick.
bool resolveChord(const std::vector<GridCell>& held, bool backtick, ResolvedChord& out);

// Ascending MIDI note set for a resolved chord (base intervals + enabled
// extensions), offset from the given root MIDI note. Integer only.
std::vector<uint8_t> chordNotes(const ResolvedChord& chord, int rootMidiNote);

// Unique ascending pitch classes (0..11) of a resolved chord, including enabled
// extensions. Shared by voicing and strum note-pool derivation.
std::vector<int> chordPitchClasses(const ResolvedChord& chord);

// Flat-spelling chord name for display/log (spec section 6.5).
std::string chordName(const ResolvedChord& chord);

