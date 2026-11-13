#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "adapters_null.h"
#include "bass_engine.h"
#include "chord_engine.h"
#include "display_manager.h"
#include "edit_engine.h"
#include "midi_event_queue.h"
#include "midi_router.h"
#include "pattern_editor_engine.h"
#include "recording_midi_out.h"
#include "rhythm_engine.h"
#include "state.h"
#include "storage_stub.h"

namespace {

constexpr uint16_t HID_ENTER = 0x28;
constexpr uint16_t HID_F1 = 0x3A;
constexpr uint16_t HID_F2 = 0x3B;
constexpr uint16_t HID_F3 = 0x3C;
constexpr uint16_t HID_F4 = 0x3D;
constexpr uint16_t HID_F6 = 0x3F;
constexpr uint16_t HID_F7 = 0x40;
constexpr uint16_t HID_ESC = 0x29;
constexpr uint16_t HID_BACKSPACE = 0x2A;
constexpr uint16_t HID_RIGHT = 0x4F;
constexpr uint16_t HID_UP = 0x52;
constexpr uint16_t HID_DOWN = 0x51;
constexpr uint16_t HID_KP1 = 0x59;
constexpr uint16_t HID_KP2 = 0x5A;
constexpr uint16_t HID_KP4 = 0x5C;
constexpr uint16_t HID_KP5 = 0x5D;
constexpr uint64_t STEP_US = 125000;  // 120 BPM, 16th note

constexpr int CMD_PATTERN = 0;
constexpr int CMD_NEW = 1;
constexpr int CMD_BARS = 2;
constexpr int CMD_GRID = 4;
constexpr int CMD_RECORD = 5;
constexpr int CMD_PREVIEW = 6;
constexpr int CMD_RENAME = 7;
constexpr int CMD_SAVE = 8;
constexpr int CMD_DELETE = 10;

class RecordingLcd : public LcdAdapter {
public:
    bool begin() override { return true; }
    void write(const std::string& l1, const std::string& l2) override {
        line1_ = l1;
        line2_ = l2;
    }
    void clear() override {}
    const std::string& line1() const { return line1_; }
    const std::string& line2() const { return line2_; }

private:
    std::string line1_;
    std::string line2_;
};

} // namespace

class PatternEditorEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        state_.loadDefaults();
        state_.pendingRhythm.enabled = false;  // the editor should enable it
        state_.pendingRhythm.muted = false;
        state_.pendingRhythm.tempo = 120;
        state_.pendingRhythm.pattern = 0;
        state_.pendingBass.pattern = 0;
        state_.pendingChord.pattern = 0;
        state_.pendingBass.channel = 3;
        state_.pendingChord.channel = 1;
        state_.rhythmClock.running.store(true);
        state_.selectedChord = ResolvedChord{0, ChordType::Major};
        state_.selectedChordValid = true;
        setStep(0);

        midi_.begin();
        router_ = new MidiRouter(midi_, state_);

        RhythmLibrary r = loadRhythmPatterns(storage_);
        installRhythmNames(r.names);
        installRhythmShortNames(r.short_names);
        rhythm_ = new RhythmEngine(state_, queue_);
        rhythm_->setPatterns(std::move(r.patterns));

        BassLibrary b = loadBassPatterns(storage_);
        installBassNames(b.names);
        bass_ = new BassEngine(state_, *router_);
        bass_->setPatterns(std::move(b.patterns));

        ChordPatternLibrary c = loadChordPatterns(storage_);
        installChordPatternNames(c.names);
        chord_ = new ChordEngine(state_, *router_);
        chord_->setPatterns(std::move(c.patterns));
        bass_->setChordEngine(chord_);

        display_ = new DisplayManager(state_, lcd_);
        edit_ = new EditEngine(state_, *display_);
        editor_ = new PatternEditorEngine(
            state_, storage_, *display_, *router_, *rhythm_, *bass_, *chord_);
        editor_->setModifiedCallback([this]() { modified_++; });
        editor_->setWriteAllowedCallback([]() { return true; });
    }

    void TearDown() override {
        delete editor_;
        delete edit_;
        delete display_;
        delete chord_;
        delete bass_;
        delete rhythm_;
        delete router_;
        clearRhythmNames();
        clearRhythmShortNames();
        clearBassNames();
        clearChordPatternNames();
    }

    void setStep(uint32_t abs) {
        currentAbs_ = abs;
        state_.rhythmClock.stepAbs.store(abs);
        state_.rhythmClock.step.store(abs % state_.rhythmClock.stepsPerBar.load());
        state_.rhythmClock.stepsPerBar.store(16);
        state_.rhythmClock.nextStepUs.store(
            static_cast<uint64_t>(abs + 1) * STEP_US);
    }

    uint64_t atStep(uint32_t abs) const {
        return static_cast<uint64_t>(abs) * STEP_US + 1000;
    }

    KeyEvent key(uint16_t usage, bool pressed = true) {
        return {usage, pressed, 0};
    }

    void press(uint16_t usage, bool pressed = true) {
        editor_->handleKeyEvent(key(usage, pressed), atStep(currentAbs_));
    }

    void pressConsumer(uint16_t usage) {
        KeyEvent ev{usage, true, 0, 0, hid_usage::PAGE_CONSUMER};
        editor_->handleKeyEvent(ev, atStep(currentAbs_));
    }

    void gotoCommand(int target) {
        int count = 12;
        int delta = (target - cmd_ + count) % count;
        for (int i = 0; i < delta; i++) press(HID_DOWN);
        cmd_ = target;
    }

    void startRecord() {
        gotoCommand(CMD_RECORD);
        press(HID_ENTER);
    }

    void startPreview() {
        gotoCommand(CMD_PREVIEW);
        press(HID_ENTER);
    }

    void enterGrid() {
        gotoCommand(CMD_GRID);
        press(HID_ENTER);
    }

    void saveFromMenu() {
        gotoCommand(CMD_SAVE);
        press(HID_ENTER);
    }

    bool routeMain(const KeyEvent& ev, uint64_t now) {
        if (editor_->handleKeyEvent(ev, now)) return true;
        if (edit_->handleKeyEvent(ev, now)) return true;
        chord_->handleKeyEvent(ev, now);
        return false;
    }

    bool hasNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) const {
        for (const auto& m : midi_.messages()) {
            if ((m.status & 0xF0) == 0x90 &&
                (m.status & 0x0F) == (channel - 1) &&
                m.data1 == note && m.data2 == velocity) {
                return true;
            }
        }
        return false;
    }

    void advanceTo(uint32_t abs) {
        setStep(abs);
        editor_->update(atStep(abs));
    }

    void armRecord() {
        startRecord();
        advanceTo(8);
        advanceTo(16);  // count-in bar 0..16; recording starts at 16
    }

    void newPattern() {
        gotoCommand(CMD_NEW);
        press(HID_ENTER);  // New -> name prompt
        press(HID_ENTER);  // commit default name -> Bars
        press(HID_ENTER);  // commit default bars -> Record now?
        press(HID_ESC);    // decline immediate recording
    }

    void deletePattern() {
        gotoCommand(CMD_DELETE);
        press(HID_ENTER); // prompt
        press(HID_ENTER); // confirm
    }

    StateManager state_;
    StorageStub storage_;
    RecordingLcd lcd_;
    RecordingMidiOutAdapter midi_;
    MidiEventQueue queue_;
    MidiRouter* router_ = nullptr;
    RhythmEngine* rhythm_ = nullptr;
    BassEngine* bass_ = nullptr;
    ChordEngine* chord_ = nullptr;
    DisplayManager* display_ = nullptr;
    EditEngine* edit_ = nullptr;
    PatternEditorEngine* editor_ = nullptr;
    uint32_t currentAbs_ = 0;
    int modified_ = 0;
    int cmd_ = 0;
};

TEST_F(PatternEditorEngineTest, OpenAndCloseOwnsPatternEngines) {
    editor_->open(EditMenu::Rhythm, 0);
    EXPECT_TRUE(editor_->active());
    EXPECT_TRUE(state_.patternEditorActive.load());
    editor_->close(0);
    EXPECT_FALSE(editor_->active());
    EXPECT_FALSE(state_.patternEditorActive.load());
}

TEST_F(PatternEditorEngineTest, ProductionPatternMenuKindsMapToEditors) {
    editor_->open(EditMenu::RhythmPattern, 0);
    display_->update(0);
    EXPECT_NE(lcd_.line1().find("Rhythm Pattern"), std::string::npos);
    state_.pendingRhythm.channel = 10;
    state_.pendingRhythm.drums.kick = 40;
    state_.pendingRhythm.drums.kick_vel = 77;
    midi_.clear();
    press(HID_KP1);
    EXPECT_TRUE(hasNoteOn(10, 40, 77));
    editor_->close(0);

    editor_->open(EditMenu::BassPattern, 0);
    display_->update(0);
    EXPECT_NE(lcd_.line1().find("Bass Pattern"), std::string::npos);
    editor_->close(0);

    editor_->open(EditMenu::ChordPattern, 0);
    display_->update(0);
    EXPECT_NE(lcd_.line1().find("Chord Pattern"), std::string::npos);
    editor_->close(0);
}

