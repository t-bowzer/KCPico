#include <gtest/gtest.h>
#include "factory.h"
#include "adapters_null.h"
#include "midimsg.h"
#include "state.h"
#include "config.h"


int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(M2Scaffolding, FactoryCreatesAdaptersOnNative) {
    Adapters a = createAdapters();
    EXPECT_NE(a.input.get(), nullptr);
    EXPECT_NE(a.midiOut.get(), nullptr);
    EXPECT_NE(a.lcd.get(), nullptr);
    EXPECT_NE(a.storage.get(), nullptr);
}

TEST(M2Scaffolding, NullInputProducesNoEvents) {
    Adapters a = createAdapters();
    auto events = a.input->poll();
    EXPECT_TRUE(events.empty());
}

TEST(M2Scaffolding, NullMidiOutDoesNotCrash) {
    Adapters a = createAdapters();
    auto msg = midi::makeNoteOn(1, 60, 100);
    EXPECT_NO_THROW(a.midiOut->send(msg));
    EXPECT_NO_THROW(a.midiOut->flush());
}

TEST(M2Scaffolding, StorageStubReadsWhatWasWritten) {
    Adapters a = createAdapters();
    EXPECT_TRUE(a.storage->writeFile("/test.txt", "hello"));
    EXPECT_TRUE(a.storage->exists("/test.txt"));
    EXPECT_EQ(a.storage->readFile("/test.txt"), "hello");
}

TEST(M2Scaffolding, ConfigLoadsFromStorageStub) {
    Adapters a = createAdapters();
    AppConfig cfg = AppConfig::load(*a.storage);
    EXPECT_TRUE(cfg.din_enabled);  // defaults since nothing stored yet
}

TEST(M2Scaffolding, NullLcdDoesNotCrash) {
    Adapters a = createAdapters();
    EXPECT_NO_THROW(a.lcd->write("Line1", "Line2"));
    EXPECT_NO_THROW(a.lcd->clear());
}

TEST(M2Scaffolding, NullInputLedDoesNotCrash) {
    Adapters a = createAdapters();
    EXPECT_NO_THROW(a.input->setLed(0x03, true));  // scroll lock
}
