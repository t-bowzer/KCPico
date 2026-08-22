#include "voicing.h"

#include <algorithm>
#include <cstdlib>
#include <climits>


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

std::vector<uint8_t> clamp127(const std::vector<int>& notes) {
    std::vector<uint8_t> out;
    out.reserve(notes.size());
    for (int n : notes) out.push_back(static_cast<uint8_t>(std::max(0, std::min(127, n))));
    return out;
}

// The tightest ascending voicing of the chord's pitch classes where consecutive
// notes are >= min_interval semitones apart and `bassIdx` is the lowest tone
// (spec 6.4 VR-7). Searches all orderings of the remaining pitch classes and
// keeps the one minimizing the top note. n <= 8, so the permutation space is
// tiny (<= 5040); this runs only on chord trigger, not per-event.
std::vector<int> tightestSpread(const std::vector<int>& pcs, int bassIdx,
                                int bassNote, int minInterval) {
    const int n = static_cast<int>(pcs.size());
    std::vector<int> remaining;
    remaining.reserve(n - 1);
    for (int i = 0; i < n; i++) if (i != bassIdx) remaining.push_back(pcs[i]);
    std::sort(remaining.begin(), remaining.end());

    std::vector<int> best;
    long bestTop = LONG_MAX;

    do {
        int prev = bassNote;
        std::vector<int> cur;
        cur.reserve(n);
        cur.push_back(bassNote);
        for (int pc : remaining) {
            int target = prev + minInterval;
            int rem = ((pc - target) % 12 + 12) % 12;   // smallest >= target with this pc
            int note = target + rem;
            cur.push_back(note);
            prev = note;
        }
        if (static_cast<long>(cur.back()) < bestTop) {
            bestTop = cur.back();
            best = cur;
        }
    } while (std::next_permutation(remaining.begin(), remaining.end()));

    return best;
}

} // namespace


int voicingRoot(int rootPc, uint8_t base_root_midi, int octave) {
    return clampRootOctave(rootMidi(rootPc, base_root_midi, octave));
}

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

std::vector<uint8_t> voiceChord(const ResolvedChord& chord,
                                uint8_t base_root_midi,
                                int octave,
                                uint8_t low,
                                uint8_t high,
                                const VoicingConfig& cfg,
                                const std::vector<uint8_t>& previous) {
    std::vector<int> pcs = chordPitchClasses(chord);
    int n = static_cast<int>(pcs.size());
    if (n == 0) return {};

    int coreCount = chordIntervalCount(chord.type);
    const uint8_t* iv = chordIntervals(chord.type);
    if (coreCount == 0) return {};

    int root = clampRootOctave(rootMidi(chord.rootPc, base_root_midi, octave));

    // Inversion (VR-8) selects which core chord tone sits in the bass by its
    // position in the interval formula (0=root, 1=3rd, 2=5th, 3=7th) — NOT by
    // its position in the sorted pitch-class list. Using the sorted list broke
    // every non-C root (e.g. F major's 1st inversion dropped to the root). A
    // request beyond the chord's own tones wraps into the next octave so, e.g.,
    // the 3rd inversion of a triad (no 7th) stays distinct from the 2nd.
    int inv = static_cast<int>(cfg.inversion);
    int octaveShift = inv / coreCount;
    int coreBassIdx = inv % coreCount;
    int bassInterval = iv[coreBassIdx];
    int bassNote = root + bassInterval + 12 * octaveShift;

    std::vector<int> notes;

    if (cfg.min_interval > 0) {
        // Minimum-interval spread (VR-7): rebuild as the tightest ascending
        // configuration starting at the (possibly inverted) bass.
        int bassPc = ((chord.rootPc + bassInterval) % 12 + 12) % 12;
        int bassPcIdx = 0;
        for (int i = 0; i < n; i++) {
            if (pcs[i] == bassPc) { bassPcIdx = i; break; }
        }
        notes = tightestSpread(pcs, bassPcIdx, bassNote, cfg.min_interval);
    } else {
        std::vector<uint8_t> base = (cfg.voicing_mode == VoicingMode::Smart)
            ? voiceSmart(chord, base_root_midi, octave, low, high, previous)
            : voiceRootPosition(chord, base_root_midi, octave, low, high);

        notes.reserve(base.size());
        for (uint8_t b : base) notes.push_back(static_cast<int>(b));

        // Manual inversion (VR-8): rotate so the chosen tone is the lowest —
        // move the first `coreBassIdx` notes up an octave (root-position notes
        // are in interval order, so this lands the selected core tone in the
        // bass), then apply any octave wrap.
        int rotate = std::min(coreBassIdx, static_cast<int>(notes.size()) - 1);
        for (int i = 0; i < rotate; i++) notes[i] += 12;
        for (int& nt : notes) nt += 12 * octaveShift;
        std::sort(notes.begin(), notes.end());
    }

    // Minimum note count (VR-6): continue the ascending pitch-class cycle
    // (next note = base[i % n] + 12) until the count is met.
    while (static_cast<int>(notes.size()) < static_cast<int>(cfg.min_notes)) {
        size_t i = notes.size();
        notes.push_back(notes[i - n] + 12);
    }

    return clamp127(notes);
}