TEST_F(PatternEditorEngineTest, NewIsSecondCommand) {
    editor_->open(EditMenu::RhythmPattern, 0);
    display_->update(0);
    EXPECT_NE(lcd_.line2().find("Pat:"), std::string::npos);

    press(HID_DOWN);
    display_->update(0);
    EXPECT_EQ(lcd_.line2().find("New"), 0u);

    press(HID_DOWN);
    display_->update(0);
    EXPECT_EQ(lcd_.line2().find("Bars:"), 0u);
}

TEST_F(PatternEditorEngineTest, NewShowsCreatedConfirmation) {
    editor_->open(EditMenu::RhythmPattern, 0);
    newPattern();
    display_->update(0);
    EXPECT_NE(lcd_.line1().find("New Pat 1"), std::string::npos);
    EXPECT_NE(lcd_.line2().find("Created"), std::string::npos);

    saveFromMenu();
    newPattern();
    display_->update(0);
    EXPECT_NE(lcd_.line1().find("New Pat 2"), std::string::npos);
}

TEST_F(PatternEditorEngineTest, NewFlowPromptsNameBarsAndRecordChoice) {
    editor_->open(EditMenu::RhythmPattern, 0);
    gotoCommand(CMD_NEW);
    press(HID_ENTER);  // start New -> name editor

    display_->update(0);
    EXPECT_NE(lcd_.line2().find("New Rhythm_"), std::string::npos);

    press(HID_ENTER);  // commit default name -> Bars selection
    display_->update(0);
    EXPECT_NE(lcd_.line1().find("Bars"), std::string::npos);
    EXPECT_NE(lcd_.line2().find("1"), std::string::npos);

    press(HID_DOWN);   // choose 2 bars
    press(HID_ENTER);  // commit bars -> Record now?
    display_->update(0);
    EXPECT_NE(lcd_.line1().find("Record now?"), std::string::npos);
    EXPECT_NE(lcd_.line2().find("Ent/Bk"), std::string::npos);

    press(HID_ESC);    // decline immediate recording
    display_->update(0);
    EXPECT_NE(lcd_.line1().find("New Pat 1"), std::string::npos);
    EXPECT_NE(lcd_.line2().find("Created"), std::string::npos);

    saveFromMenu();
    RhythmPattern p;
    ASSERT_TRUE(parseRhythmPattern(storage_.readFile("/rhythms/user1.json"), p));
    ASSERT_FALSE(p.tracks.empty());
    EXPECT_EQ(p.tracks[0].pattern.size(), 32u);  // two bars
}

TEST_F(PatternEditorEngineTest, NewFlowCanStartRecordingImmediately) {
    editor_->open(EditMenu::RhythmPattern, 0);
    gotoCommand(CMD_NEW);
    press(HID_ENTER);  // name
    press(HID_ENTER);  // default bars
    press(HID_ENTER);  // Record now? -> prompt
    press(HID_ENTER);  // accept and arm

    EXPECT_TRUE(editor_->recording());
    press(HID_ESC);    // cancel the arm/take
    EXPECT_FALSE(editor_->recording());
}

TEST_F(PatternEditorEngineTest, NewRenameAndSaveFromProductionRhythmMenu) {
    editor_->open(EditMenu::RhythmPattern, 0);
    newPattern();
    gotoCommand(CMD_PATTERN);
    display_->update(0);
    EXPECT_NE(lcd_.line2().find("Pat: New Rhythm"), std::string::npos);

    gotoCommand(CMD_RENAME);
    press(HID_ENTER);
    for (int i = 0; i < 10; i++) press(HID_BACKSPACE);
    press(0x17);  // t
    press(0x08);  // e
    press(0x16);  // s
    press(0x17);  // t
    press(HID_ENTER);

    gotoCommand(CMD_PATTERN);
    display_->update(0);
    EXPECT_NE(lcd_.line2().find("Pat: test"), std::string::npos);

    saveFromMenu();
    ASSERT_TRUE(storage_.exists("/rhythms/user1.json"));
    RhythmPattern p;
    ASSERT_TRUE(parseRhythmPattern(storage_.readFile("/rhythms/user1.json"), p));
    EXPECT_EQ(p.name, "test");
}

TEST_F(PatternEditorEngineTest, RhythmEditorForcesMuteOffAndIgnoresF6) {
    state_.pendingRhythm.muted = true;
    editor_->open(EditMenu::RhythmPattern, 0);
    EXPECT_FALSE(state_.pendingRhythm.muted);

    EXPECT_TRUE(editor_->handleKeyEvent(key(HID_F6), atStep(0)));
    EXPECT_TRUE(editor_->handleKeyEvent(key(HID_F6, false), atStep(0)));
    EXPECT_FALSE(state_.pendingRhythm.muted);

    state_.pendingRhythm.channel = 10;
    state_.pendingRhythm.drums.kick = 40;
    state_.pendingRhythm.drums.kick_vel = 77;
    midi_.clear();
    press(HID_KP1);
    EXPECT_TRUE(hasNoteOn(10, 40, 77));

    editor_->close(0);
    EXPECT_TRUE(state_.pendingRhythm.muted);
}

