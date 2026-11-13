#include "pattern_editor_engine.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <vector>

#include "bass_engine.h"
#include "chord_engine.h"
#include "defaults.h"
#include "display_manager.h"
#include "keymap.h"
#include "midi_router.h"
#include "pattern_limits.h"
#include "rhythm_engine.h"

namespace {

constexpr uint16_t HID_ENTER     = 0x28;
constexpr uint16_t HID_ESC       = 0x29;
constexpr uint16_t HID_BACKSPACE = 0x2A;
constexpr uint16_t HID_LEFT      = 0x50;
constexpr uint16_t HID_RIGHT     = 0x4F;
constexpr uint16_t HID_UP        = 0x52;
constexpr uint16_t HID_DOWN      = 0x51;

constexpr uint16_t HID_KP_SLASH = 0x54;
constexpr uint16_t HID_KP_STAR  = 0x55;
constexpr uint16_t HID_KP1      = 0x59;
constexpr uint16_t HID_KP2      = 0x5A;
constexpr uint16_t HID_KP3      = 0x5B;
constexpr uint16_t HID_KP4      = 0x5C;
constexpr uint16_t HID_KP5      = 0x5D;
constexpr uint16_t HID_KP6      = 0x5E;
constexpr uint16_t HID_KP7      = 0x5F;
constexpr uint16_t HID_KP8      = 0x60;
constexpr uint16_t HID_KP9      = 0x61;
constexpr uint16_t HID_KP0      = 0x62;
constexpr uint16_t HID_KP_DOT   = 0x63;

constexpr int MAX_NAME_LEN = 16;

struct RhythmPiece {
    uint8_t note;
    const char* name;
    const char* short_name;
};

constexpr RhythmPiece kPieces[13] = {
    {36, "kick", "Kick"},       {38, "snare", "Snare"},
    {42, "hihat", "HiHat"},     {46, "open_hat", "OpenHat"},
    {37, "rimshot", "Rimshot"}, {39, "clap", "Clap"},
    {49, "crash", "Crash"},     {51, "ride", "Ride"},
    {61, "bongo", "Bongo"},     {62, "conga_lo", "CongaLo"},
    {63, "conga_hi", "CongaHi"}, {75, "clave", "Clave"},
    {82, "shaker", "Shaker"},
};

constexpr uint8_t QUANTIZE_CHOICES[] = {4, 8, 16};
constexpr uint8_t BAR_CHOICES[] = {1, 2, 4, 8};

std::string padTo(const std::string& s, size_t n) {
    if (s.size() >= n) return s.substr(0, n);
    return s + std::string(n - s.size(), ' ');
}

const char* hexDigit(uint8_t v) {
    static const char* digits = "0123456789ABCDEF";
    return (v < 16) ? (digits + v) : "0";
}

EditMenu normalizedEditorKind(EditMenu kind) {
    switch (kind) {
        case EditMenu::ChordPattern:  return EditMenu::Chord;
        case EditMenu::RhythmPattern: return EditMenu::Rhythm;
        case EditMenu::BassPattern:   return EditMenu::Bass;
        default:                      return kind;
    }
}

int userFileNumber(const std::string& file) {
    if (file.compare(0, 4, "user") != 0) return 0;
    int n = 0;
    size_t i = 4;
    while (i < file.size() && file[i] >= '0' && file[i] <= '9') {
        n = n * 10 + (file[i] - '0');
        i++;
    }
    if (i == 4 || file.compare(i, 5, ".json") != 0) return 0;
    return n;
}

} // namespace

PatternEditorEngine::PatternEditorEngine(
        StateManager& state, StorageAdapter& storage, DisplayManager& display,
        MidiRouter& router, RhythmEngine& rhythm, BassEngine& bass,
        ChordEngine& chord)
    : state_(state), storage_(storage), display_(display), router_(router),
      rhythm_(rhythm), bass_(bass), chord_(chord) {}

void PatternEditorEngine::setModifiedCallback(std::function<void()> cb) {
    modified_ = std::move(cb);
}

void PatternEditorEngine::setWriteAllowedCallback(std::function<bool()> cb) {
    writeAllowed_ = std::move(cb);
}

// --- pattern bookkeeping ---------------------------------------------------

int PatternEditorEngine::loopSteps() const {
    return static_cast<int>(bars_) * static_cast<int>(stepsPerBar_);
}

uint8_t PatternEditorEngine::liveStepsPerBar() const {
    int idx = state_.pendingRhythm.pattern;
    if (idx >= 0 && idx < static_cast<int>(rhythmPatterns_.size())) {
        int spb = rhythmPatterns_[idx].steps_per_bar;
        if (spb == 12 || spb == 16) return static_cast<uint8_t>(spb);
    }
    int spb = state_.rhythmClock.stepsPerBar.load();
    return (spb == 12) ? 12 : 16;
}

bool PatternEditorEngine::meterMatchesLive() const {
    return stepsPerBar_ == liveStepsPerBar();
}

int PatternEditorEngine::patternCount(EditMenu kind) const {
    switch (kind) {
        case EditMenu::Rhythm: return static_cast<int>(rhythmPatterns_.size());
        case EditMenu::Bass:   return static_cast<int>(bassPatterns_.size());
        case EditMenu::Chord:  return static_cast<int>(chordPatterns_.size());
        default: return 0;
    }
}

const char* PatternEditorEngine::patternName(EditMenu kind, int index) const {
    switch (kind) {
        case EditMenu::Rhythm: return rhythmName(index);
        case EditMenu::Bass:   return bassName(index);
        case EditMenu::Chord:  return chordPatternName(index);
        default: return "";
    }
}

const char* PatternEditorEngine::patternDir(EditMenu kind) const {
    switch (kind) {
        case EditMenu::Rhythm: return "/rhythms";
        case EditMenu::Bass:   return "/bass";
        case EditMenu::Chord:  return "/chordpatterns";
        default: return "";
    }
}

int PatternEditorEngine::builtinCount(EditMenu kind) const {
    switch (kind) {
        case EditMenu::Rhythm: return RHYTHM_COUNT;
        case EditMenu::Bass:   return BASS_COUNT;
        case EditMenu::Chord:  return CHORD_PATTERN_COUNT;
        default: return 0;
    }
}

std::string PatternEditorEngine::builtinFileName(EditMenu kind, int index) const {
    switch (kind) {
        case EditMenu::Rhythm: return rhythmFileName(index);
        case EditMenu::Bass:   return bassFileName(index);
        case EditMenu::Chord:  return chordPatternFileName(index);
        default: return "";
    }
}

bool PatternEditorEngine::isBuiltinFile(EditMenu kind,
                                        const std::string& file) const {
    for (int i = 0; i < builtinCount(kind); i++) {
        if (file == builtinFileName(kind, i)) return true;
    }
    return false;
}

std::string PatternEditorEngine::fileForIndex(EditMenu kind, int index) const {
    if (index < 0) return "";
    if (index < builtinCount(kind)) return builtinFileName(kind, index);

    std::vector<std::string> files = storage_.listFiles(patternDir(kind));
    std::vector<std::string> user;
    for (const auto& f : files) {
        if (f.size() < 5 || f.compare(f.size() - 5, 5, ".json") != 0) continue;
        if (isBuiltinFile(kind, f)) continue;
        // Match the loader: skip files it would not add to the library.
        std::string raw = storage_.readFile(std::string(patternDir(kind)) +
                                            "/" + f);
        bool valid = false;
        switch (kind) {
            case EditMenu::Rhythm: {
                RhythmPattern p;
                valid = parseRhythmPattern(raw, p) && !p.tracks.empty();
                break;
            }
            case EditMenu::Bass: {
                BassPattern p;
                valid = parseBassPattern(raw, p) && p.valid();
                break;
            }
            case EditMenu::Chord: {
                ChordPattern p;
                valid = parseChordPattern(raw, p) && p.valid();
                break;
            }
            default:
                break;
        }
        if (!valid) continue;
        user.push_back(f);
    }
    std::sort(user.begin(), user.end());
    int ui = index - builtinCount(kind);
    if (ui < 0 || ui >= static_cast<int>(user.size())) return "";
    return user[ui];
}

std::string PatternEditorEngine::uniqueUserFileName(EditMenu kind) const {
    std::set<std::string> used;
    for (const auto& f : storage_.listFiles(patternDir(kind))) used.insert(f);
    for (int i = 1; i < 10000; i++) {
        std::string name = "user" + std::to_string(i) + ".json";
        if (used.count(name) == 0 && !isBuiltinFile(kind, name)) return name;
    }
    return "user9999.json";
}

std::string PatternEditorEngine::uniquePatternName(
        EditMenu kind, const std::string& base,
        const std::string& current) const {
    auto used = [&](const std::string& name) {
        for (int i = 0; i < patternCount(kind); i++) {
            const char* n = patternName(kind, i);
            if (n && name == n && name != current) return true;
        }
        return false;
    };
    if (!used(base)) return base;
    for (int i = 2; i < 1000; i++) {
        std::string n = base + " " + std::to_string(i);
        if (!used(n)) return n;
    }
    return base;
}

std::string PatternEditorEngine::workingName() const {
    switch (kind_) {
        case EditMenu::Rhythm: return rhythmWork_.name;
        case EditMenu::Bass:   return bassWork_.name;
        case EditMenu::Chord:  return chordWork_.name;
        default: return "";
    }
}

void PatternEditorEngine::setWorkingName(const std::string& name) {
    switch (kind_) {
        case EditMenu::Rhythm: rhythmWork_.name = name; break;
        case EditMenu::Bass:   bassWork_.name = name; break;
        case EditMenu::Chord:  chordWork_.name = name; break;
        default: break;
    }
}

void PatternEditorEngine::loadLibraries() {
    RhythmLibrary r = loadRhythmPatterns(storage_);
    installRhythmNames(r.names);
    installRhythmShortNames(r.short_names);
    rhythmPatterns_ = std::move(r.patterns);

    BassLibrary b = loadBassPatterns(storage_);
    installBassNames(b.names);
    bassPatterns_ = std::move(b.patterns);

    ChordPatternLibrary c = loadChordPatterns(storage_);
    installChordPatternNames(c.names);
    chordPatterns_ = std::move(c.patterns);
}

void PatternEditorEngine::loadPattern(EditMenu kind, int index) {
    kind_ = kind;
    patternIndex_ = index;
    builtin_ = index < builtinCount(kind);
    fileName_ = fileForIndex(kind, index);
    dirty_ = false;
    newUnsaved_ = false;
    cursorBar_ = 0;
    cursorStep_ = 0;
    cursorPiece_ = 0;

    switch (kind) {
        case EditMenu::Rhythm:
            if (index >= 0 && index < static_cast<int>(rhythmPatterns_.size())) {
                rhythmWork_ = rhythmPatterns_[index];
            } else {
                rhythmWork_ = RhythmPattern{};
                rhythmWork_.name = "New Rhythm";
                rhythmWork_.steps_per_bar = liveStepsPerBar();
                rhythmWork_.tracks = {{36, "kick", {}}};
            }
            break;
        case EditMenu::Bass:
            if (index >= 0 && index < static_cast<int>(bassPatterns_.size())) {
                bassWork_ = bassPatterns_[index];
            } else {
                bassWork_ = BassPattern{};
                bassWork_.name = "New Bass";
                bassWork_.steps_per_bar = liveStepsPerBar();
            }
            break;
        case EditMenu::Chord:
            if (index >= 0 && index < static_cast<int>(chordPatterns_.size())) {
                chordWork_ = chordPatterns_[index];
            } else {
                chordWork_ = ChordPattern{};
                chordWork_.name = "New Chord";
                chordWork_.steps_per_bar = liveStepsPerBar();
            }
            break;
        default:
            break;
    }
    normalizeWorking();
    screen_ = Screen::Menu;
    renderMenu();
}

