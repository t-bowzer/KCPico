#include "voicing.h"

#include <algorithm>
#include <cstdlib>


namespace {

// Root octave bounds: C1 (24) as lowest root, C7 (96) as highest usable root
// octave (C8 = 108 would overflow MIDI 127 once extensions are stacked).
constexpr int ROOT_LOW_BASE  = 24;   // C1
constexpr int ROOT_HIGH_BASE = 96;   // C7

// Raw ascending note list (root + intervals + extensions), no clamping.
std::vector<int> rawNotes(const ResolvedChord& chord, int root) {
    std::vector<int> notes;
    int count = chordIntervalCount(chord.type);
    const uint8_t* iv = chordIntervals(chord.type);
    for (int i = 0; i < count; i++) notes.push_back(root + iv[i]);
    if (chord.add9)  notes.push_back(root + 14);
    if (chord.add11) notes.push_back(root + 17);
    if (chord.add13) notes.push_back(root + 21);
    std::sort(notes.begin(), notes.end());
    return notes;
}

// Clamp a note's octave into [C1, C7], preserving pitch class.
int clampRootOctave(int root) {
    int pc = ((root % 12) + 12) % 12;
    int base = root - pc;  // multiple of 12 (octave base)
    base = std::max(ROOT_LOW_BASE, std::min(ROOT_HIGH_BASE, base));
    return base + pc;
}

// Nearest note with the given pitch class to `reference` (ties resolve down).
int nearestPc(int pc, int reference) {
    int p = ((pc % 12) + 12) % 12;
    int rem = ((reference - p) % 12 + 12) % 12;  // 0..11
    if (rem <= 6) return reference - rem;
    return reference + (12 - rem);
}

// Unique sorted pitch classes of the chord.
std::vector<int> chordPitchClasses(const ResolvedChord& c) {
    std::vector<int> tones;
    int count = chordIntervalCount(c.type);
    const uint8_t* iv = chordIntervals(c.type);
    for (int i = 0; i < count; i++) tones.push_back(((c.rootPc + iv[i]) % 12 + 12) % 12);
    if (c.add9)  tones.push_back(((c.rootPc + 14) % 12 + 12) % 12);
    if (c.add11) tones.push_back(((c.rootPc + 17) % 12 + 12) % 12);
    if (c.add13) tones.push_back(((c.rootPc + 21) % 12 + 12) % 12);
    std::sort(tones.begin(), tones.end());
    tones.erase(std::unique(tones.begin(), tones.end()), tones.end());
    return tones;
}

std::vector<uint8_t> clamp127(const std::vector<int>& notes) {
    std::vector<uint8_t> out;
    out.reserve(notes.size());
    for (int n : notes) out.push_back(static_cast<uint8_t>(std::max(0, std::min(127, n))));
    return out;
}

} // namespace


std::vector<uint8_t> voiceRootPosition(const ResolvedChord& chord,
                                       uint8_t base_root_midi,
                                       int octave,
                                       uint8_t low,
                                       uint8_t high) {
    (void)low;
    (void)high;

    // Full keyboard root range (C1..C7); octave transposes by whole octaves and
    // clamps at the ends rather than collapsing into a narrow window.
    int root = clampRootOctave(rootMidi(chord.rootPc, base_root_midi, octave));
    std::vector<int> notes = rawNotes(chord, root);
    return clamp127(notes);
}

std::vector<uint8_t> voiceSmart(const ResolvedChord& chord,
                                uint8_t base_root_midi,
                                int octave,
                                uint8_t low,
                                uint8_t high,
                                const std::vector<uint8_t>& previous) {
    std::vector<int> tones = chordPitchClasses(chord);
    int n = static_cast<int>(tones.size());

    if (previous.empty() || static_cast<int>(previous.size()) != n) {
        return voiceRootPosition(chord, base_root_midi, octave, low, high);
    }

    // Center the voicing around the note range midpoint (shifted by octave),
    // so successive chords hover near a stable center instead of drifting down.
    int center = clampRootOctave(static_cast<int>(low + high) / 2 + 12 * octave);

    std::vector<int> bestNotes;
    long bestScore = -1;

    for (int r = 0; r < n; r++) {
        std::vector<int> notes(n);
        notes[0] = nearestPc(tones[r], center);
        for (int j = 1; j < n; j++) {
            int tone = tones[(r + j) % n];
            int base = notes[0] + (((tone - tones[r]) % 12 + 12) % 12);
            while (base <= notes[j - 1]) base += 12;
            notes[j] = base;
        }

        long score = 0;
        for (int j = 0; j < n; j++) {
            score += std::abs(static_cast<int>(notes[j]) - static_cast<int>(previous[j]));
        }

        if (bestScore < 0 || score < bestScore) {
            bestScore = score;
            bestNotes = notes;
        }
    }

    return clamp127(bestNotes);
}

