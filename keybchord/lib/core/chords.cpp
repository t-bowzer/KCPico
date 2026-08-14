#include "chords.h"

#include <algorithm>


namespace {

// Extension tension offsets (spec section 6.2).
constexpr int EXT_ADD9  = 14;
constexpr int EXT_ADD11 = 17;
constexpr int EXT_ADD13 = 21;

constexpr uint8_t kIntervals[][7] = {
    {0, 4, 7},              // Major
    {0, 3, 7},              // Minor
    {0, 4, 7, 10},          // Dom7
    {0, 4, 7, 11},          // Maj7
    {0, 3, 7, 10},          // Min7
    {0, 3, 6},              // Dim
    {0, 3, 6, 9},           // Dim7
    {0, 4, 8},              // Aug
    {0, 5, 7},              // Sus4
    {0, 2, 7},              // Sus2
    {0, 3, 6, 10},          // Min7b5
};

constexpr uint8_t kIntervalCounts[] = {
    3, 3, 4, 4, 4, 3, 4, 3, 3, 3, 4,
};

// Circle-of-fifths root order (spec section 5.1): semitone offsets from C.
constexpr int kRootPc[12] = {1, 8, 3, 10, 5, 0, 7, 2, 9, 4, 11, 6};

const char* const kRootName[12] = {
    "Db", "Ab", "Eb", "Bb", "F", "C", "G", "D", "A", "E", "B", "F#",
};

// Flat spelling of a pitch class (spec section 6.5, flats-by-default).
const char* rootNameForPc(int pc) {
    switch (pc) {
        case 0:  return "C";
        case 1:  return "Db";
        case 2:  return "D";
        case 3:  return "Eb";
        case 4:  return "E";
        case 5:  return "F";
        case 6:  return "F#";
        case 7:  return "G";
        case 8:  return "Ab";
        case 9:  return "A";
        case 10: return "Bb";
        case 11: return "B";
        default: return "C";
    }
}

} // namespace


const uint8_t* chordIntervals(ChordType type) {
    return kIntervals[static_cast<int>(type)];
}

int chordIntervalCount(ChordType type) {
    return kIntervalCounts[static_cast<int>(type)];
}

int rootPcForColumn(int column) {
    if (column < 0 || column >= 12) return 0;
    return kRootPc[column];
}

const char* rootNameForColumn(int column) {
    if (column < 0 || column >= 12) return "C";
    return kRootName[column];
}

int rootMidi(int rootPc, uint8_t base_root_midi, int octave) {
    return static_cast<int>(base_root_midi) + rootPc + 12 * octave;
}

bool resolveSameColumn(bool major, bool minor, bool seventh, ChordType& out) {
    if (major && minor && seventh) { out = ChordType::Aug;   return true; }
    if (major && seventh)          { out = ChordType::Maj7;  return true; }
    if (minor && seventh)          { out = ChordType::Min7;  return true; }
    if (major && minor)            { out = ChordType::Dim;   return true; }
    if (seventh)                   { out = ChordType::Dom7;  return true; }
    if (minor)                     { out = ChordType::Minor; return true; }
    if (major)                     { out = ChordType::Major; return true; }
    return false;
}