void PatternEditorEngine::loadCurrentPresetPattern() {
    int idx = 0;
    switch (kind_) {
        case EditMenu::Rhythm: idx = state_.pendingRhythm.pattern; break;
        case EditMenu::Bass:   idx = state_.pendingBass.pattern; break;
        case EditMenu::Chord:  idx = state_.pendingChord.pattern; break;
        default: break;
    }
    int count = patternCount(kind_);
    if (count <= 0) idx = 0;
    else if (idx >= count) idx = 0;
    if (idx < 0) idx = 0;
    loadPattern(kind_, idx);
}

void PatternEditorEngine::normalizeWorking() {
    switch (kind_) {
        case EditMenu::Rhythm: {
            stepsPerBar_ = static_cast<uint8_t>(
                std::min(64, std::max(4, static_cast<int>(rhythmWork_.steps_per_bar))));
            if (rhythmWork_.tracks.empty()) {
                rhythmWork_.tracks.push_back({36, "kick", {}});
            }
            size_t maxLen = 0;
            for (const auto& t : rhythmWork_.tracks) {
                maxLen = std::max(maxLen, t.pattern.size());
            }
            int b = static_cast<int>(maxLen / stepsPerBar_);
            if (b < 1) b = 1;
            if (b > 8) b = 8;
            bars_ = static_cast<uint8_t>(b);
            rhythmWork_.steps_per_bar = stepsPerBar_;
            resizeWorking();
            break;
        }
        case EditMenu::Bass: {
            stepsPerBar_ = static_cast<uint8_t>(
                std::min(64, std::max(4, static_cast<int>(bassWork_.steps_per_bar))));
            int b = static_cast<int>(bassWork_.steps.size() / stepsPerBar_);
            if (b < 1) b = 1;
            if (b > 8) b = 8;
            bars_ = static_cast<uint8_t>(b);
            bassWork_.steps_per_bar = stepsPerBar_;
            if (bassWork_.steps.empty()) {
                bassWork_.steps.assign(loopSteps(), BASS_DEGREE_REST);
            }
            resizeWorking();
            break;
        }
        case EditMenu::Chord: {
            stepsPerBar_ = static_cast<uint8_t>(
                std::min(64, std::max(4, static_cast<int>(chordWork_.steps_per_bar))));
            int b = static_cast<int>(chordWork_.steps.size() / stepsPerBar_);
            if (b < 1) b = 1;
            if (b > 8) b = 8;
            bars_ = static_cast<uint8_t>(b);
            chordWork_.steps_per_bar = stepsPerBar_;
            if (chordWork_.steps.empty()) {
                chordWork_.steps.assign(loopSteps(), 0);
            }
            resizeWorking();
            break;
        }
        default:
            break;
    }
}

void PatternEditorEngine::resizeWorking() {
    const int n = loopSteps();
    switch (kind_) {
        case EditMenu::Rhythm:
            for (auto& t : rhythmWork_.tracks) {
                t.pattern.resize(static_cast<size_t>(n), 0);
            }
            if (rhythmWork_.tracks.empty()) {
                rhythmWork_.tracks.push_back({36, "kick", std::vector<uint8_t>(n, 0)});
            }
            break;
        case EditMenu::Bass:
            bassWork_.steps.resize(static_cast<size_t>(n), BASS_DEGREE_REST);
            bassWork_.sustain_steps.resize(static_cast<size_t>(n), 0);
            break;
        case EditMenu::Chord:
            chordWork_.steps.resize(static_cast<size_t>(n), 0);
            chordWork_.sustain_steps.resize(static_cast<size_t>(n), 0);
            break;
        default:
            break;
    }
    if (cursorBar_ >= bars_) cursorBar_ = bars_ - 1;
    if (cursorStep_ >= stepsPerBar_) cursorStep_ = stepsPerBar_ - 1;
}

void PatternEditorEngine::clearWorking() {
    const int n = loopSteps();
    switch (kind_) {
        case EditMenu::Rhythm:
            for (auto& t : rhythmWork_.tracks) {
                t.pattern.assign(static_cast<size_t>(n), 0);
            }
            if (rhythmWork_.tracks.empty()) {
                rhythmWork_.tracks.push_back({36, "kick", std::vector<uint8_t>(n, 0)});
            }
            break;
        case EditMenu::Bass:
            bassWork_.steps.assign(static_cast<size_t>(n), BASS_DEGREE_REST);
            bassWork_.sustain_steps.assign(static_cast<size_t>(n), 0);
            bassWork_.hold = false;
            break;
        case EditMenu::Chord:
            chordWork_.steps.assign(static_cast<size_t>(n), 0);
            chordWork_.sustain_steps.assign(static_cast<size_t>(n), 0);
            break;
        default:
            break;
    }
    dirty_ = true;
}

std::string PatternEditorEngine::serializeWorking() const {
    const int n = loopSteps();
    switch (kind_) {
        case EditMenu::Rhythm: {
            RhythmPattern p = rhythmWork_;
            p.steps_per_bar = stepsPerBar_;
            for (auto& t : p.tracks) {
                t.pattern.resize(static_cast<size_t>(n), 0);
            }
            return serializeRhythmPattern(p, false);
        }
        case EditMenu::Bass: {
            BassPattern p = bassWork_;
            p.steps_per_bar = stepsPerBar_;
            p.steps.resize(static_cast<size_t>(n), BASS_DEGREE_REST);
            bool anySustain = false;
            for (uint8_t v : p.sustain_steps) {
                if (v != 0) { anySustain = true; break; }
            }
            if (anySustain) {
                p.sustain_steps.resize(static_cast<size_t>(n), 0);
            } else {
                p.sustain_steps.clear();
            }
            return serializeBassPattern(p, false);
        }
        case EditMenu::Chord: {
            ChordPattern p = chordWork_;
            p.steps_per_bar = stepsPerBar_;
            p.steps.resize(static_cast<size_t>(n), 0);
            bool anySustain = false;
            for (uint8_t v : p.sustain_steps) {
                if (v != 0) { anySustain = true; break; }
            }
            if (anySustain) {
                p.sustain_steps.resize(static_cast<size_t>(n), 0);
            } else {
                p.sustain_steps.clear();
            }
            return serializeChordPattern(p, false);
        }
        default:
            return "";
    }
}

void PatternEditorEngine::reloadLibrary(EditMenu kind) {
    switch (kind) {
        case EditMenu::Rhythm: {
            RhythmLibrary lib = loadRhythmPatterns(storage_);
            installRhythmNames(lib.names);
            installRhythmShortNames(lib.short_names);
            rhythmPatterns_ = std::move(lib.patterns);
            rhythm_.stagePatterns(rhythmPatterns_);
            break;
        }
        case EditMenu::Bass: {
            BassLibrary lib = loadBassPatterns(storage_);
            installBassNames(lib.names);
            bassPatterns_ = std::move(lib.patterns);
            bass_.setPatterns(bassPatterns_);
            break;
        }
        case EditMenu::Chord: {
            ChordPatternLibrary lib = loadChordPatterns(storage_);
            installChordPatternNames(lib.names);
            chordPatterns_ = std::move(lib.patterns);
            chord_.setPatterns(chordPatterns_);
            break;
        }
        default:
            break;
    }
}

void PatternEditorEngine::selectSavedPattern(const std::string& name) {
    int idx = -1;
    switch (kind_) {
        case EditMenu::Rhythm: idx = rhythmIndex(name); break;
        case EditMenu::Bass:   idx = bassIndex(name); break;
        case EditMenu::Chord:  idx = chordPatternIndex(name); break;
        default: break;
    }
    if (idx < 0) idx = 0;
    patternIndex_ = idx;
    builtin_ = idx < builtinCount(kind_);
    fileName_ = fileForIndex(kind_, idx);
    switch (kind_) {
        case EditMenu::Rhythm: state_.pendingRhythm.pattern = static_cast<uint8_t>(idx); break;
        case EditMenu::Bass:   state_.pendingBass.pattern = static_cast<uint8_t>(idx); break;
        case EditMenu::Chord:  state_.pendingChord.pattern = static_cast<uint8_t>(idx); break;
        default: break;
    }
    if (modified_) modified_();
}

// --- commands/screens ------------------------------------------------------

const char* PatternEditorEngine::editorTitle() const {
    switch (kind_) {
        case EditMenu::Chord:  return "Chord Pattern";
        case EditMenu::Rhythm: return "Rhythm Pattern";
        case EditMenu::Bass:   return "Bass Pattern";
        default:               return "Pattern Edit";
    }
}

const char* PatternEditorEngine::commandName(Command cmd) const {
    switch (cmd) {
        case Command::Pattern: return "Pattern";
        case Command::Bars: return "Bars";
        case Command::Quantize: return "Quantize";
        case Command::Grid: return "Grid";
        case Command::Record: return "Record";
        case Command::Preview: return "Preview";
        case Command::New: return "New";
        case Command::Rename: return "Rename";
        case Command::Save: return "Save";
        case Command::SaveAs: return "Save As";
        case Command::Delete: return "Delete";
        case Command::Clear: return "Clear";
        default: return "";
    }
}

std::string PatternEditorEngine::commandLabel(Command cmd) const {
    switch (cmd) {
        case Command::Pattern: {
            std::string n = workingName();
            if (n.size() > 10) n.resize(10);
            return std::string("Pat: ") + n;
        }
        case Command::Bars:
            return std::string("Bars: ") + std::to_string(bars_);
        case Command::Quantize:
            return std::string("Quant: 1/") + std::to_string(quantize_);
        default:
            return commandName(cmd);
    }
}

std::string PatternEditorEngine::commandFullName(Command cmd) const {
    switch (cmd) {
        case Command::Pattern: return "Pattern";
        case Command::Bars: return "Bars";
        case Command::Quantize: return "Quantize";
        default: return commandName(cmd);
    }
}

bool PatternEditorEngine::isValueCommand(Command cmd) const {
    return cmd == Command::Pattern || cmd == Command::Bars ||
           cmd == Command::Quantize;
}

int PatternEditorEngine::commandValue(Command cmd) const {
    switch (cmd) {
        case Command::Pattern:
            return patternIndex_;
        case Command::Bars: return bars_;
        case Command::Quantize: return quantize_;
        default: return 0;
    }
}