TEST_F(PatternEditorEngineTest, BassEditorForcesEnableOnAndIgnoresF7) {
    state_.pendingBass.enabled = false;
    state_.pendingBass.channel = 3;
    state_.pendingBass.velocity = 73;
    state_.pendingBass.octave = -1;
    editor_->open(EditMenu::BassPattern, 0);
    EXPECT_TRUE(state_.pendingBass.enabled);

    EXPECT_TRUE(editor_->handleKeyEvent(key(HID_F7), atStep(0)));
    EXPECT_TRUE(editor_->handleKeyEvent(key(HID_F7, false), atStep(0)));
    EXPECT_TRUE(state_.pendingBass.enabled);

    midi_.clear();
    press(HID_KP1);
    EXPECT_TRUE(hasNoteOn(3, 48, 73));

    editor_->close(0);
    EXPECT_TRUE(state_.pendingBass.enabled);
}

TEST_F(PatternEditorEngineTest, CloseRestoresRhythmEnableState) {
    editor_->open(EditMenu::Rhythm, 0);
    startPreview();  // preview enables rhythm for the session
    EXPECT_TRUE(state_.pendingRhythm.enabled);
    EXPECT_FALSE(state_.pendingRhythm.muted);
    editor_->close(0);
    EXPECT_FALSE(state_.pendingRhythm.enabled);
    EXPECT_FALSE(state_.pendingRhythm.muted);
}

TEST_F(PatternEditorEngineTest, RecordsRhythmPolyphonicallyAndOverdubs) {
    editor_->open(EditMenu::Rhythm, 0);
    newPattern();       // user pattern, empty
    armRecord();        // count-in bar 0..16; recording starts at 16
    EXPECT_TRUE(editor_->recording());

    setStep(17);
    press(HID_KP1);     // kick at step 0
    press(HID_KP2);     // snare also at step 0
    advanceTo(32);      // one bar recorded -> auto-stop
    EXPECT_FALSE(editor_->recording());

    // Overdub a second take: add an open-hat at step 0.
    startRecord();
    advanceTo(48);      // second take starts at 48
    EXPECT_TRUE(editor_->recording());
    setStep(49);        // step 0 of the take
    press(HID_KP4);     // open-hat (KP4) at step 0
    advanceTo(64);      // one bar completed -> stop

    saveFromMenu();
    ASSERT_TRUE(storage_.exists("/rhythms/user1.json"));
    RhythmPattern p;
    ASSERT_TRUE(parseRhythmPattern(storage_.readFile("/rhythms/user1.json"), p));
    ASSERT_EQ(p.tracks.size(), 3u);
    bool kick = false, snare = false, openHat = false;
    for (const auto& t : p.tracks) {
        if (t.note == 36 && t.pattern[0] == 1) kick = true;
        if (t.note == 38 && t.pattern[0] == 1) snare = true;
        if (t.note == 46 && t.pattern[0] == 1) openHat = true;
    }
    EXPECT_TRUE(kick);
    EXPECT_TRUE(snare);
    EXPECT_TRUE(openHat);
}

TEST_F(PatternEditorEngineTest, RecordsBassSustain) {
    editor_->open(EditMenu::Bass, 0);
    newPattern();
    armRecord();
    setStep(17);
    press(HID_KP1);     // root at step 0, held
    advanceTo(20);
    press(HID_KP1, false);
    advanceTo(32);
    EXPECT_FALSE(editor_->recording());

    saveFromMenu();
    ASSERT_TRUE(storage_.exists("/bass/user1.json"));
    BassPattern p;
    ASSERT_TRUE(parseBassPattern(storage_.readFile("/bass/user1.json"), p));
    ASSERT_EQ(p.steps.size(), 16u);
    EXPECT_EQ(p.steps[0], 0);
    ASSERT_EQ(p.sustain_steps.size(), 16u);
    EXPECT_EQ(p.sustain_steps[0], 4);  // held from step 32 to 36
}

TEST_F(PatternEditorEngineTest, NewBassRecordCountInProgressesWithLiveClock) {
    state_.pendingRhythm.enabled = false;
    state_.rhythmClock.running.store(false);
    editor_->open(EditMenu::BassPattern, 0);
    newPattern();
    startRecord();
    EXPECT_TRUE(editor_->recording());

    uint64_t t = atStep(0);
    bool sawRecording = false;
    for (int i = 0; i < 4000; i++) {
        t += 1000;
        rhythm_->update(t);
        editor_->update(t);
        display_->update(t);
        if (lcd_.line1().find("Recording") != std::string::npos) {
            sawRecording = true;
            break;
        }
    }
    EXPECT_TRUE(sawRecording)
        << "line1=[" << lcd_.line1() << "] line2=[" << lcd_.line2() << "]";
}

