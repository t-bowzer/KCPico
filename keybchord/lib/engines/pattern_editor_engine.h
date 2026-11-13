#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "bass.h"
#include "base.h"
#include "chordpattern.h"
#include "params.h"
#include "rhythm.h"
#include "state.h"

class BassEngine;
class ChordEngine;
class DisplayManager;
class MidiRouter;
class RhythmEngine;
class StorageAdapter;


// On-device bass/rhythm/chord pattern editor (M17).
//
// The editor owns a working copy of one pattern. Recording is an overdub into
// the rhythm 16th grid and reuses the M14 hi-hat count-in. Preview/audition are
// sent directly by the editor while it suppresses the normal pattern engines.
// Saves are explicit; New/Save As create userN.json files and built-ins may be
// overwritten only after confirmation.
class PatternEditorEngine {
public:
    PatternEditorEngine(StateManager& state, StorageAdapter& storage,
                        DisplayManager& display, MidiRouter& router,
                        RhythmEngine& rhythm, BassEngine& bass,
                        ChordEngine& chord);

    // Called after the working pattern changes the preset's pattern selection.
    void setModifiedCallback(std::function<void()> cb);
    // Returns false while the MSC drive is active (editor write actions off).
    void setWriteAllowedCallback(std::function<bool()> cb);

    bool active() const { return active_; }
    bool nameEditing() const { return nameEditing_; }
    bool recording() const { return recording_ || armed_; }
    bool previewing() const { return previewing_; }

    // Open the editor for chord/rhythm/bass. Accepts either the base menu enum
    // or the matching *Pattern menu enum. The caller should stop the sequencer
    // first.
    void open(EditMenu kind, uint64_t now_us);
    void close(uint64_t now_us);

    // Stop recording/preview/audition but keep the editor open (panic/hotplug).
    void stopAll(uint64_t now_us);

    // Returns true when the event was consumed. Chord-grid keys return false so
    // the chord engine can keep playing while the editor is open.
    bool handleKeyEvent(const KeyEvent& ev, uint64_t now_us);
    void update(uint64_t now_us);

private:
    enum class Screen : uint8_t { Off, Menu, Grid, Recording, Preview };
    enum class Prompt : uint8_t { None, Discard, OverwriteBuiltin, Delete, Clear, NewRecord };
    enum class PendingAction : uint8_t { None, Exit, New, SwitchPattern };
    enum class NewFlow : uint8_t { None, Name, Bars, RecordPrompt };
    enum class Command : uint8_t {
        Pattern = 0, New, Bars, Quantize, Grid, Record, Preview,
        Rename, Save, SaveAs, Delete, Clear, COUNT
    };

    StateManager& state_;
    StorageAdapter& storage_;
    DisplayManager& display_;
    MidiRouter& router_;
    RhythmEngine& rhythm_;
    BassEngine& bass_;
    ChordEngine& chord_;
    std::function<void()> modified_;
    std::function<bool()> writeAllowed_;

    bool active_ = false;
    bool nameEditing_ = false;
    std::string nameBuffer_;

    EditMenu kind_ = EditMenu::Rhythm;
    int patternIndex_ = 0;
    bool builtin_ = false;
    bool newUnsaved_ = false;
    bool dirty_ = false;
    std::string fileName_;
    NewFlow newFlow_ = NewFlow::None;
    int newFileNumber_ = 1;

    Screen screen_ = Screen::Off;
    int command_ = 0;
    Prompt prompt_ = Prompt::None;
    uint64_t promptDeadlineUs_ = 0;
    PendingAction pendingAction_ = PendingAction::None;
    int pendingPattern_ = 0;
    bool valueSelected_ = false;
    Command selectedCommand_ = Command::Pattern;
    int pendingValue_ = 0;

    uint8_t bars_ = 1;
    uint8_t stepsPerBar_ = 16;
    uint8_t quantize_ = 16;  // 4, 8, or 16 (steps per quantize division)
    int cursorBar_ = 0;
    int cursorStep_ = 0;
    int cursorPiece_ = 0;