std::string PatternEditorEngine::selectedValueString() const {
    switch (selectedCommand_) {
        case Command::Pattern: {
            if (pendingValue_ < 0) return workingName();
            const char* n = patternName(kind_, pendingValue_);
            return n ? std::string(n) : workingName();
        }
        case Command::Bars:
            return std::to_string(pendingValue_);
        case Command::Quantize:
            return std::string("1/") + std::to_string(pendingValue_);
        default:
            return "";
    }
}

void PatternEditorEngine::showSelectedValue(uint64_t now_us) {
    display_.selectParam(commandName(selectedCommand_), true);
    display_.showValue(commandFullName(selectedCommand_), selectedValueString(),
                       true, now_us);
}

void PatternEditorEngine::beginValueSelection(Command cmd, uint64_t now_us) {
    valueSelected_ = true;
    selectedCommand_ = cmd;
    pendingValue_ = commandValue(cmd);
    statusActive_ = false;
    showSelectedValue(now_us);
}

void PatternEditorEngine::adjustSelectedValue(int dir, uint64_t now_us) {
    if (dir == 0) return;
    switch (selectedCommand_) {
        case Command::Pattern: {
            int count = patternCount(kind_);
            if (count > 0) {
                if (pendingValue_ < 0) {
                    pendingValue_ = (dir > 0) ? 0 : count - 1;
                } else {
                    pendingValue_ = (pendingValue_ + dir + count) % count;
                }
            }
            break;
        }
        case Command::Bars: {
            int cur = 0;
            for (int i = 0; i < 4; i++) {
                if (BAR_CHOICES[i] == pendingValue_) { cur = i; break; }
            }
            int next = (cur + (dir > 0 ? 1 : -1) + 4) % 4;
            pendingValue_ = BAR_CHOICES[next];
            break;
        }
        case Command::Quantize: {
            int cur = (pendingValue_ == 4) ? 0 : (pendingValue_ == 8 ? 1 : 2);
            cur = (cur + (dir > 0 ? 1 : -1) + 3) % 3;
            pendingValue_ = QUANTIZE_CHOICES[cur];
            break;
        }
        default:
            break;
    }
    showSelectedValue(now_us);
}

void PatternEditorEngine::commitSelectedValue(uint64_t now_us) {
    Command cmd = selectedCommand_;
    int value = pendingValue_;
    valueSelected_ = false;
    switch (cmd) {
        case Command::Pattern:
            if (value >= 0 && value != patternIndex_) {
                requestDiscard(PendingAction::SwitchPattern, kind_, value,
                               now_us);
            } else {
                renderMenu();
            }
            break;
        case Command::Bars:
            if (value != bars_) {
                if (kind_ == EditMenu::Bass && bassWork_.hold) {
                    showStatus(editorTitle(), "Hold pattern", now_us);
                } else {
                    bars_ = static_cast<uint8_t>(value);
                    resizeWorking();
                    dirty_ = true;
                }
            }
            if (newFlow_ == NewFlow::Bars) {
                newFlow_ = NewFlow::RecordPrompt;
                beginPrompt(Prompt::NewRecord, "Record now?", "Ent/Bk", now_us);
            } else {
                renderMenu();
            }
            break;
        case Command::Quantize:
            quantize_ = static_cast<uint8_t>(value);
            renderMenu();
            break;
        default:
            renderMenu();
            break;
    }
}

void PatternEditorEngine::cancelSelectedValue(uint64_t now_us) {
    valueSelected_ = false;
    if (newFlow_ == NewFlow::Bars) {
        newFlow_ = NewFlow::None;
        loadCurrentPresetPattern();
        renderMenu();
        return;
    }
    (void)now_us;
    renderMenu();
}

int PatternEditorEngine::navDirection(const KeyEvent& ev) const {
    if (ev.usage_page == hid_usage::PAGE_KEYBOARD) {
        if (ev.hid_usage == HID_UP || ev.hid_usage == HID_LEFT) return -1;
        if (ev.hid_usage == HID_DOWN || ev.hid_usage == HID_RIGHT) return +1;
        return 0;
    }
    if (ev.usage_page == hid_usage::PAGE_CONSUMER) {
        if (ev.hid_usage == hid_usage::CONSUMER_VOLUME_UP) return -1;
        if (ev.hid_usage == hid_usage::CONSUMER_VOLUME_DOWN) return +1;
    }
    return 0;
}

void PatternEditorEngine::renderMenu() {
    if (!active_) return;
    display_.showMenu(editorTitle(),
                      commandLabel(static_cast<Command>(command_)), false);
}

void PatternEditorEngine::showForcedPersistent(const std::string& line1,
                                               const std::string& line2) {
    // DisplayManager::showPersistent deliberately does not replace a menu or
    // prompt; the editor transitions from its menu to grid/record/status, so
    // clear the current screen first.
    display_.cancel();
    display_.showPersistent(line1, line2);
}

void PatternEditorEngine::renderGrid() {
    if (!active_) return;
    showForcedPersistent(gridLine1(), gridLine2());
}

void PatternEditorEngine::renderRecording() {
    if (!active_) return;
    showForcedPersistent(recordLine1_, recordLine2_);
}

void PatternEditorEngine::renderPreview() {
    if (!active_) return;
    showForcedPersistent("Preview", "Loop");
}

void PatternEditorEngine::render() {
    if (!active_) return;
    switch (screen_) {
        case Screen::Menu: renderMenu(); break;
        case Screen::Grid: renderGrid(); break;
        case Screen::Recording: renderRecording(); break;
        case Screen::Preview: renderPreview(); break;
        default: break;
    }
}

void PatternEditorEngine::showStatus(const std::string& line1,
                                     const std::string& line2,
                                     uint64_t now_us) {
    statusActive_ = true;
    statusUntilUs_ = now_us +
        static_cast<uint64_t>(state_.config.display_revert_ms) * 1000ULL;
    showForcedPersistent(line1, line2);
}

void PatternEditorEngine::beginPrompt(Prompt prompt, const std::string& l1,
                                      const std::string& l2,
                                      uint64_t now_us) {
    prompt_ = prompt;
    promptDeadlineUs_ = now_us +
        static_cast<uint64_t>(state_.config.display_prompt_ms) * 1000ULL;
    statusActive_ = false;
    valueSelected_ = false;
    display_.showPrompt(l1, l2, now_us);
}

void PatternEditorEngine::cancelPrompt(uint64_t now_us) {
    bool declinedNewRecord = prompt_ == Prompt::NewRecord;
    prompt_ = Prompt::None;
    pendingAction_ = PendingAction::None;
    if (declinedNewRecord) {
        newFlow_ = NewFlow::None;
        showStatus("New Pat " + std::to_string(newFileNumber_), "Created", now_us);
        return;
    }
    render();
}

void PatternEditorEngine::confirmPrompt(uint64_t now_us) {
    Prompt p = prompt_;
    prompt_ = Prompt::None;
    switch (p) {
        case Prompt::Discard:
            performPending(now_us);
            break;
        case Prompt::OverwriteBuiltin:
            doSave(now_us);
            break;
        case Prompt::Delete:
            doDelete(now_us);
            break;
        case Prompt::Clear:
            doClear(now_us);
            break;
        case Prompt::NewRecord:
            newFlow_ = NewFlow::None;
            startRecordArm(now_us);
            break;
        default:
            render();
            break;
    }
}

void PatternEditorEngine::requestDiscard(PendingAction action, EditMenu kind,
                                         int pattern, uint64_t now_us) {
    pendingAction_ = action;
    pendingPattern_ = pattern;
    (void)kind;
    if (!dirty_) {
        performPending(now_us);
        return;
    }
    beginPrompt(Prompt::Discard, "Discard?", "Ent/Bk", now_us);
}

void PatternEditorEngine::performPending(uint64_t now_us) {
    PendingAction action = pendingAction_;
    pendingAction_ = PendingAction::None;
    switch (action) {
        case PendingAction::Exit:
            close(now_us);
            break;
        case PendingAction::New:
            doNew(now_us);
            break;
        case PendingAction::SwitchPattern:
            loadPattern(kind_, pendingPattern_);
            break;
        default:
            render();
            break;
    }
}

void PatternEditorEngine::executeCommand(Command cmd, uint64_t now_us) {
    if (isValueCommand(cmd)) {
        beginValueSelection(cmd, now_us);
        return;
    }
    switch (cmd) {
        case Command::Grid:
            if (kind_ == EditMenu::Bass && bassWork_.hold) {
                showStatus(editorTitle(), "Hold pattern", now_us);
                return;
            }
            screen_ = Screen::Grid;
            statusActive_ = false;
            renderGrid();
            break;
        case Command::Record:
            if (recording_ || armed_) stopRecording(now_us, "Recorded");
            else startRecordArm(now_us);
            break;
        case Command::Preview:
            if (previewing_) stopPreview(now_us, true);
            else startPreview(now_us);
            break;
        case Command::New:
            requestDiscard(PendingAction::New, kind_, 0, now_us);
            break;
        case Command::Rename:
            beginRename(now_us);
            break;
        case Command::Save:
            saveCurrent(false, now_us);
            break;
        case Command::SaveAs:
            saveCurrent(true, now_us);
            break;
        case Command::Delete:
            if (builtin_) {
                showStatus(editorTitle(), "Built-in", now_us);
            } else if (newUnsaved_) {
                loadCurrentPresetPattern();
                showStatus(editorTitle(), "Discarded", now_us);
            } else {
                beginPrompt(Prompt::Delete, "Delete pattern?", "Ent/Bk", now_us);
            }
            break;
        case Command::Clear:
            beginPrompt(Prompt::Clear, "Clear pattern?", "Ent/Bk", now_us);
            break;
        default:
            break;
    }
}

// --- record/preview --------------------------------------------------------

void PatternEditorEngine::ensureRhythm(uint64_t now_us) {
    (void)now_us;
    if (!rhythmStateSaved_) {
        savedRhythmEnabled_ = state_.pendingRhythm.enabled;
        savedRhythmMuted_ = state_.pendingRhythm.muted;
        rhythmStateSaved_ = true;
    }
    if (!state_.pendingRhythm.enabled) {
        state_.pendingRhythm.enabled = true;
        if (modified_) modified_();
    }
    if (kind_ == EditMenu::Rhythm) {
        // The rhythm pattern editor always emits; keep the forced unmuted state
        // through preview/record as well.
        if (state_.pendingRhythm.muted) {
            state_.pendingRhythm.muted = false;
            if (modified_) modified_();
        }
    } else if (!state_.pendingRhythm.muted) {
        state_.pendingRhythm.muted = true;
        if (modified_) modified_();
    }
    lastRhythmEnabled_ = state_.pendingRhythm.enabled;
    lastRhythmMuted_ = state_.pendingRhythm.muted;
}