TEST_F(PatternEditorEngineTest, NewBassRecordReplansWhenClockRestarts) {
    state_.pendingRhythm.enabled = false;
    state_.rhythmClock.running.store(true);  // stale snapshot before re-enable
    setStep(1000);
    editor_->open(EditMenu::BassPattern, 0);
    newPattern();
    startRecord();
    EXPECT_TRUE(editor_->recording());

    uint64_t t = atStep(1000);
    bool sawRecording = false;
    for (int i = 0; i < 4000; i++) {
        t += 1000;
        rhythm_->update(t);
        editor_->update(t);
        display_->update(t);
        if (lcd_.line1().find("Recording") != std::string::npos) {
            sawRecording = true;
            break;
        }
    }
    EXPECT_TRUE(sawRecording)
        << "line1=[" << lcd_.line1() << "] line2=[" << lcd_.line2() << "]";
}

TEST_F(PatternEditorEngineTest, NewChordRecordCountInProgressesWithLiveClock) {
    state_.pendingRhythm.enabled = false;
    state_.rhythmClock.running.store(false);
    editor_->open(EditMenu::ChordPattern, 0);
    newPattern();
    startRecord();
    EXPECT_TRUE(editor_->recording());

    uint64_t t = atStep(0);
    bool sawRecording = false;
    for (int i = 0; i < 4000; i++) {
        t += 1000;
        rhythm_->update(t);
        editor_->update(t);
        display_->update(t);
        if (lcd_.line1().find("Recording") != std::string::npos) {
            sawRecording = true;
            break;
        }
    }
    EXPECT_TRUE(sawRecording)
        << "line1=[" << lcd_.line1() << "] line2=[" << lcd_.line2() << "]";
}

TEST_F(PatternEditorEngineTest, NewChordRecordReplansWhenClockRestarts) {
    state_.pendingRhythm.enabled = false;
    state_.rhythmClock.running.store(true);  // stale snapshot before re-enable
    setStep(1000);
    editor_->open(EditMenu::ChordPattern, 0);
    newPattern();
    startRecord();
    EXPECT_TRUE(editor_->recording());

    uint64_t t = atStep(1000);
    bool sawRecording = false;
    for (int i = 0; i < 4000; i++) {
        t += 1000;
        rhythm_->update(t);
        editor_->update(t);
        display_->update(t);
        if (lcd_.line1().find("Recording") != std::string::npos) {
            sawRecording = true;
            break;
        }
    }
    EXPECT_TRUE(sawRecording)
        << "line1=[" << lcd_.line1() << "] line2=[" << lcd_.line2() << "]";
}

TEST_F(PatternEditorEngineTest, ChordRecordArmStopsOnStalledClock) {
    editor_->open(EditMenu::ChordPattern, 0);
    newPattern();
    startRecord();
    EXPECT_TRUE(editor_->recording());

    uint64_t t = atStep(0) + 4200000ULL;  // > two bars at 120 BPM
    editor_->update(t);
    display_->update(t);
    EXPECT_FALSE(editor_->recording());
    EXPECT_NE(lcd_.line2().find("Rhythm stalled"), std::string::npos)
        << "line2=[" << lcd_.line2() << "]";
}

TEST_F(PatternEditorEngineTest, RecordsChordDegreeBitmask) {
    editor_->open(EditMenu::Chord, 0);
    newPattern();
    armRecord();
    setStep(17);
    press(HID_KP1);     // root
    press(HID_KP2);     // 3rd (same step)
    advanceTo(32);
    EXPECT_FALSE(editor_->recording());

    saveFromMenu();
    ASSERT_TRUE(storage_.exists("/chordpatterns/user1.json"));
    ChordPattern p;
    ASSERT_TRUE(parseChordPattern(storage_.readFile("/chordpatterns/user1.json"), p));
    ASSERT_EQ(p.steps.size(), 16u);
    EXPECT_EQ(p.steps[0], 3);  // root + 3rd
}

TEST_F(PatternEditorEngineTest, GridEditingWritesRhythmTrack) {
    editor_->open(EditMenu::Rhythm, 0);
    newPattern();
    enterGrid();        // grid, cursor = kick/step 0
    press(HID_ENTER);   // rest -> hit
    press(HID_ESC);     // back to menu
    saveFromMenu();
    ASSERT_TRUE(storage_.exists("/rhythms/user1.json"));
    RhythmPattern p;
    ASSERT_TRUE(parseRhythmPattern(storage_.readFile("/rhythms/user1.json"), p));
    ASSERT_EQ(p.tracks.size(), 1u);
    EXPECT_EQ(p.tracks[0].note, 36);
    EXPECT_EQ(p.tracks[0].pattern[0], 1);
}