    // Local copies used for editing. The engines own their playback copies; the
    // rhythm one is staged to Core 1 after a save.
    std::vector<RhythmPattern> rhythmPatterns_;
    std::vector<BassPattern> bassPatterns_;
    std::vector<ChordPattern> chordPatterns_;

    RhythmPattern rhythmWork_;
    BassPattern bassWork_;
    ChordPattern chordWork_;

    bool armed_ = false;
    bool recording_ = false;
    bool countInPlanned_ = false;
    uint32_t countInStartAbs_ = 0;
    uint32_t recordStartAbs_ = 0;
    uint8_t lastRecordBar_ = 0;
    uint32_t armLastStepAbs_ = 0;
    uint64_t armStartedUs_ = 0;
    uint64_t armLastProgressUs_ = 0;

    bool previewing_ = false;
    bool previewFirstPending_ = false;
    uint32_t previewStartAbs_ = 0;
    uint32_t lastPreviewAbs_ = 0;

    bool rhythmStateSaved_ = false;
    bool savedRhythmEnabled_ = false;
    bool savedRhythmMuted_ = false;
    bool lastRhythmEnabled_ = false;
    bool lastRhythmMuted_ = false;

    bool rhythmMuteOverridden_ = false;
    bool savedRhythmMuteOverride_ = false;

    std::vector<uint8_t> previewNotes_;
    uint8_t previewChannel_ = 1;
    uint64_t previewOffDeadlineUs_ = 0;

    std::vector<uint8_t> auditionNotes_;
    uint8_t auditionChannel_ = 1;
    uint64_t auditionOffDeadlineUs_ = 0;

    bool bassPadActive_ = false;
    int bassPadDegree_ = -1;
    uint16_t bassPadUsage_ = 0;
    uint32_t bassPadAbs_ = 0;
    uint16_t bassPadRel_ = 0;

    bool chordPadActive_[4] = {false, false, false, false};
    uint32_t chordPadAbs_[4] = {0, 0, 0, 0};
    uint16_t chordPadRel_[4] = {0, 0, 0, 0};
    uint8_t chordLiveMask_ = 0;
    uint8_t chordLiveSoundingMask_ = 0;
    uint8_t chordLiveNotes_[4] = {0, 0, 0, 0};
    uint64_t chordLiveOffUs_[4] = {0, 0, 0, 0};

    uint64_t statusUntilUs_ = 0;
    bool statusActive_ = false;
    uint64_t uiRestoreUntilUs_ = 0;
    bool uiRestoreActive_ = false;
    std::string recordLine1_;
    std::string recordLine2_;

    // --- pattern bookkeeping ---
    int loopSteps() const;
    uint8_t liveStepsPerBar() const;
    bool meterMatchesLive() const;
    int patternCount(EditMenu kind) const;
    const char* patternName(EditMenu kind, int index) const;
    bool isBuiltinFile(EditMenu kind, const std::string& file) const;
    std::string builtinFileName(EditMenu kind, int index) const;
    int builtinCount(EditMenu kind) const;
    const char* patternDir(EditMenu kind) const;
    std::string fileForIndex(EditMenu kind, int index) const;
    std::string uniqueUserFileName(EditMenu kind) const;
    std::string uniquePatternName(EditMenu kind, const std::string& base,
                                  const std::string& current) const;
    void loadLibraries();
    void loadPattern(EditMenu kind, int index);
    void loadCurrentPresetPattern();
    void normalizeWorking();
    void resizeWorking();
    void clearWorking();
    std::string serializeWorking() const;
    void reloadLibrary(EditMenu kind);
    void selectSavedPattern(const std::string& name);
    std::string workingName() const;
    void setWorkingName(const std::string& name);