void PatternEditorEngine::restoreRhythm(uint64_t now_us) {
    (void)now_us;
    if (!rhythmStateSaved_) return;
    // Preserve explicit F5/F6-style enable/mute edits made while the editor
    // auto-enabled the rhythm. If the user did not touch them, restore the
    // pre-editor state.
    if (state_.pendingRhythm.enabled != lastRhythmEnabled_) {
        savedRhythmEnabled_ = state_.pendingRhythm.enabled;
        lastRhythmEnabled_ = state_.pendingRhythm.enabled;
    }
    if (state_.pendingRhythm.muted != lastRhythmMuted_) {
        savedRhythmMuted_ = state_.pendingRhythm.muted;
        lastRhythmMuted_ = state_.pendingRhythm.muted;
    }
    state_.pendingRhythm.enabled = savedRhythmEnabled_;
    state_.pendingRhythm.muted = savedRhythmMuted_;
    rhythmStateSaved_ = false;
    if (modified_) modified_();
}

void PatternEditorEngine::startRecordArm(uint64_t now_us) {
    if (kind_ == EditMenu::Bass && bassWork_.hold) {
        showStatus(editorTitle(), "Hold pattern", now_us);
        return;
    }
    if (!meterMatchesLive()) {
        showStatus("Meter mismatch", "Select rhythm", now_us);
        return;
    }
    ensureRhythm(now_us);
    armed_ = true;
    recording_ = false;
    countInPlanned_ = false;
    recordStartAbs_ = 0;
    lastRecordBar_ = 0;
    armStartedUs_ = now_us;
    armLastProgressUs_ = now_us;
    armLastStepAbs_ = state_.rhythmClock.stepAbs.load();
    state_.recordGuideActive.store(true);
    state_.recordGuideHatsActive.store(true);
    state_.recordGuideHatsStartAbs.store(0);
    if (state_.rhythmClock.running.load()) {
        planCountIn(state_.rhythmClock.stepAbs.load());
    }
    screen_ = Screen::Recording;
    statusActive_ = false;
    recordLine1_ = "Rec arm";
    recordLine2_ = "Wait bar";
    renderRecording();
}

void PatternEditorEngine::planCountIn(uint32_t stepAbs) {
    int spb = liveStepsPerBar();
    if (spb <= 0) return;
    uint32_t u = static_cast<uint32_t>(spb);
    uint32_t toBar = (u - (stepAbs % u)) % u;
    countInStartAbs_ = stepAbs + toBar;
    recordStartAbs_ = countInStartAbs_ + u;
    state_.recordGuideHatsStartAbs.store(countInStartAbs_);
    countInPlanned_ = true;
}

void PatternEditorEngine::startRecording(uint64_t now_us) {
    (void)now_us;
    armed_ = false;
    recording_ = true;
    lastRecordBar_ = 1;
    recordLine1_ = "Recording 1/" + std::to_string(bars_);
    recordLine2_.clear();
    renderRecording();
}

void PatternEditorEngine::stopRecording(uint64_t now_us, const char* message) {
    if (!recording_ && !armed_) return;
    bool hadTake = recording_;
    recording_ = false;
    armed_ = false;
    countInPlanned_ = false;
    state_.recordGuideActive.store(false);
    state_.recordGuideHatsActive.store(false);
    state_.recordGuideHatsStartAbs.store(0);
    if (hadTake) {
        if (bassPadActive_) releaseBassPad(now_us);
        for (int i = 0; i < 4; i++) {
            if (chordPadActive_[i]) releaseChordPad(i, now_us);
        }
    }
    bassPadActive_ = false;
    for (bool& b : chordPadActive_) b = false;
    chordLiveMask_ = 0;
    releaseChordLiveAudition();
    releaseAudition();
    armLastStepAbs_ = 0;
    armStartedUs_ = 0;
    armLastProgressUs_ = 0;
    screen_ = Screen::Menu;
    if (hadTake) dirty_ = true;
    if (message) showStatus(editorTitle(), message, now_us);
    else renderMenu();
}

void PatternEditorEngine::updateRecording(uint64_t now_us) {
    if (!armed_ && !recording_) return;
    uint32_t stepAbs = state_.rhythmClock.stepAbs.load();

    if (armed_) {
        uint32_t spb = static_cast<uint32_t>(liveStepsPerBar());
        if (spb == 0) spb = 16;
        uint64_t barUs =
            static_cast<uint64_t>(stepUs(state_.pendingRhythm.tempo)) * spb;

        if (!state_.rhythmClock.running.load()) {
            // The engine may still be starting after the editor enabled it.
            if (now_us - armStartedUs_ >= barUs) ensureRhythm(now_us);
            if (now_us - armStartedUs_ >= barUs * 2) {
                stopRecording(now_us, "Rhythm stalled");
            }
            return;
        }

        // If the rhythm engine restarted (disabled -> enabled) after the count-in
        // was planned, stepAbs jumps back to 0. Re-plan instead of waiting for a
        // count-in target that may now be minutes away.
        if (countInPlanned_ && stepAbs < countInStartAbs_) {
            if (countInStartAbs_ - stepAbs > spb) {
                countInPlanned_ = false;
                armLastStepAbs_ = stepAbs;
                armLastProgressUs_ = now_us;
            }
        }
        if (!countInPlanned_) {
            planCountIn(stepAbs);
            armLastStepAbs_ = stepAbs;
            armLastProgressUs_ = now_us;
        }
        if (stepAbs != armLastStepAbs_) {
            armLastStepAbs_ = stepAbs;
            armLastProgressUs_ = now_us;
        } else if (now_us - armLastProgressUs_ >= barUs * 2) {
            // A running-but-frozen clock must not leave the editor on Wait bar.
            stopRecording(now_us, "Rhythm stalled");
            return;
        }
        if (stepAbs < countInStartAbs_) {
            if (recordLine1_ != "Rec arm" || recordLine2_ != "Wait bar") {
                recordLine1_ = "Rec arm";
                recordLine2_ = "Wait bar";
                renderRecording();
            }
        } else if (stepAbs < recordStartAbs_) {
            if (recordLine1_ != "Count-in" || recordLine2_ != "1 bar") {
                recordLine1_ = "Count-in";
                recordLine2_ = "1 bar";
                renderRecording();
            }
        } else {
            startRecording(now_us);
        }
        return;
    }

    if (!state_.rhythmClock.running.load()) return;

    int loop = loopSteps();
    if (loop <= 0) return;
    if (stepAbs <= recordStartAbs_) return;
    uint32_t rel = (stepAbs - 1) - recordStartAbs_;
    if (rel >= static_cast<uint32_t>(loop)) {
        stopRecording(now_us, "Recorded");
        return;
    }
    int bar = static_cast<int>(rel / stepsPerBar_) + 1;
    if (bar != lastRecordBar_) {
        lastRecordBar_ = static_cast<uint8_t>(bar);
        recordLine1_ = "Recording " + std::to_string(bar) + "/" +
                       std::to_string(bars_);
        renderRecording();
    }
    if ((stepAbs - recordStartAbs_) >= static_cast<uint32_t>(loop)) {
        stopRecording(now_us, "Recorded");
    }
}

void PatternEditorEngine::startPreview(uint64_t now_us) {
    if (kind_ == EditMenu::Bass && bassWork_.hold) {
        showStatus(editorTitle(), "Hold pattern", now_us);
        return;
    }
    if (!meterMatchesLive()) {
        showStatus("Meter mismatch", "Select rhythm", now_us);
        return;
    }
    ensureRhythm(now_us);
    state_.recordGuideActive.store(false);
    state_.recordGuideHatsActive.store(false);
    state_.recordGuideHatsStartAbs.store(0);
    previewing_ = true;
    previewFirstPending_ = true;
    int spb = liveStepsPerBar();
    uint32_t stepAbs = state_.rhythmClock.stepAbs.load();
    uint32_t u = static_cast<uint32_t>(spb);
    uint32_t toBar = (u - (stepAbs % u)) % u;
    previewStartAbs_ = stepAbs + toBar;
    lastPreviewAbs_ = previewStartAbs_ - 1;
    statusActive_ = false;
    screen_ = Screen::Preview;
    renderPreview();
}

void PatternEditorEngine::stopPreview(uint64_t now_us, bool showStatusMsg) {
    if (!previewing_) return;
    previewing_ = false;
    previewFirstPending_ = false;
    releasePreviewNotes();
    screen_ = Screen::Menu;
    if (showStatusMsg) showStatus("Preview", "Stopped", now_us);
    else renderMenu();
}

void PatternEditorEngine::updatePreview(uint64_t now_us) {
    if (!previewing_) return;
    // ensureRhythm() enables the rhythm engine; Core 1 publishes the running
    // flag asynchronously. Wait for it rather than cancelling preview instantly.
    if (!state_.rhythmClock.running.load()) return;
    uint32_t stepAbs = state_.rhythmClock.stepAbs.load();
    int loop = loopSteps();
    if (loop <= 0) return;

    if (previewFirstPending_) {
        if (stepAbs <= previewStartAbs_) return;
        processPreviewStep(0, now_us);
        previewFirstPending_ = false;
        lastPreviewAbs_ = previewStartAbs_;
        return;
    }
    if (stepAbs <= lastPreviewAbs_ + 1) return;
    uint32_t fired = stepAbs - 1;
    if (fired < previewStartAbs_) return;
    uint32_t rel = (fired - previewStartAbs_) % static_cast<uint32_t>(loop);
    processPreviewStep(static_cast<int>(rel), now_us);
    lastPreviewAbs_ = fired;
}

void PatternEditorEngine::processPreviewStep(int rel, uint64_t now_us) {
    const int n = loopSteps();
    if (rel < 0 || rel >= n) return;

    switch (kind_) {
        case EditMenu::Rhythm: {
            const RhythmParams& rp = state_.pendingRhythm;
            for (const auto& t : rhythmWork_.tracks) {
                if (rel >= static_cast<int>(t.pattern.size())) continue;
                uint8_t v = t.pattern[rel];
                if (v == 0) continue;
                if (v == 1) v = RHYTHM_DEFAULT_VELOCITY;
                uint8_t note = mapDrumNote(t.note, rp.drums);
                uint8_t vel = mapDrumVelocity(t.note, rp.drums, v);
                if (vel == 0) continue;
                router_.noteOn(rp.channel, note, vel);
            }
            break;
        }
        case EditMenu::Bass: {
            if (rel >= static_cast<int>(bassWork_.steps.size())) break;
            int degree = bassWork_.steps[rel];
            if (degree < 0) break;
            releasePreviewNotes();
            ResolvedChord c;
            chordForPreview(c);
            int offset = bassDegreeOffset(c.type, degree);
            if (offset < 0) break;
            int root = rootMidi(c.rootPc, state_.config.base_root_midi,
                                state_.pendingBass.octave);
            int note = root + offset;
            note = std::max(0, std::min(127, note));
            const BassParams& b = state_.pendingBass;
            router_.noteOn(b.channel, static_cast<uint8_t>(note), b.velocity);
            previewNotes_.push_back(static_cast<uint8_t>(note));
            previewChannel_ = b.channel;
            int sustain = bassStepSustain(bassWork_, rel);
            uint64_t dur = (sustain > 0)
                ? static_cast<uint64_t>(stepUs(state_.pendingRhythm.tempo)) *
                      static_cast<uint64_t>(sustain)
                : static_cast<uint64_t>(b.note_duration_ms) * 1000ULL;
            previewOffDeadlineUs_ = now_us + dur;
            break;
        }
        case EditMenu::Chord: {
            if (rel >= static_cast<int>(chordWork_.steps.size())) break;
            uint8_t mask = chordWork_.steps[rel];
            if (mask == 0) break;
            releasePreviewNotes();
            ResolvedChord c;
            chordForPreview(c);
            std::vector<int> offsets = chordPatternOffsets(c.type, mask);
            int root = rootMidi(c.rootPc, state_.config.base_root_midi,
                                state_.pendingChord.octave);
            const ChordParams& p = state_.pendingChord;
            for (int off : offsets) {
                int note = std::max(0, std::min(127, root + off));
                router_.noteOn(p.channel, static_cast<uint8_t>(note), p.velocity);
                previewNotes_.push_back(static_cast<uint8_t>(note));
            }
            previewChannel_ = p.channel;
            int sustain = chordPatternStepSustain(chordWork_, rel);
            uint64_t dur = (sustain > 0)
                ? static_cast<uint64_t>(stepUs(state_.pendingRhythm.tempo)) *
                      static_cast<uint64_t>(sustain)
                : static_cast<uint64_t>(p.note_duration_ms) * 1000ULL;
            previewOffDeadlineUs_ = now_us + dur;
            break;
        }
        default:
            break;
    }
}