TEST_F(PatternEditorEngineTest, PreviewSendsRhythmNotes) {
    editor_->open(EditMenu::Rhythm, 0);  // Rock 1
    startPreview();                      // preview from next bar (step 0)
    midi_.clear();
    advanceTo(1);                        // fires step 0
    EXPECT_GE(midi_.noteOnCount(), 2);   // kick + hi-hat
    press(HID_ENTER);                    // stop
    EXPECT_FALSE(editor_->previewing());
}

TEST_F(PatternEditorEngineTest, MeterMismatchBlocksRecording) {
    // A 3/4 live rhythm vs. the 16-step Walking bass pattern.
    state_.pendingRhythm.pattern = 2;  // Waltz, 12 steps/bar
    editor_->open(EditMenu::Bass, 0);
    startRecord();
    EXPECT_FALSE(editor_->recording());
}

TEST_F(PatternEditorEngineTest, DeletesUserPattern) {
    editor_->open(EditMenu::Rhythm, 0);
    newPattern();
    saveFromMenu();
    ASSERT_TRUE(storage_.exists("/rhythms/user1.json"));

    // Native tests have no Core 1 loop; drain the staged rhythm library before
    // a second save/reload cycle.
    rhythm_->update(atStep(0));

    deletePattern();
    EXPECT_FALSE(storage_.exists("/rhythms/user1.json"));
}


TEST_F(PatternEditorEngineTest, EachKindHasItsOwnEditorScreen) {
    editor_->open(EditMenu::Chord, 0);
    display_->update(0);
    EXPECT_NE(lcd_.line1().find("Chord Pattern"), std::string::npos);
    editor_->close(0);

    editor_->open(EditMenu::Rhythm, 0);
    display_->update(0);
    EXPECT_NE(lcd_.line1().find("Rhythm Pattern"), std::string::npos);
    editor_->close(0);

    editor_->open(EditMenu::Bass, 0);
    display_->update(0);
    EXPECT_NE(lcd_.line1().find("Bass Pattern"), std::string::npos);
    editor_->close(0);
}

TEST_F(PatternEditorEngineTest, BarSelectionCancelDoesNotApply) {
    editor_->open(EditMenu::Rhythm, 0);
    newPattern();                 // command 6 (New)
    gotoCommand(CMD_BARS);
    press(HID_ENTER);             // select Bars
    press(HID_RIGHT);             // pending 2
    press(HID_ESC);               // cancel, back to browsing
    saveFromMenu();

    RhythmPattern p;
    ASSERT_TRUE(parseRhythmPattern(storage_.readFile("/rhythms/user1.json"), p));
    EXPECT_EQ(p.tracks[0].pattern.size(), 16u);  // still 1 bar
}

TEST_F(PatternEditorEngineTest, BarSelectionApplyResizes) {
    editor_->open(EditMenu::Rhythm, 0);
    newPattern();
    gotoCommand(CMD_BARS);
    press(HID_ENTER);
    press(HID_RIGHT);             // pending 2
    press(HID_ENTER);             // apply
    saveFromMenu();

    RhythmPattern p;
    ASSERT_TRUE(parseRhythmPattern(storage_.readFile("/rhythms/user1.json"), p));
    EXPECT_EQ(p.tracks[0].pattern.size(), 32u);  // 2 bars
}

TEST_F(PatternEditorEngineTest, VolumeKeysChangeSelectedValue) {
    editor_->open(EditMenu::Rhythm, 0);
    newPattern();
    gotoCommand(CMD_BARS);
    press(HID_ENTER);             // select
    pressConsumer(hid_usage::CONSUMER_VOLUME_DOWN);  // pending 2
    press(HID_ENTER);             // apply
    saveFromMenu();

    RhythmPattern p;
    ASSERT_TRUE(parseRhythmPattern(storage_.readFile("/rhythms/user1.json"), p));
    EXPECT_EQ(p.tracks[0].pattern.size(), 32u);
}


TEST_F(PatternEditorEngineTest, ChordGridLineDecodesDegrees) {
    editor_->open(EditMenu::Chord, 0);
    newPattern();
    enterGrid();         // grid
    press(HID_KP1);      // root
    press(HID_KP2);      // 3rd
    display_->update(0);
    EXPECT_NE(lcd_.line1().find("R3"), std::string::npos)
        << "line1=[" << lcd_.line1() << "]";
}