bool resolveChord(const std::vector<GridCell>& held, bool backtick, ResolvedChord& out) {
    if (held.empty()) return false;

    // Determine which columns are held and which qualities exist per column.
    int  columnCount[12] = {0};
    bool hasMajor[12]    = {false};
    bool hasMinor[12]    = {false};
    bool hasSeventh[12]  = {false};

    int firstColumn = -1;
    int columnSpan  = 0;

    for (const auto& c : held) {
        int col = c.column;
        if (col < 0 || col >= 12) continue;
        if (columnCount[col] == 0) {
            if (firstColumn < 0) firstColumn = col;
            columnSpan++;
        }
        columnCount[col]++;
        switch (c.quality) {
            case ChordQuality::Major:   hasMajor[col]   = true; break;
            case ChordQuality::Minor:   hasMinor[col]   = true; break;
            case ChordQuality::Seventh: hasSeventh[col] = true; break;
            default: break;
        }
    }

    // Leftmost-column backtick special case (spec section 6.3).
    if (backtick && columnSpan == 1 && firstColumn == 0) {
        if (hasMajor[0]) {
            out.rootPc = rootPcForColumn(0);
            out.type   = ChordType::Sus4;
            return true;
        }
        if (hasMinor[0]) {
            out.rootPc = rootPcForColumn(0);
            out.type   = ChordType::Major;
            out.add9   = true;
            return true;
        }
    }

    if (columnSpan == 1) {
        ChordType t;
        if (!resolveSameColumn(hasMajor[firstColumn], hasMinor[firstColumn],
                               hasSeventh[firstColumn], t)) {
            return false;
        }
        out.rootPc = rootPcForColumn(firstColumn);
        out.type   = t;
        return true;
    }

    // Left-adjacent combinations: a Major key plus a key one column to its left.
    for (int col = 1; col < 12; col++) {
        if (hasMajor[col] && hasSeventh[col - 1]) {
            out.rootPc = rootPcForColumn(col);
            out.type   = ChordType::Sus4;
            return true;
        }
    }
    for (int col = 1; col < 12; col++) {
        if (hasMajor[col] && hasMinor[col - 1]) {
            out.rootPc = rootPcForColumn(col);
            out.type   = ChordType::Major;
            out.add9   = true;
            return true;
        }
    }

    return false;
}

std::vector<uint8_t> chordNotes(const ResolvedChord& chord, int rootMidiNote) {
    std::vector<int> notes;
    notes.reserve(8);

    int count = chordIntervalCount(chord.type);
    const uint8_t* iv = chordIntervals(chord.type);
    for (int i = 0; i < count; i++) {
        notes.push_back(rootMidiNote + iv[i]);
    }

    if (chord.add9)  notes.push_back(rootMidiNote + EXT_ADD9);
    if (chord.add11) notes.push_back(rootMidiNote + EXT_ADD11);
    if (chord.add13) notes.push_back(rootMidiNote + EXT_ADD13);

    std::sort(notes.begin(), notes.end());

    std::vector<uint8_t> out;
    out.reserve(notes.size());
    for (int n : notes) {
        int clamped = std::max(0, std::min(127, n));
        out.push_back(static_cast<uint8_t>(clamped));
    }
    return out;
}

std::vector<int> chordPitchClasses(const ResolvedChord& chord) {
    std::vector<int> tones;
    int count = chordIntervalCount(chord.type);
    const uint8_t* iv = chordIntervals(chord.type);
    for (int i = 0; i < count; i++) {
        tones.push_back(((chord.rootPc + iv[i]) % 12 + 12) % 12);
    }
    if (chord.add9)  tones.push_back(((chord.rootPc + EXT_ADD9)  % 12 + 12) % 12);
    if (chord.add11) tones.push_back(((chord.rootPc + EXT_ADD11) % 12 + 12) % 12);
    if (chord.add13) tones.push_back(((chord.rootPc + EXT_ADD13) % 12 + 12) % 12);
    std::sort(tones.begin(), tones.end());
    tones.erase(std::unique(tones.begin(), tones.end()), tones.end());
    return tones;
}

std::string chordName(const ResolvedChord& chord) {
    std::string root = rootNameForPc(chord.rootPc);

    std::string quality;
    switch (chord.type) {
        case ChordType::Major:  quality = "";        break;
        case ChordType::Minor:  quality = "m";       break;
        case ChordType::Dom7:   quality = "7";       break;
        case ChordType::Maj7:   quality = "maj7";    break;
        case ChordType::Min7:   quality = "m7";      break;
        case ChordType::Dim:    quality = "dim";     break;
        case ChordType::Dim7:   quality = "dim7";    break;
        case ChordType::Aug:    quality = "aug";     break;
        case ChordType::Sus4:   quality = "sus4";    break;
        case ChordType::Sus2:   quality = "sus2";    break;
        case ChordType::Min7b5: quality = "m7b5";    break;
        default:                quality = "";        break;
    }

    std::string ext;
    if (chord.add9)  ext += "add9";
    if (chord.add11) ext += "add11";
    if (chord.add13) ext += "add13";

    return root + quality + ext;
}