bool PatternEditorEngine::recordStepFor(uint64_t now_us, int& rel,
                                        uint32_t& abs) const {
    if (!recording_) return false;
    uint32_t nextAbs = state_.rhythmClock.stepAbs.load();
    if (nextAbs == 0) return false;
    uint32_t stepDur = stepUs(state_.pendingRhythm.tempo);
    uint64_t next = state_.rhythmClock.nextStepUs.load();
    uint32_t chosen = nextAbs - 1;
    if (next > 0 && next > now_us && (next - now_us) < stepDur / 2) {
        chosen = nextAbs;
    }
    int q = 16 / (quantize_ ? quantize_ : 16);
    if (q < 1) q = 1;
    uint32_t snapped = ((chosen + static_cast<uint32_t>(q) / 2) /
                        static_cast<uint32_t>(q)) * static_cast<uint32_t>(q);
    if (snapped < recordStartAbs_) return false;
    uint32_t r = snapped - recordStartAbs_;
    int loop = loopSteps();
    if (r >= static_cast<uint32_t>(loop)) return false;
    rel = static_cast<int>(r);
    abs = snapped;
    return true;
}

void PatternEditorEngine::recordRhythmPad(int piece, uint64_t now_us) {
    int rel = 0;
    uint32_t abs = 0;
    if (!recordStepFor(now_us, rel, abs)) return;
    int idx = ensureRhythmTrack(piece);
    if (idx < 0) return;
    if (rel >= static_cast<int>(rhythmWork_.tracks[idx].pattern.size())) return;
    rhythmWork_.tracks[idx].pattern[rel] = 1;
    dirty_ = true;
}

void PatternEditorEngine::recordBassPad(int degree, uint64_t now_us) {
    int rel = 0;
    uint32_t abs = 0;
    if (!recordStepFor(now_us, rel, abs)) return;
    if (rel >= static_cast<int>(bassWork_.steps.size())) return;
    if (degree < 0) {
        bassWork_.steps[rel] = BASS_DEGREE_REST;
        if (rel < static_cast<int>(bassWork_.sustain_steps.size())) {
            bassWork_.sustain_steps[rel] = 0;
        }
        bassPadActive_ = false;
        dirty_ = true;
        return;
    }
    bassWork_.steps[rel] = static_cast<int8_t>(degree);
    if (rel < static_cast<int>(bassWork_.sustain_steps.size())) {
        bassWork_.sustain_steps[rel] = 0;
    }
    bassPadActive_ = true;
    bassPadDegree_ = degree;
    bassPadAbs_ = abs;
    bassPadRel_ = static_cast<uint16_t>(rel);
    dirty_ = true;
}

void PatternEditorEngine::releaseBassPad(uint64_t now_us) {
    (void)now_us;
    if (!bassPadActive_) return;
    uint32_t currentAbs = state_.rhythmClock.stepAbs.load();
    uint32_t held = (currentAbs > bassPadAbs_) ? currentAbs - bassPadAbs_ : 0;
    int maxHeld = loopSteps() - static_cast<int>(bassPadRel_);
    if (maxHeld < 0) maxHeld = 0;
    if (held > static_cast<uint32_t>(maxHeld)) held = static_cast<uint32_t>(maxHeld);
    if (held > 0 && bassPadRel_ < bassWork_.sustain_steps.size() &&
        bassWork_.steps[bassPadRel_] == bassPadDegree_) {
        uint8_t& s = bassWork_.sustain_steps[bassPadRel_];
        if (held > s) s = static_cast<uint8_t>(std::min<uint32_t>(held, 255));
        dirty_ = true;
    }
    bassPadActive_ = false;
}

void PatternEditorEngine::recordChordPad(int bit, uint64_t now_us) {
    if (bit < 0 || bit >= 4) return;
    int rel = 0;
    uint32_t abs = 0;
    if (!recordStepFor(now_us, rel, abs)) return;
    if (rel >= static_cast<int>(chordWork_.steps.size())) return;
    chordWork_.steps[rel] |= static_cast<uint8_t>(1u << bit);
    chordPadActive_[bit] = true;
    chordPadAbs_[bit] = abs;
    chordPadRel_[bit] = static_cast<uint16_t>(rel);
    dirty_ = true;
}

void PatternEditorEngine::releaseChordPad(int bit, uint64_t now_us) {
    (void)now_us;
    if (bit < 0 || bit >= 4 || !chordPadActive_[bit]) return;
    uint32_t currentAbs = state_.rhythmClock.stepAbs.load();
    uint32_t held = (currentAbs > chordPadAbs_[bit])
        ? currentAbs - chordPadAbs_[bit] : 0;
    int rel = chordPadRel_[bit];
    int maxHeld = loopSteps() - rel;
    if (maxHeld < 0) maxHeld = 0;
    if (held > static_cast<uint32_t>(maxHeld)) held = static_cast<uint32_t>(maxHeld);
    if (held > 0 && rel >= 0 &&
        rel < static_cast<int>(chordWork_.sustain_steps.size()) &&
        (chordWork_.steps[rel] & (1u << bit))) {
        uint8_t& s = chordWork_.sustain_steps[rel];
        if (held > s) s = static_cast<uint8_t>(std::min<uint32_t>(held, 255));
        dirty_ = true;
    }
    chordPadActive_[bit] = false;
}

void PatternEditorEngine::clearChordStep(uint64_t now_us, int rel) {
    (void)now_us;
    if (rel < 0 || rel >= static_cast<int>(chordWork_.steps.size())) return;
    chordWork_.steps[rel] = 0;
    if (rel < static_cast<int>(chordWork_.sustain_steps.size())) {
        chordWork_.sustain_steps[rel] = 0;
    }
    for (int i = 0; i < 4; i++) {
        if (chordPadActive_[i] && chordPadRel_[i] == rel) {
            chordPadActive_[i] = false;
        }
    }
    dirty_ = true;
}

// --- midi ------------------------------------------------------------------

void PatternEditorEngine::releasePreviewNotes() {
    for (uint8_t n : previewNotes_) {
        router_.noteOff(previewChannel_, n);
    }
    previewNotes_.clear();
}

void PatternEditorEngine::releaseAudition() {
    for (uint8_t n : auditionNotes_) {
        router_.noteOff(auditionChannel_, n);
    }
    auditionNotes_.clear();
}

void PatternEditorEngine::sendAudition(const std::vector<uint8_t>& notes,
                                       uint8_t channel, uint8_t velocity,
                                       uint64_t now_us, uint32_t durationMs) {
    releaseAudition();
    if (notes.empty()) return;
    auditionChannel_ = channel;
    for (uint8_t n : notes) {
        router_.noteOn(channel, n, velocity);
        auditionNotes_.push_back(n);
    }
    auditionOffDeadlineUs_ = now_us +
        static_cast<uint64_t>(durationMs) * 1000ULL;
}

void PatternEditorEngine::auditionRhythmPiece(int piece) {
    if (piece < 0 || piece >= 13) return;
    const RhythmParams& rp = state_.pendingRhythm;
    uint8_t note = mapDrumNote(kPieces[piece].note, rp.drums);
    uint8_t vel = mapDrumVelocity(kPieces[piece].note, rp.drums,
                                  RHYTHM_DEFAULT_VELOCITY);
    if (vel == 0) return;
    router_.noteOn(rp.channel, note, vel);
}

void PatternEditorEngine::auditionBassDegree(int degree, uint64_t now_us) {
    if (degree < 0) return;
    ResolvedChord c;
    chordForPreview(c);
    int offset = bassDegreeOffset(c.type, degree);
    if (offset < 0) return;
    int root = rootMidi(c.rootPc, state_.config.base_root_midi,
                        state_.pendingBass.octave);
    int note = std::max(0, std::min(127, root + offset));
    sendAudition({static_cast<uint8_t>(note)}, state_.pendingBass.channel,
                 state_.pendingBass.velocity, now_us,
                 state_.pendingBass.note_duration_ms);
}

void PatternEditorEngine::updateChordLiveAudition(uint8_t mask,
                                                   uint64_t now_us) {
    ResolvedChord c;
    chordForPreview(c);
    int root = rootMidi(c.rootPc, state_.config.base_root_midi,
                        state_.pendingChord.octave);
    const ChordParams& p = state_.pendingChord;
    uint32_t duration = p.note_duration_ms > 0
        ? static_cast<uint32_t>(p.note_duration_ms) : 1u;

    uint8_t remove = static_cast<uint8_t>(chordLiveSoundingMask_ & ~mask);
    for (int bit = 0; bit < 4; bit++) {
        uint8_t b = static_cast<uint8_t>(1u << bit);
        if (!(remove & b)) continue;
        router_.noteOff(p.channel, chordLiveNotes_[bit]);
        chordLiveSoundingMask_ &= static_cast<uint8_t>(~b);
        chordLiveOffUs_[bit] = 0;
    }

    for (int bit = 0; bit < 4; bit++) {
        uint8_t b = static_cast<uint8_t>(1u << bit);
        if (!(mask & b) || (chordLiveSoundingMask_ & b)) continue;
        std::vector<int> offsets = chordPatternOffsets(
            c.type, static_cast<uint8_t>(b));
        if (offsets.empty()) continue;
        int note = root + offsets[0];
        note = std::max(0, std::min(127, note));
        router_.noteOn(p.channel, static_cast<uint8_t>(note), p.velocity);
        chordLiveNotes_[bit] = static_cast<uint8_t>(note);
        chordLiveOffUs_[bit] = now_us +
            static_cast<uint64_t>(duration) * 1000ULL;
        chordLiveSoundingMask_ |= b;
    }
}