    // --- commands/screens ---
    const char* editorTitle() const;
    const char* commandName(Command cmd) const;
    std::string commandLabel(Command cmd) const;
    std::string commandFullName(Command cmd) const;
    bool isValueCommand(Command cmd) const;
    int commandValue(Command cmd) const;
    std::string selectedValueString() const;
    void beginValueSelection(Command cmd, uint64_t now_us);
    void adjustSelectedValue(int dir, uint64_t now_us);
    void commitSelectedValue(uint64_t now_us);
    void cancelSelectedValue(uint64_t now_us);
    void showSelectedValue(uint64_t now_us);
    int navDirection(const KeyEvent& ev) const;
    void renderMenu();
    void renderGrid();
    void renderRecording();
    void renderPreview();
    void render();
    void showForcedPersistent(const std::string& line1,
                              const std::string& line2);
    void showStatus(const std::string& line1, const std::string& line2,
                    uint64_t now_us);
    void beginPrompt(Prompt prompt, const std::string& l1,
                     const std::string& l2, uint64_t now_us);
    void cancelPrompt(uint64_t now_us);
    void confirmPrompt(uint64_t now_us);
    void requestDiscard(PendingAction action, EditMenu kind, int pattern,
                        uint64_t now_us);
    void performPending(uint64_t now_us);
    void executeCommand(Command cmd, uint64_t now_us);

    // --- record/preview ---
    void ensureRhythm(uint64_t now_us);
    void restoreRhythm(uint64_t now_us);
    void startRecordArm(uint64_t now_us);
    void planCountIn(uint32_t stepAbs);
    void startRecording(uint64_t now_us);
    void stopRecording(uint64_t now_us, const char* message);
    void updateRecording(uint64_t now_us);
    void startPreview(uint64_t now_us);
    void stopPreview(uint64_t now_us, bool showStatus);
    void updatePreview(uint64_t now_us);
    void processPreviewStep(int rel, uint64_t now_us);
    bool recordStepFor(uint64_t now_us, int& rel, uint32_t& abs) const;
    void recordRhythmPad(int piece, uint64_t now_us);
    void recordBassPad(int degree, uint64_t now_us);
    void recordChordPad(int bit, uint64_t now_us);
    void releaseBassPad(uint64_t now_us);
    void releaseChordPad(int bit, uint64_t now_us);
    void clearChordStep(uint64_t now_us, int rel);

    // --- midi ---
    void sendAudition(const std::vector<uint8_t>& notes, uint8_t channel,
                      uint8_t velocity, uint64_t now_us, uint32_t durationMs);
    void releaseAudition();
    void releasePreviewNotes();
    void auditionRhythmPiece(int piece);
    void auditionBassDegree(int degree, uint64_t now_us);
    void updateChordLiveAudition(uint8_t mask, uint64_t now_us);
    void releaseChordLiveAudition();
    void expireChordLiveAudition(uint64_t now_us);
    bool chordForPreview(ResolvedChord& out) const;

    // --- save/delete/name ---
    void saveCurrent(bool saveAs, uint64_t now_us);
    bool doSave(uint64_t now_us);
    void doNew(uint64_t now_us);
    void doDelete(uint64_t now_us);
    void doClear(uint64_t now_us);
    void beginRename(uint64_t now_us);
    void commitName(uint64_t now_us);
    void cancelNameEdit(uint64_t now_us);
    bool handleNameEdit(const KeyEvent& ev, uint64_t now_us);

    // --- helpers ---
    int rhythmTrackIndex(int piece) const;
    int ensureRhythmTrack(int piece);
    uint8_t rhythmCell(int piece, int step) const;
    void editRhythmCell();
    bool coveredBySustain(int step) const;
    char cellChar(int step) const;
    std::string gridLine1() const;
    std::string gridLine2() const;
    int cursorRel() const;
    void moveCursor(int dStep, int dBar);
    bool isEnter(const KeyEvent& ev) const;
    bool isBack(const KeyEvent& ev) const;
    bool keyIsChord(const KeyEvent& ev) const;
    bool keyIsParameterAction(const KeyEvent& ev) const;
    bool keyIsIgnoredParam(const KeyEvent& ev) const;
    bool keyIsExt(const KeyEvent& ev) const;
    bool keyIsPlainPad(const KeyEvent& ev) const;
    void handleLivePad(const KeyEvent& ev, uint64_t now_us);
    void markUiRestore(uint64_t now_us);
    int padIndex(const KeyEvent& ev) const;
    static std::string padTo(const std::string& s, size_t n);
};
