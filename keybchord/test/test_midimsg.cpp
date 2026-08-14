#include <gtest/gtest.h>
#include "midimsg.h"
#include "base.h"


TEST(MidiMsg, ChannelStatus) {
    EXPECT_EQ(midi::channelStatus(midi::STATUS_NOTE_ON, 1), 0x90);
    EXPECT_EQ(midi::channelStatus(midi::STATUS_NOTE_ON, 2), 0x91);
    EXPECT_EQ(midi::channelStatus(midi::STATUS_NOTE_ON, 16), 0x9F);
    EXPECT_EQ(midi::channelStatus(midi::STATUS_CONTROL_CHANGE, 1), 0xB0);
    EXPECT_EQ(midi::channelStatus(midi::STATUS_PROGRAM_CHANGE, 1), 0xC0);
}

TEST(MidiMsg, MakeNoteOn) {
    auto msg = midi::makeNoteOn(1, 60, 100);
    EXPECT_EQ(msg.status, 0x90);
    EXPECT_EQ(msg.data1, 60);
    EXPECT_EQ(msg.data2, 100);
}

TEST(MidiMsg, MakeNoteOff) {
    auto msg = midi::makeNoteOff(1, 60);
    EXPECT_EQ(msg.status, 0x80);  // true Note Off
    EXPECT_EQ(msg.data1, 60);
    EXPECT_EQ(msg.data2, 0);
}

TEST(MidiMsg, MakeCC) {
    auto msg = midi::makeCC(3, 7, 100);
    EXPECT_EQ(msg.status, 0xB2);
    EXPECT_EQ(msg.data1, 7);
    EXPECT_EQ(msg.data2, 100);
}

TEST(MidiMsg, MakeProgramChange) {
    auto msg = midi::makeProgramChange(5, 42);
    EXPECT_EQ(msg.status, 0xC4);
    EXPECT_EQ(msg.data1, 42);
    EXPECT_EQ(msg.data2, 0);
}

TEST(MidiMsg, MakeSystem) {
    auto msg = midi::makeSystem(midi::SYSTEM_CLOCK);
    EXPECT_EQ(msg.status, 0xF8);
    EXPECT_EQ(msg.data1, 0);
    EXPECT_EQ(msg.data2, 0);
}

TEST(MidiMsg, Constants) {
    EXPECT_EQ(midi::CC_ALL_SOUND_OFF, 120);
    EXPECT_EQ(midi::CC_ALL_NOTES_OFF, 123);
    EXPECT_EQ(midi::CC_PAN, 10);
    EXPECT_EQ(midi::CC_VOLUME, 7);
}