void PatternEditorEngine::releaseChordLiveAudition() {
    const ChordParams& p = state_.pendingChord;
    for (int bit = 0; bit < 4; bit++) {
        uint8_t b = static_cast<uint8_t>(1u << bit);
        if (!(chordLiveSoundingMask_ & b)) continue;
        router_.noteOff(p.channel, chordLiveNotes_[bit]);
        chordLiveOffUs_[bit] = 0;
    }
    chordLiveSoundingMask_ = 0;
}

void PatternEditorEngine::expireChordLiveAudition(uint64_t now_us) {
    const ChordParams& p = state_.pendingChord;
    for (int bit = 0; bit < 4; bit++) {
        uint8_t b = static_cast<uint8_t>(1u << bit);
        if (!(chordLiveSoundingMask_ & b)) continue;
        if (chordLiveOffUs_[bit] != 0 && now_us >= chordLiveOffUs_[bit]) {
            router_.noteOff(p.channel, chordLiveNotes_[bit]);
            chordLiveSoundingMask_ &= static_cast<uint8_t>(~b);
            chordLiveOffUs_[bit] = 0;
        }
    }
}

bool PatternEditorEngine::chordForPreview(ResolvedChord& out) const {
    if (state_.selectedChordValid) {
        out = state_.selectedChord;
        return true;
    }
    out = ResolvedChord{0, ChordType::Major};
    return false;
}

// --- save/delete/name ------------------------------------------------------

void PatternEditorEngine::saveCurrent(bool saveAs, uint64_t now_us) {
    if (writeAllowed_ && !writeAllowed_()) {
        showStatus("USB drive active", "Save off", now_us);
        return;
    }
    if (saveAs) {
        std::string base = workingName();
        if (base.empty()) base = "Pattern";
        std::string newName = uniquePatternName(kind_, base + " Copy",
                                                workingName());
        fileName_ = uniqueUserFileName(kind_);
        builtin_ = false;
        newUnsaved_ = true;
        setWorkingName(newName);
    }
    if (builtin_ && !saveAs) {
        beginPrompt(Prompt::OverwriteBuiltin, "Overwrite?", "Ent/Bk", now_us);
        return;
    }
    doSave(now_us);
}

bool PatternEditorEngine::doSave(uint64_t now_us) {
    std::string json = serializeWorking();
    if (!pattern_limits::fileSizeAllowed(
            kind_ == EditMenu::Rhythm ? pattern_limits::Kind::Rhythm
          : kind_ == EditMenu::Bass   ? pattern_limits::Kind::Bass
                                      : pattern_limits::Kind::Chord,
            json.size())) {
        showStatus("Pattern too large", "", now_us);
        return false;
    }
    if (fileName_.empty()) fileName_ = uniqueUserFileName(kind_);
    std::string path = std::string(patternDir(kind_)) + "/" + fileName_;
    bool targetExists = storage_.exists(path);
    size_t oldSize = targetExists ? storage_.readFile(path).size() : 0;
    bool targetIsUser = !isBuiltinFile(kind_, fileName_);

    if (!targetExists && patternCount(kind_) >=
            pattern_limits::MAX_PATTERNS_PER_KIND) {
        showStatus("Pattern limit", "", now_us);
        return false;
    }

    size_t allBytes = 0;
    size_t userBytes = 0;
    const EditMenu kinds[] = {EditMenu::Rhythm, EditMenu::Bass, EditMenu::Chord};
    for (EditMenu k : kinds) {
        for (const auto& f : storage_.listFiles(patternDir(k))) {
            if (f.size() < 5 || f.compare(f.size() - 5, 5, ".json") != 0) continue;
            std::string p = std::string(patternDir(k)) + "/" + f;
            size_t sz = storage_.readFile(p).size();
            allBytes += sz;
            if (!isBuiltinFile(k, f)) userBytes += sz;
        }
    }
    if (allBytes >= oldSize) allBytes -= oldSize;
    else allBytes = 0;
    if (targetIsUser) {
        if (userBytes >= oldSize) userBytes -= oldSize;
        else userBytes = 0;
    }
    allBytes += json.size();
    if (targetIsUser) userBytes += json.size();

    if (!pattern_limits::budgetAllowed(allBytes, userBytes)) {
        showStatus("Pattern limit", "", now_us);
        return false;
    }
    if (!storage_.writeFile(path, json)) {
        showStatus("Pattern save fail", "", now_us);
        return false;
    }
    reloadLibrary(kind_);
    selectSavedPattern(workingName());
    newUnsaved_ = false;
    dirty_ = false;
    showStatus("Pattern saved", "", now_us);
    return true;
}

void PatternEditorEngine::doNew(uint64_t now_us) {
    fileName_ = uniqueUserFileName(kind_);
    int newNumber = userFileNumber(fileName_);
    if (newNumber <= 0) newNumber = 1;
    newFileNumber_ = newNumber;
    builtin_ = false;
    newUnsaved_ = true;
    dirty_ = true;
    patternIndex_ = -1;
    stepsPerBar_ = liveStepsPerBar();
    bars_ = 1;
    quantize_ = 16;
    cursorBar_ = 0;
    cursorStep_ = 0;
    cursorPiece_ = 0;
    switch (kind_) {
        case EditMenu::Rhythm: {
            std::string name = uniquePatternName(kind_, "New Rhythm", "");
            rhythmWork_ = RhythmPattern{};
            rhythmWork_.name = name;
            rhythmWork_.steps_per_bar = stepsPerBar_;
            rhythmWork_.swing = state_.pendingRhythm.swing;
            rhythmWork_.tracks = {{36, "kick", std::vector<uint8_t>(loopSteps(), 0)}};
            break;
        }
        case EditMenu::Bass: {
            std::string name = uniquePatternName(kind_, "New Bass", "");
            bassWork_ = BassPattern{};
            bassWork_.name = name;
            bassWork_.steps_per_bar = stepsPerBar_;
            break;
        }
        case EditMenu::Chord: {
            std::string name = uniquePatternName(kind_, "New Chord", "");
            chordWork_ = ChordPattern{};
            chordWork_.name = name;
            chordWork_.steps_per_bar = stepsPerBar_;
            break;
        }
        default:
            break;
    }
    normalizeWorking();
    screen_ = Screen::Menu;
    newFlow_ = NewFlow::Name;
    nameEditing_ = true;
    nameBuffer_ = workingName();
    statusActive_ = false;
    display_.showNameEdit(nameBuffer_);
    (void)now_us;
}

void PatternEditorEngine::doDelete(uint64_t now_us) {
    if (builtin_) {
        showStatus(editorTitle(), "Built-in", now_us);
        return;
    }
    if (newUnsaved_) {
        loadCurrentPresetPattern();
        showStatus(editorTitle(), "Discarded", now_us);
        return;
    }
    std::string path = std::string(patternDir(kind_)) + "/" + fileName_;
    storage_.remove(path);
    reloadLibrary(kind_);
    int count = patternCount(kind_);
    int idx = patternIndex_;
    if (idx >= count) idx = count - 1;
    if (idx < 0) idx = 0;
    dirty_ = false;
    loadPattern(kind_, idx);
    switch (kind_) {
        case EditMenu::Rhythm: state_.pendingRhythm.pattern = static_cast<uint8_t>(idx); break;
        case EditMenu::Bass:   state_.pendingBass.pattern = static_cast<uint8_t>(idx); break;
        case EditMenu::Chord:  state_.pendingChord.pattern = static_cast<uint8_t>(idx); break;
        default: break;
    }
    if (modified_) modified_();
    showStatus(editorTitle(), "Deleted", now_us);
}

void PatternEditorEngine::doClear(uint64_t now_us) {
    clearWorking();
    screen_ = Screen::Menu;
    renderMenu();
    showStatus(editorTitle(), "Cleared", now_us);
}

void PatternEditorEngine::beginRename(uint64_t now_us) {
    if (builtin_) {
        showStatus(editorTitle(), "Built-in", now_us);
        return;
    }
    nameEditing_ = true;
    nameBuffer_ = workingName();
    statusActive_ = false;
    display_.showNameEdit(nameBuffer_);
}

void PatternEditorEngine::commitName(uint64_t now_us) {
    std::string oldName = workingName();
    std::string base = nameBuffer_.empty() ? oldName : nameBuffer_;
    std::string name = uniquePatternName(kind_, base, oldName);
    setWorkingName(name);
    dirty_ = true;
    nameEditing_ = false;
    if (newFlow_ == NewFlow::Name) {
        newFlow_ = NewFlow::Bars;
        beginValueSelection(Command::Bars, now_us);
        return;
    }
    renderMenu();
}

void PatternEditorEngine::cancelNameEdit(uint64_t now_us) {
    nameEditing_ = false;
    if (newFlow_ == NewFlow::Name) {
        newFlow_ = NewFlow::None;
        loadCurrentPresetPattern();
        renderMenu();
        return;
    }
    (void)now_us;
    renderMenu();
}

bool PatternEditorEngine::handleNameEdit(const KeyEvent& ev, uint64_t now_us) {
    if (!ev.pressed) return true;
    if (isEnter(ev)) { commitName(now_us); return true; }
    if (ev.usage_page == hid_usage::PAGE_KEYBOARD &&
        ev.hid_usage == HID_ESC) {
        cancelNameEdit(now_us);
        return true;
    }
    if (ev.usage_page == hid_usage::PAGE_KEYBOARD &&
        ev.hid_usage == HID_BACKSPACE) {
        if (!nameBuffer_.empty()) {
            nameBuffer_.pop_back();
            display_.showNameEdit(nameBuffer_);
        }
        return true;
    }
    char c = keymapCharForUsage(ev.hid_usage, ev.modifiers, ev.usage_page);
    if (c != '\0' && nameBuffer_.size() < MAX_NAME_LEN) {
        nameBuffer_.push_back(c);
        display_.showNameEdit(nameBuffer_);
    }
    return true;
}

// --- helpers ---------------------------------------------------------------