TEST_F(PatternEditorEngineTest, LiveBassPadsUsePresetParameters) {
    state_.pendingBass.channel = 3;
    state_.pendingBass.velocity = 73;
    state_.pendingBass.octave = -1;
    editor_->open(EditMenu::Bass, 0);
    midi_.clear();

    press(HID_KP1);  // C root, base 60 - 1 octave = 48
    EXPECT_TRUE(hasNoteOn(3, 48, 73));

    state_.selectedChord = ResolvedChord{7, ChordType::Major};  // G
    press(HID_KP2);  // 3rd -> 55 + 4 = 59
    EXPECT_TRUE(hasNoteOn(3, 59, 73));
}

TEST_F(PatternEditorEngineTest, LiveBassOctaveRootPadPlaysOneOctaveUp) {
    state_.pendingBass.channel = 3;
    state_.pendingBass.velocity = 73;
    state_.pendingBass.octave = -1;
    editor_->open(EditMenu::BassPattern, 0);
    midi_.clear();

    press(HID_KP5);  // root +1 octave: base 60 -1 octave +12 = 60
    EXPECT_TRUE(hasNoteOn(3, 60, 73));
}

TEST_F(PatternEditorEngineTest, BassGridCyclesThroughOctaveRoot) {
    editor_->open(EditMenu::Bass, 0);
    newPattern();
    enterGrid();
    for (int i = 0; i < 5; i++) press(HID_ENTER);  // rest -> R -> 3 -> 5 -> 7 -> R+
    press(HID_ESC);
    saveFromMenu();

    BassPattern p;
    ASSERT_TRUE(parseBassPattern(storage_.readFile("/bass/user1.json"), p));
    EXPECT_EQ(p.steps[0], 4);
}

TEST_F(PatternEditorEngineTest, LiveRhythmPadsUsePresetDrumMap) {
    state_.pendingRhythm.channel = 10;
    state_.pendingRhythm.drums.kick = 40;
    state_.pendingRhythm.drums.kick_vel = 77;
    editor_->open(EditMenu::Rhythm, 0);
    midi_.clear();

    press(HID_KP1);  // kick
    EXPECT_TRUE(hasNoteOn(10, 40, 77));

    state_.pendingRhythm.drums.kick_vel = 128;  // Off
    midi_.clear();
    press(HID_KP1);
    EXPECT_EQ(midi_.noteOnCount(), 0);
}

TEST_F(PatternEditorEngineTest, ChordLivePadsCombineHeldDegrees) {
    state_.pendingChord.channel = 1;
    state_.pendingChord.velocity = 77;
    state_.pendingChord.octave = 0;
    editor_->open(EditMenu::ChordPattern, 0);
    midi_.clear();

    press(HID_KP1);  // root
    EXPECT_TRUE(hasNoteOn(1, 60, 77));

    midi_.clear();
    press(HID_KP2);  // 3rd only; the held root must not retrigger
    EXPECT_TRUE(hasNoteOn(1, 64, 77));
    EXPECT_FALSE(hasNoteOn(1, 60, 77));

    midi_.clear();
    press(HID_KP1, false);  // release root, 3rd remains sounding
    EXPECT_EQ(midi_.noteOffCount(), 1);
    EXPECT_EQ(midi_.noteOnCount(), 0);
}

TEST_F(PatternEditorEngineTest, ChordLivePadsPlayWhileRecording) {
    state_.pendingChord.channel = 1;
    state_.pendingChord.velocity = 77;
    state_.pendingChord.octave = 0;
    editor_->open(EditMenu::ChordPattern, 0);
    newPattern();
    armRecord();  // count-in complete; recording starts at step 16
    setStep(17);

    midi_.clear();
    press(HID_KP1);  // root records and auditions
    EXPECT_TRUE(hasNoteOn(1, 60, 77));

    midi_.clear();
    press(HID_KP2);  // 3rd auditions without retriggering the recorded root
    EXPECT_TRUE(hasNoteOn(1, 64, 77));
    EXPECT_FALSE(hasNoteOn(1, 60, 77));

    advanceTo(32);  // finish the take
}

TEST_F(PatternEditorEngineTest, PlainNumpadIsLiveAndModifiedNumpadPasses) {
    editor_->open(EditMenu::Bass, 0);

    EXPECT_TRUE(editor_->handleKeyEvent(key(HID_KP1), atStep(0)));

    KeyEvent altKpPlus{0x57, true, 0x04, 0, hid_usage::PAGE_KEYBOARD};  // Alt+KpPlus
    EXPECT_FALSE(editor_->handleKeyEvent(altKpPlus, atStep(0)));
    EXPECT_TRUE(edit_->handleKeyEvent(altKpPlus, atStep(0)));
    EXPECT_EQ(state_.pendingStrum.root_pc, 1);
}

