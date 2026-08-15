#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "adapters_null.h"
#include "chords.h"
#include "display_manager.h"
#include "state.h"


class RecordingLcdAdapter : public LcdAdapter {
public:
    struct Frame {
        std::string l1;
        std::string l2;
    };

    bool begin() override { return true; }
    void write(const std::string& l1, const std::string& l2) override {
        frames_.push_back({l1, l2});
    }
    void clear() override { clearCount_++; }

    const std::vector<Frame>& frames() const { return frames_; }
    void reset() { frames_.clear(); clearCount_ = 0; }

    int clearCount_ = 0;

private:
    std::vector<Frame> frames_;
};


class DisplayManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        state_.loadDefaults();
        lcd_.reset();
    }

    static std::string pad(const std::string& s) {
        return s + std::string(16 - s.size(), ' ');
    }

    static std::string sp(int n) {
        return std::string(n, ' ');
    }

    StateManager state_;
    RecordingLcdAdapter lcd_;
};


TEST_F(DisplayManagerTest, IdleShowsChordDirtyAndLocation) {
    state_.selectedChord = {3, ChordType::Maj7};  // Eb -> Ebmaj7
    state_.selectedChordValid = true;
    state_.dirty = true;
    state_.currentBank = 0;
    state_.currentSlot = 2;
    state_.pendingRhythm.tempo = 120;
    state_.pendingRhythm.pattern = 0;
    state_.pendingChord.play_mode = PlayMode::Held;

    DisplayManager dm(state_, lcd_);
    dm.update(0);

    ASSERT_EQ(lcd_.frames().size(), 1u);
    EXPECT_EQ(lcd_.frames()[0].l1, "Ebmaj7" + sp(3) + "*B1:P3");
    EXPECT_EQ(lcd_.frames()[0].l2, "q=120 Rk >Held");
}

TEST_F(DisplayManagerTest, IdleWithoutChordShowsPlaceholder) {
    DisplayManager dm(state_, lcd_);
    dm.update(0);

    ASSERT_EQ(lcd_.frames().size(), 1u);
    EXPECT_EQ(lcd_.frames()[0].l1, "--" + sp(8) + "B1:P1");
}

TEST_F(DisplayManagerTest, ShowMenuRendersTitleAndParam) {
    DisplayManager dm(state_, lcd_);
    dm.showMenu("Strum Edit", "Octave");
    dm.update(0);

    ASSERT_EQ(lcd_.frames().size(), 1u);
    EXPECT_EQ(lcd_.frames()[0].l1, pad("Strum Edit"));
    EXPECT_EQ(lcd_.frames()[0].l2, pad("Octave"));
}

TEST_F(DisplayManagerTest, SelectParamUpdatesBottomLine) {
    DisplayManager dm(state_, lcd_);
    dm.showMenu("Strum Edit", "Octave");
    dm.update(0);
    lcd_.reset();

    dm.selectParam("Duration");
    dm.update(0);

    ASSERT_EQ(lcd_.frames().size(), 1u);
    EXPECT_EQ(lcd_.frames()[0].l1, pad("Strum Edit"));
    EXPECT_EQ(lcd_.frames()[0].l2, pad("Duration"));
}

TEST_F(DisplayManagerTest, ShowValueRevertsToMenu) {
    DisplayManager dm(state_, lcd_);
    dm.showMenu("Strum Edit", "Octave");
    dm.update(0);
    lcd_.reset();

    dm.showValue("Strum Octave", "+2", true, 1000);
    dm.update(1000);

    ASSERT_EQ(lcd_.frames().size(), 1u);
    EXPECT_EQ(lcd_.frames()[0].l1, pad("Strum Octave"));
    EXPECT_EQ(lcd_.frames()[0].l2, pad("+2"));

    // Reverts to the menu screen after the revert timeout.
    lcd_.reset();
    dm.update(1000 + 1500UL * 1000);
    ASSERT_EQ(lcd_.frames().size(), 1u);
    EXPECT_EQ(lcd_.frames()[0].l1, pad("Strum Edit"));
    EXPECT_EQ(lcd_.frames()[0].l2, pad("Octave"));
}

TEST_F(DisplayManagerTest, ShowValueRevertsToIdle) {
    DisplayManager dm(state_, lcd_);
    dm.update(0);
    lcd_.reset();

    dm.showValue("Chord Octave", "+1", false, 1000);
    dm.update(1000);
    ASSERT_EQ(lcd_.frames().size(), 1u);
    EXPECT_EQ(lcd_.frames()[0].l1, pad("Chord Octave"));
    EXPECT_EQ(lcd_.frames()[0].l2, pad("+1"));

    lcd_.reset();
    dm.update(1000 + 1500UL * 1000);
    ASSERT_EQ(lcd_.frames().size(), 1u);
    EXPECT_EQ(lcd_.frames()[0].l1, "--" + sp(8) + "B1:P1");
}

TEST_F(DisplayManagerTest, ShowPromptAndAutoCancel) {
    DisplayManager dm(state_, lcd_);
    dm.update(0);
    lcd_.reset();

    dm.showPrompt("Save preset?", 1000);
    dm.update(1000);

    ASSERT_EQ(lcd_.frames().size(), 1u);
    EXPECT_EQ(lcd_.frames()[0].l1, pad("Save preset?"));
    EXPECT_EQ(lcd_.frames()[0].l2, "Enter=Yes Bk=No");

    lcd_.reset();
    dm.update(1000 + 5000UL * 1000);
    ASSERT_EQ(lcd_.frames().size(), 1u);
    EXPECT_EQ(lcd_.frames()[0].l1, "--" + sp(8) + "B1:P1");
}

TEST_F(DisplayManagerTest, NoRedundantWritesWhenIdleUnchanged) {
    DisplayManager dm(state_, lcd_);
    dm.update(0);
    lcd_.reset();

    dm.update(1000);
    EXPECT_TRUE(lcd_.frames().empty());
}

TEST_F(DisplayManagerTest, CancelReturnsToIdle) {
    DisplayManager dm(state_, lcd_);
    dm.showMenu("Strum Edit", "Octave");
    dm.update(0);
    lcd_.reset();

    dm.cancel();
    dm.update(0);
    ASSERT_EQ(lcd_.frames().size(), 1u);
    EXPECT_EQ(lcd_.frames()[0].l1, "--" + sp(8) + "B1:P1");
}

TEST_F(DisplayManagerTest, NullLcdDoesNotCrash) {
    NullLcdAdapter nullLcd;
    DisplayManager dm(state_, nullLcd);

    EXPECT_NO_THROW(dm.update(0));
    EXPECT_NO_THROW(dm.showMenu("Strum Edit", "Octave"));
    EXPECT_NO_THROW(dm.selectParam("Duration"));
    EXPECT_NO_THROW(dm.showValue("Strum Octave", "+2", true, 0));
    EXPECT_NO_THROW(dm.showPrompt("Save?", 0));
    EXPECT_NO_THROW(dm.update(1));
    EXPECT_NO_THROW(dm.cancel());
}