int PatternEditorEngine::rhythmTrackIndex(int piece) const {
    if (piece < 0 || piece >= 13) return -1;
    for (size_t i = 0; i < rhythmWork_.tracks.size(); i++) {
        if (rhythmWork_.tracks[i].note == kPieces[piece].note) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int PatternEditorEngine::ensureRhythmTrack(int piece) {
    int idx = rhythmTrackIndex(piece);
    if (idx >= 0) return idx;
    if (piece < 0 || piece >= 13) return -1;
    RhythmTrack t;
    t.note = kPieces[piece].note;
    t.name = kPieces[piece].name;
    t.pattern.assign(static_cast<size_t>(loopSteps()), 0);
    rhythmWork_.tracks.push_back(std::move(t));
    return static_cast<int>(rhythmWork_.tracks.size()) - 1;
}

uint8_t PatternEditorEngine::rhythmCell(int piece, int step) const {
    int idx = rhythmTrackIndex(piece);
    if (idx < 0) return 0;
    const auto& p = rhythmWork_.tracks[idx].pattern;
    if (step < 0 || step >= static_cast<int>(p.size())) return 0;
    return p[step];
}

void PatternEditorEngine::editRhythmCell() {
    if (kind_ != EditMenu::Rhythm) return;
    int piece = cursorPiece_;
    int step = cursorRel();
    int idx = ensureRhythmTrack(piece);
    if (idx < 0) return;
    auto& p = rhythmWork_.tracks[idx].pattern;
    if (step < 0 || step >= static_cast<int>(p.size())) return;
    uint8_t v = p[step];
    if (v == 0) v = 1;
    else if (v < 100) v = 127;
    else v = 0;
    p[step] = v;
    dirty_ = true;
}

bool PatternEditorEngine::coveredBySustain(int step) const {
    if (kind_ == EditMenu::Rhythm) return false;
    if (step < 0) return false;
    if (kind_ == EditMenu::Bass) {
        if (step >= static_cast<int>(bassWork_.steps.size())) return false;
        for (int j = 0; j < step; j++) {
            if (j >= static_cast<int>(bassWork_.steps.size())) break;
            if (bassWork_.steps[j] < 0) continue;
            int s = (j < static_cast<int>(bassWork_.sustain_steps.size()))
                ? bassWork_.sustain_steps[j] : 0;
            if (s > 0 && step < j + s) return true;
        }
        return false;
    }
    if (kind_ == EditMenu::Chord) {
        if (step >= static_cast<int>(chordWork_.steps.size())) return false;
        for (int j = 0; j < step; j++) {
            if (j >= static_cast<int>(chordWork_.steps.size())) break;
            if (chordWork_.steps[j] == 0) continue;
            int s = (j < static_cast<int>(chordWork_.sustain_steps.size()))
                ? chordWork_.sustain_steps[j] : 0;
            if (s > 0 && step < j + s) return true;
        }
        return false;
    }
    return false;
}

char PatternEditorEngine::cellChar(int step) const {
    int rel = cursorBar_ * stepsPerBar_ + step;
    switch (kind_) {
        case EditMenu::Rhythm: {
            uint8_t v = rhythmCell(cursorPiece_, rel);
            if (v == 0) return '.';
            return (v >= 100) ? 'X' : 'x';
        }
        case EditMenu::Bass: {
            if (rel < 0 || rel >= static_cast<int>(bassWork_.steps.size())) return '.';
            int d = bassWork_.steps[rel];
            if (d >= 0) {
                static const char* chars = "R3578";
                return (d < 4) ? chars[d] : '?';
            }
            return coveredBySustain(rel) ? '-' : '.';
        }
        case EditMenu::Chord: {
            if (rel < 0 || rel >= static_cast<int>(chordWork_.steps.size())) return '.';
            uint8_t m = chordWork_.steps[rel];
            if (m != 0) return hexDigit(m)[0];
            return coveredBySustain(rel) ? '-' : '.';
        }
        default:
            return '.';
    }
}

std::string PatternEditorEngine::gridLine1() const {
    std::string name;
    if (kind_ == EditMenu::Rhythm) name = kPieces[cursorPiece_].short_name;
    else if (kind_ == EditMenu::Bass) name = "Bass";
    else name = "Chord";
    std::string bar = " B" + std::to_string(cursorBar_ + 1) + "/" +
                      std::to_string(bars_) + " ";
    char cell = cellChar(cursorStep_);
    std::string label = " ";
    if (kind_ == EditMenu::Chord) {
        int rel = cursorRel();
        uint8_t m = (rel >= 0 && rel < static_cast<int>(chordWork_.steps.size()))
            ? chordWork_.steps[rel] : 0;
        if (m == 0) {
            label = coveredBySustain(rel) ? " Sust" : " Rest";
        } else {
            std::string degrees;
            if (m & CHORD_DEGREE_ROOT) degrees.push_back('R');
            if (m & CHORD_DEGREE_THIRD) degrees.push_back('3');
            if (m & CHORD_DEGREE_FIFTH) degrees.push_back('5');
            if (m & CHORD_DEGREE_SEVENTH) degrees.push_back('7');
            label = " " + degrees;
        }
    } else {
        label = " ";
        label.push_back(cell);
    }
    return padTo(name + bar + label, 16);
}

std::string PatternEditorEngine::gridLine2() const {
    std::string out;
    for (int i = 0; i < stepsPerBar_; i++) {
        out.push_back(i == cursorStep_ ? '>' : cellChar(i));
    }
    return out;
}

int PatternEditorEngine::cursorRel() const {
    return cursorBar_ * stepsPerBar_ + cursorStep_;
}

void PatternEditorEngine::moveCursor(int dStep, int dBar) {
    if (dBar != 0) {
        int nb = (cursorBar_ + dBar + bars_) % bars_;
        cursorBar_ = nb;
    }
    if (dStep != 0) {
        cursorStep_ += dStep;
        if (cursorStep_ < 0) {
            cursorStep_ = stepsPerBar_ - 1;
            cursorBar_ = (cursorBar_ - 1 + bars_) % bars_;
        } else if (cursorStep_ >= stepsPerBar_) {
            cursorStep_ = 0;
            cursorBar_ = (cursorBar_ + 1) % bars_;
        }
    }
}

bool PatternEditorEngine::isEnter(const KeyEvent& ev) const {
    if (ev.usage_page == hid_usage::PAGE_KEYBOARD && ev.hid_usage == HID_ENTER) {
        return true;
    }
    KeymapResolver resolver(&state_.keymap);
    return resolver.resolve(ev.hid_usage, ev.modifiers, ev.usage_page).cmd ==
           KeyCmd::Enter;
}

bool PatternEditorEngine::isBack(const KeyEvent& ev) const {
    if (ev.usage_page != hid_usage::PAGE_KEYBOARD) return false;
    return ev.hid_usage == HID_ESC || ev.hid_usage == HID_BACKSPACE;
}

bool PatternEditorEngine::keyIsChord(const KeyEvent& ev) const {
    KeymapResolver resolver(&state_.keymap);
    KeyCmd cmd = resolver.resolve(ev.hid_usage, ev.modifiers, ev.usage_page).cmd;
    return cmd == KeyCmd::ChordKey || cmd == KeyCmd::Backtick;
}

bool PatternEditorEngine::keyIsParameterAction(const KeyEvent& ev) const {
    KeymapResolver resolver(&state_.keymap);
    switch (resolver.resolve(ev.hid_usage, ev.modifiers, ev.usage_page).cmd) {
        case KeyCmd::CycleParam:
        case KeyCmd::SetParam:
        case KeyCmd::IncParam:
        case KeyCmd::DecParam:
        case KeyCmd::ToggleParam:
        case KeyCmd::DrumMute:
        case KeyCmd::TapTempo:
            return true;
        default:
            return false;
    }
}

bool PatternEditorEngine::keyIsIgnoredParam(const KeyEvent& ev) const {
    if (kind_ != EditMenu::Rhythm && kind_ != EditMenu::Bass) return false;

    KeymapResolver resolver(&state_.keymap);
    KeyAction a = resolver.resolve(ev.hid_usage, ev.modifiers, ev.usage_page);
    switch (a.cmd) {
        case KeyCmd::CycleParam:
        case KeyCmd::SetParam:
        case KeyCmd::IncParam:
        case KeyCmd::DecParam:
        case KeyCmd::ToggleParam:
            break;
        default:
            return false;
    }

    if (kind_ == EditMenu::Rhythm) return a.param == ParamId::RhythmMute;
    return a.param == ParamId::BassEnable;
}

bool PatternEditorEngine::keyIsExt(const KeyEvent& ev) const {
    KeymapResolver resolver(&state_.keymap);
    switch (resolver.resolve(ev.hid_usage, ev.modifiers, ev.usage_page).cmd) {
        case KeyCmd::Ext9:
        case KeyCmd::Ext11:
        case KeyCmd::Ext13:
            return true;
        default:
            return false;
    }
}

int PatternEditorEngine::padIndex(const KeyEvent& ev) const {
    if (ev.usage_page != hid_usage::PAGE_KEYBOARD) return -1;
    switch (ev.hid_usage) {
        case HID_KP1: return 0;
        case HID_KP2: return 1;
        case HID_KP3: return 2;
        case HID_KP4: return 3;
        case HID_KP5: return 4;
        case HID_KP6: return 5;
        case HID_KP7: return 6;
        case HID_KP8: return 7;
        case HID_KP9: return 8;
        case HID_KP0: return 9;
        case HID_KP_DOT: return 10;
        case HID_KP_SLASH: return 11;
        case HID_KP_STAR: return 12;
        default: return -1;
    }
}

bool PatternEditorEngine::keyIsPlainPad(const KeyEvent& ev) const {
    return ev.modifiers == 0 && padIndex(ev) >= 0;
}

void PatternEditorEngine::handleLivePad(const KeyEvent& ev, uint64_t now_us) {
    int p = padIndex(ev);
    if (p < 0) return;

    if (kind_ == EditMenu::Rhythm) {
        if (ev.pressed) {
            if (recording_) recordRhythmPad(p, now_us);
            auditionRhythmPiece(p);
        }
        return;
    }

    if (kind_ == EditMenu::Chord) {
        if (ev.hid_usage >= HID_KP1 && ev.hid_usage <= HID_KP4) {
            int bit = static_cast<int>(ev.hid_usage - HID_KP1);
            uint8_t bitMask = static_cast<uint8_t>(1u << bit);
            if (ev.pressed) {
                chordLiveMask_ |= bitMask;
                if (recording_) recordChordPad(bit, now_us);
            } else {
                chordLiveMask_ &= static_cast<uint8_t>(~bitMask);
                if (recording_) releaseChordPad(bit, now_us);
            }
            // Diff-only: held degrees keep sounding without retriggering.
            updateChordLiveAudition(chordLiveMask_, now_us);
            return;
        }

        if (ev.hid_usage == HID_KP0 && ev.pressed) {
            chordLiveMask_ = 0;
            releaseChordLiveAudition();
            if (recording_) {
                int rr = 0;
                uint32_t aa = 0;
                if (recordStepFor(now_us, rr, aa)) clearChordStep(now_us, rr);
            }
        }
        return;
    }

    if (kind_ != EditMenu::Bass) return;

    int degree = -1;
    bool isBassPad = false;
    if (ev.hid_usage >= HID_KP1 && ev.hid_usage <= HID_KP5) {
        degree = static_cast<int>(ev.hid_usage - HID_KP1);
        isBassPad = true;
    } else if (ev.hid_usage == HID_KP0) {
        degree = -1;
        isBassPad = true;
    }
    if (!isBassPad) return;

    if (ev.pressed) {
        if (recording_) {
            recordBassPad(degree, now_us);
            bassPadUsage_ = ev.hid_usage;
        }
        if (degree >= 0) auditionBassDegree(degree, now_us);
        else releaseAudition();
    } else if (recording_ && ev.hid_usage == bassPadUsage_) {
        releaseBassPad(now_us);
    }
}

void PatternEditorEngine::markUiRestore(uint64_t now_us) {
    statusActive_ = false;
    uiRestoreActive_ = true;
    uiRestoreUntilUs_ = now_us +
        static_cast<uint64_t>(state_.config.display_revert_ms) * 1000ULL;
}

std::string PatternEditorEngine::padTo(const std::string& s, size_t n) {
    return ::padTo(s, n);
}

// --- lifecycle -------------------------------------------------------------

void PatternEditorEngine::open(EditMenu kind, uint64_t now_us) {
    if (active_) return;
    active_ = true;
    state_.patternEditorActive.store(true);
    state_.cursorActive = false;
    state_.editMenu = EditMenu::None;
    state_.editParam = 0;
    kind_ = normalizedEditorKind(kind);
    command_ = 0;
    valueSelected_ = false;
    uiRestoreActive_ = false;
    uiRestoreUntilUs_ = 0;
    prompt_ = Prompt::None;
    pendingAction_ = PendingAction::None;
    nameEditing_ = false;
    newFlow_ = NewFlow::None;
    chordLiveMask_ = 0;
    chordLiveSoundingMask_ = 0;
    for (uint64_t& d : chordLiveOffUs_) d = 0;
    rhythmMuteOverridden_ = false;
    screen_ = Screen::Menu;

    // Pattern editors always emit. Suppress the preset mute/disable settings for
    // this session and restore them when the editor closes.
    if (kind_ == EditMenu::Rhythm) {
        savedRhythmMuteOverride_ = state_.pendingRhythm.muted;
        rhythmMuteOverridden_ = true;
        state_.pendingRhythm.muted = false;
        if (modified_) modified_();
    } else if (kind_ == EditMenu::Bass) {
        state_.pendingBass.enabled = true;
        if (modified_) modified_();
    }

    loadLibraries();
    loadCurrentPresetPattern();
    (void)now_us;
}

void PatternEditorEngine::close(uint64_t now_us) {
    if (!active_) return;
    stopAll(now_us);
    restoreRhythm(now_us);
    if (rhythmMuteOverridden_) {
        state_.pendingRhythm.muted = savedRhythmMuteOverride_;
        rhythmMuteOverridden_ = false;
        if (modified_) modified_();
    }
    // The bass editor forces BassEnable on for the session and leaves it on after
    // exit, so the edited pattern is immediately audible through the normal bass
    // engine. (The rhythm editor still restores its mute state.)
    active_ = false;
    state_.patternEditorActive.store(false);
    screen_ = Screen::Off;
    display_.cancel();
}

void PatternEditorEngine::stopAll(uint64_t now_us) {
    if (recording_ || armed_) {
        stopRecording(now_us, nullptr);
    }
    if (previewing_) stopPreview(now_us, false);
    releasePreviewNotes();
    releaseAudition();
    releaseChordLiveAudition();
    bassPadActive_ = false;
    for (bool& b : chordPadActive_) b = false;
    chordLiveMask_ = 0;
    armLastStepAbs_ = 0;
    armStartedUs_ = 0;
    armLastProgressUs_ = 0;
    state_.recordGuideActive.store(false);
    state_.recordGuideHatsActive.store(false);
    state_.recordGuideHatsStartAbs.store(0);
    valueSelected_ = false;
    uiRestoreActive_ = false;
    uiRestoreUntilUs_ = 0;
    prompt_ = Prompt::None;
    pendingAction_ = PendingAction::None;
    nameEditing_ = false;
    newFlow_ = NewFlow::None;
    restoreRhythm(now_us);
    if (active_) renderMenu();
}

// --- key handling ----------------------------------------------------------

bool PatternEditorEngine::handleKeyEvent(const KeyEvent& ev, uint64_t now_us) {
    if (!active_) return false;

    if (nameEditing_) return handleNameEdit(ev, now_us);

    if (prompt_ != Prompt::None) {
        if (ev.pressed) {
            if (isEnter(ev)) confirmPrompt(now_us);
            else cancelPrompt(now_us);
        }
        return true;
    }

    // The rhythm/bass pattern editors always emit: ignore the matching global
    // mute/disable control so it cannot suppress the session.
    if (keyIsIgnoredParam(ev)) return true;

    // Plain numpad is live in the bass/rhythm editors: bass degrees and drums
    // use the current preset parameters. In the chord editor the pads audition
    // held degrees except in the grid, where they retain their bit-toggle role.
    bool chordLivePad = kind_ == EditMenu::Chord && screen_ != Screen::Grid;
    if ((kind_ == EditMenu::Bass || kind_ == EditMenu::Rhythm ||
         chordLivePad) && keyIsPlainPad(ev)) {
        handleLivePad(ev, now_us);
        return true;
    }

    // Chord-grid keys stay live. In preview, a live chord takes over.
    if (keyIsChord(ev)) {
        if (previewing_ && ev.pressed) stopPreview(now_us, false);
        return false;
    }

    // Direct parameter edits (including F1-F8 and Ctrl/Alt combos) are passed
    // through to EditEngine in every editor state.
    if (keyIsParameterAction(ev)) {
        if (ev.pressed) markUiRestore(now_us);
        return false;
    }

    // Held chord extensions are playable while recording/previewing; in the
    // menu and grid the arrow keys are navigation/edit keys.
    if (keyIsExt(ev) && (recording_ || armed_ || previewing_)) {
        return false;
    }

    // Volume adjusts tempo while recording/previewing; in the menus/grid it
    // navigates the editor.
    bool volumeKey = ev.usage_page == hid_usage::PAGE_CONSUMER &&
        (ev.hid_usage == hid_usage::CONSUMER_VOLUME_UP ||
         ev.hid_usage == hid_usage::CONSUMER_VOLUME_DOWN);
    if (volumeKey && (recording_ || armed_ || previewing_)) {
        if (ev.pressed) markUiRestore(now_us);
        return false;
    }

    if (recording_ || armed_) {
        if (ev.pressed) {
            if (isEnter(ev) || isBack(ev)) {
                stopRecording(now_us, "Recorded");
                screen_ = Screen::Menu;
                renderMenu();
                return true;
            }
        }
        return true;
    }

    if (previewing_) {
        if (ev.pressed && (isEnter(ev) || isBack(ev))) {
            stopPreview(now_us, true);
            return true;
        }
        return true;
    }

    if (screen_ == Screen::Grid) {
        if (!ev.pressed) return true;
        if (isBack(ev)) {
            screen_ = Screen::Menu;
            renderMenu();
            return true;
        }
        if (ev.hid_usage == HID_LEFT) { moveCursor(-1, 0); renderGrid(); return true; }
        if (ev.hid_usage == HID_RIGHT) { moveCursor(+1, 0); renderGrid(); return true; }
        if (ev.hid_usage == HID_UP) {
            if (kind_ == EditMenu::Rhythm) {
                cursorPiece_ = (cursorPiece_ + 12) % 13;
            } else {
                moveCursor(0, -1);
            }
            renderGrid();
            return true;
        }
        if (ev.hid_usage == HID_DOWN) {
            if (kind_ == EditMenu::Rhythm) {
                cursorPiece_ = (cursorPiece_ + 1) % 13;
            } else {
                moveCursor(0, +1);
            }
            renderGrid();
            return true;
        }
        if (isEnter(ev)) {
            if (kind_ == EditMenu::Rhythm) {
                editRhythmCell();
            } else if (kind_ == EditMenu::Bass) {
                int rel = cursorRel();
                if (rel >= 0 && rel < static_cast<int>(bassWork_.steps.size())) {
                    int d = bassWork_.steps[rel];
                    d = (d + 2) % 6 - 1;  // -1,0,1,2,3,4 cycle
                    bassWork_.steps[rel] = static_cast<int8_t>(d);
                    if (rel < static_cast<int>(bassWork_.sustain_steps.size())) {
                        bassWork_.sustain_steps[rel] = 0;
                    }
                    dirty_ = true;
                }
            } else {
                moveCursor(+1, 0);
            }
            renderGrid();
            return true;
        }
        if (kind_ == EditMenu::Chord && keyIsPlainPad(ev)) {
            int rel = cursorRel();
            if (ev.hid_usage >= HID_KP1 && ev.hid_usage <= HID_KP4) {
                int bit = static_cast<int>(ev.hid_usage - HID_KP1);
                if (rel >= 0 && rel < static_cast<int>(chordWork_.steps.size())) {
                    chordWork_.steps[rel] ^= static_cast<uint8_t>(1u << bit);
                    if (chordWork_.steps[rel] == 0 &&
                        rel < static_cast<int>(chordWork_.sustain_steps.size())) {
                        chordWork_.sustain_steps[rel] = 0;
                    }
                    dirty_ = true;
                }
            } else if (ev.hid_usage == HID_KP0) {
                clearChordStep(now_us, rel);
            }
            renderGrid();
            return true;
        }
        return true;
    }

    if (screen_ == Screen::Menu) {
        if (!ev.pressed) return true;

        if (valueSelected_) {
            if (isEnter(ev)) {
                commitSelectedValue(now_us);
                return true;
            }
            if (isBack(ev)) {
                cancelSelectedValue(now_us);
                return true;
            }
            int dir = navDirection(ev);
            if (dir != 0) {
                adjustSelectedValue(dir, now_us);
            }
            return true;
        }

        if (isEnter(ev)) {
            executeCommand(static_cast<Command>(command_), now_us);
            return true;
        }
        if (isBack(ev)) {
            requestDiscard(PendingAction::Exit, kind_, 0, now_us);
            return true;
        }
        int dir = navDirection(ev);
        if (dir != 0) {
            command_ = (command_ + dir + static_cast<int>(Command::COUNT)) %
                       static_cast<int>(Command::COUNT);
            renderMenu();
            return true;
        }
        return true;
    }

    return true;
}

void PatternEditorEngine::update(uint64_t now_us) {
    if (!active_) return;

    if (prompt_ != Prompt::None && now_us >= promptDeadlineUs_) {
        cancelPrompt(now_us);
    }
    if (statusActive_ && !nameEditing_ && prompt_ == Prompt::None &&
        now_us >= statusUntilUs_) {
        statusActive_ = false;
        if (valueSelected_) showSelectedValue(now_us);
        else render();
    }
    if (uiRestoreActive_ && !nameEditing_ && prompt_ == Prompt::None &&
        now_us >= uiRestoreUntilUs_) {
        uiRestoreActive_ = false;
        if (valueSelected_) showSelectedValue(now_us);
        else render();
    }
    if (!state_.pendingRhythm.enabled) {
        if (armed_ || recording_) {
            stopRecording(now_us, "Rhythm off");
        } else if (previewing_) {
            stopPreview(now_us, false);
            showStatus(editorTitle(), "Rhythm off", now_us);
        }
    }
    expireChordLiveAudition(now_us);
    if (!auditionNotes_.empty() && now_us >= auditionOffDeadlineUs_) {
        releaseAudition();
    }
    if (!previewNotes_.empty() && now_us >= previewOffDeadlineUs_) {
        releasePreviewNotes();
    }
    if (armed_ || recording_) updateRecording(now_us);
    if (previewing_) updatePreview(now_us);
}
