#include <gtest/gtest.h>
#include "state.h"


TEST(StateManager, Defaults) {
    StateManager sm;
    EXPECT_EQ(sm.pendingChord.channel, 1);
    EXPECT_EQ(sm.pendingStrum.channel, 2);
    EXPECT_EQ(sm.pendingRhythm.channel, 10);
    EXPECT_EQ(sm.currentBank, 0);
    EXPECT_EQ(sm.currentSlot, 0);
    EXPECT_EQ(sm.editMenu, EditMenu::None);
    EXPECT_EQ(sm.editParam, 0);
    EXPECT_FALSE(sm.dirty);
}

TEST(StateManager, SnapshotActive) {
    StateManager sm;
    sm.pendingChord.velocity = 64;
    sm.snapshotActive();
    EXPECT_EQ(sm.activeChord.velocity, 64);
}

TEST(StateManager, SnapshotKeepsActiveIndependent) {
    StateManager sm;
    sm.pendingChord.velocity = 80;
    sm.snapshotActive();
    EXPECT_EQ(sm.activeChord.velocity, 80);

    sm.pendingChord.velocity = 100;
    EXPECT_EQ(sm.activeChord.velocity, 80);  // unchanged
    EXPECT_EQ(sm.pendingChord.velocity, 100);
}

TEST(StateManager, NoteOnOff) {
    StateManager sm;
    EXPECT_FALSE(sm.isNoteActive(1, 60));

    sm.noteOn(1, 60);
    EXPECT_TRUE(sm.isNoteActive(1, 60));
    EXPECT_FALSE(sm.isNoteActive(1, 61));
    EXPECT_FALSE(sm.isNoteActive(2, 60));

    sm.noteOff(1, 60);
    EXPECT_FALSE(sm.isNoteActive(1, 60));
}

TEST(StateManager, AllNotesOff) {
    StateManager sm;
    sm.noteOn(1, 60);
    sm.noteOn(2, 64);
    sm.noteOn(3, 67);
    EXPECT_TRUE(sm.isNoteActive(1, 60));
    EXPECT_TRUE(sm.isNoteActive(2, 64));

    sm.allNotesOff();
    EXPECT_FALSE(sm.isNoteActive(1, 60));
    EXPECT_FALSE(sm.isNoteActive(2, 64));
    EXPECT_FALSE(sm.isNoteActive(3, 67));
}

TEST(StateManager, LoadDefaultsResetsAll) {
    StateManager sm;
    sm.pendingChord.channel = 16;
    sm.dirty = true;
    sm.currentBank = 5;
    sm.noteOn(1, 60);

    sm.loadDefaults();

    EXPECT_EQ(sm.pendingChord.channel, 1);
    EXPECT_FALSE(sm.dirty);
    EXPECT_EQ(sm.currentBank, 0);
    EXPECT_FALSE(sm.isNoteActive(1, 60));
}