TEST_F(PatternEditorEngineTest, ParameterKeysPassThroughIncludingValueSelection) {
    editor_->open(EditMenu::Bass, 0);
    EXPECT_FALSE(editor_->handleKeyEvent(key(HID_F1), atStep(0)));
    EXPECT_TRUE(edit_->handleKeyEvent(key(HID_F1), atStep(0)));
    EXPECT_EQ(state_.pendingChord.play_mode, PlayMode::PressToPlay);

    EXPECT_FALSE(editor_->handleKeyEvent(key(HID_F3), atStep(0)));
    EXPECT_TRUE(edit_->handleKeyEvent(key(HID_F3), atStep(0)));
    EXPECT_EQ(state_.pendingBass.pattern, 1);

    // Also pass while a value item is selected.
    newPattern();
    gotoCommand(CMD_BARS);
    press(HID_ENTER);
    EXPECT_FALSE(editor_->handleKeyEvent(key(HID_F2), atStep(0)));
    EXPECT_TRUE(edit_->handleKeyEvent(key(HID_F2), atStep(0)));
    EXPECT_EQ(state_.pendingChord.arp_mode, ArpMode::Down);
    press(HID_ESC);  // cancel Bars selection
}

TEST_F(PatternEditorEngineTest, RhythmEditorPassesRhythmPatternKey) {
    editor_->open(EditMenu::Rhythm, 0);
    EXPECT_FALSE(editor_->handleKeyEvent(key(HID_F4), atStep(0)));
    EXPECT_TRUE(edit_->handleKeyEvent(key(HID_F4), atStep(0)));
    EXPECT_EQ(state_.pendingRhythm.pattern, 1);
}

TEST_F(PatternEditorEngineTest, ChordKeysPassThroughWhileEditorOpen) {
    editor_->open(EditMenu::Rhythm, 0);
    midi_.clear();

    KeyEvent q = key(0x14, true);  // Q chord key
    routeMain(q, atStep(0));
    chord_->update(atStep(0) + ChordEngine::PRESS_GROUP_US);
    EXPECT_GT(midi_.noteOnCount(), 0);
}

TEST_F(PatternEditorEngineTest, LiveRhythmPadWorksWhileValueSelected) {
    state_.pendingRhythm.channel = 10;
    state_.pendingRhythm.drums.kick = 40;
    state_.pendingRhythm.drums.kick_vel = 77;
    editor_->open(EditMenu::Rhythm, 0);
    newPattern();
    gotoCommand(CMD_BARS);
    press(HID_ENTER);

    midi_.clear();
    press(HID_KP1);
    EXPECT_TRUE(hasNoteOn(10, 40, 77));
    press(HID_ESC);
}

TEST_F(PatternEditorEngineTest, LiveChordPadWorksWhileValueSelected) {
    state_.pendingChord.channel = 1;
    state_.pendingChord.velocity = 77;
    state_.pendingChord.octave = 0;
    editor_->open(EditMenu::ChordPattern, 0);
    newPattern();
    gotoCommand(CMD_BARS);
    press(HID_ENTER);

    midi_.clear();
    press(HID_KP1);
    EXPECT_TRUE(hasNoteOn(1, 60, 77));
    press(HID_ESC);
}

TEST_F(PatternEditorEngineTest, EnterStopsRecordingWithoutF2) {
    editor_->open(EditMenu::Rhythm, 0);
    startRecord();
    advanceTo(8);
    advanceTo(16);
    EXPECT_TRUE(editor_->recording());
    press(HID_ENTER);
    EXPECT_FALSE(editor_->recording());
}

TEST_F(PatternEditorEngineTest, UiRestoresAfterPassedParameterValue) {
    editor_->open(EditMenu::Bass, 0);
    EXPECT_FALSE(editor_->handleKeyEvent(key(HID_F1), atStep(0)));
    EXPECT_TRUE(edit_->handleKeyEvent(key(HID_F1), atStep(0)));
    display_->update(atStep(0));
    EXPECT_NE(lcd_.line1().find("Chord Mode"), std::string::npos);

    uint64_t later = atStep(0) +
        static_cast<uint64_t>(state_.config.display_revert_ms) * 1000ULL + 1;
    editor_->update(later);
    display_->update(later);
    EXPECT_NE(lcd_.line1().find("Bass Pattern"), std::string::npos);
}


TEST_F(PatternEditorEngineTest, ExplicitRhythmEnableEditPersistsOnClose) {
    editor_->open(EditMenu::Rhythm, 0);
    EXPECT_FALSE(editor_->handleKeyEvent(key(0x3E), atStep(0)));  // F5
    EXPECT_TRUE(edit_->handleKeyEvent(key(0x3E), atStep(0)));
    EXPECT_TRUE(state_.pendingRhythm.enabled);
    editor_->close(0);
    EXPECT_TRUE(state_.pendingRhythm.enabled);
}
