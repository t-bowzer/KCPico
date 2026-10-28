#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "base.h"
#include "hid_consumer.h"

namespace {

std::vector<KeyEvent> applyReport(hid_consumer::ReportParser& parser,
                                  const std::vector<uint8_t>& report,
                                  uint64_t when = 1000) {
    std::vector<KeyEvent> out;
    parser.apply(report.data(), static_cast<uint16_t>(report.size()), when, out);
    return out;
}

// Consumer Control bitmap report (report ID 1): Volume Up, Volume Down, Mute.
const std::vector<uint8_t> kBitmapDescriptor = {
    0x05, 0x0C,        // Usage Page (Consumer)
    0x09, 0x01,        // Usage (Consumer Control)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        // Report ID (1)
    0x15, 0x00,        // Logical Minimum (0)
    0x25, 0x01,        // Logical Maximum (1)
    0x75, 0x01,        // Report Size (1)
    0x95, 0x03,        // Report Count (3)
    0x09, 0xE9,        // Usage (Volume Increment)
    0x09, 0xEA,        // Usage (Volume Decrement)
    0x09, 0xE2,        // Usage (Mute)
    0x81, 0x02,        // Input (Data,Var,Abs)
    0xC0,              // End Collection
};

// No report ID; one Volume Up bit.
const std::vector<uint8_t> kNoReportIdDescriptor = {
    0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01,
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x01,
    0x09, 0xE9, 0x81, 0x02, 0xC0,
};

// Array report (report ID 2): one byte whose value is the Consumer usage.
const std::vector<uint8_t> kArrayDescriptor = {
    0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01,
    0x85, 0x02, 0x15, 0x00, 0x26, 0xFF, 0x03,
    0x75, 0x08, 0x95, 0x01, 0x19, 0x00, 0x2A, 0xFF, 0x03,
    0x81, 0x00, 0xC0,
};

} // namespace


TEST(HidConsumer, ParsesBitmapPressAndRelease) {
    hid_consumer::ReportParser parser;
    ASSERT_TRUE(parser.setDescriptor(kBitmapDescriptor.data(),
                                     kBitmapDescriptor.size()));

    auto events = applyReport(parser, {0x01, 0x01});
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].hid_usage, hid_usage::CONSUMER_VOLUME_UP);
    EXPECT_EQ(events[0].usage_page, hid_usage::PAGE_CONSUMER);
    EXPECT_TRUE(events[0].pressed);

    events = applyReport(parser, {0x01, 0x00}, 2000);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].hid_usage, hid_usage::CONSUMER_VOLUME_UP);
    EXPECT_FALSE(events[0].pressed);
}

TEST(HidConsumer, ParsesMultipleBitsAndTransitions) {
    hid_consumer::ReportParser parser;
    ASSERT_TRUE(parser.setDescriptor(kBitmapDescriptor.data(),
                                     kBitmapDescriptor.size()));

    auto events = applyReport(parser, {0x01, 0x02});  // Volume Down
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].hid_usage, hid_usage::CONSUMER_VOLUME_DOWN);
    EXPECT_TRUE(events[0].pressed);

    events = applyReport(parser, {0x01, 0x06}, 2000);  // add Mute
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].hid_usage, hid_usage::CONSUMER_MUTE);
    EXPECT_TRUE(events[0].pressed);

    events = applyReport(parser, {0x01, 0x00}, 3000);  // release both
    ASSERT_EQ(events.size(), 2u);
    EXPECT_FALSE(events[0].pressed);
    EXPECT_FALSE(events[1].pressed);
}

TEST(HidConsumer, ParsesReportWithoutReportId) {
    hid_consumer::ReportParser parser;
    ASSERT_TRUE(parser.setDescriptor(kNoReportIdDescriptor.data(),
                                     kNoReportIdDescriptor.size()));
    auto events = applyReport(parser, {0x01});
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].hid_usage, hid_usage::CONSUMER_VOLUME_UP);
}

TEST(HidConsumer, ParsesArrayReport) {
    hid_consumer::ReportParser parser;
    ASSERT_TRUE(parser.setDescriptor(kArrayDescriptor.data(),
                                     kArrayDescriptor.size()));
    auto events = applyReport(parser, {0x02, 0xE9});
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].hid_usage, hid_usage::CONSUMER_VOLUME_UP);
    EXPECT_TRUE(events[0].pressed);

    events = applyReport(parser, {0x02, 0x00}, 2000);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].hid_usage, hid_usage::CONSUMER_VOLUME_UP);
    EXPECT_FALSE(events[0].pressed);
}

TEST(HidConsumer, IgnoresNonConsumerDescriptor) {
    const std::vector<uint8_t> keyboard = {0x05, 0x07, 0x09, 0x06, 0xA1, 0x01,
                                           0x85, 0x01, 0x75, 0x08, 0x95, 0x01,
                                           0x81, 0x00, 0xC0};
    hid_consumer::ReportParser parser;
    EXPECT_FALSE(parser.setDescriptor(keyboard.data(), keyboard.size()));
}

// QMK-style Consumer Control report: report ID 4, one 16-bit usage array entry
// with logical/usage minimum 1 and maximum 0x2A0.
const std::vector<uint8_t> kQmkConsumerDescriptor = {
    0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01,
    0x85, 0x04, 0x15, 0x01, 0x26, 0xA0, 0x02,
    0x75, 0x10, 0x95, 0x01, 0x19, 0x01, 0x2A, 0xA0, 0x02,
    0x81, 0x00, 0xC0,
};

TEST(HidConsumer, QmkArrayUsesLogicalMinimumAndReportId) {
    hid_consumer::ReportParser parser;
    ASSERT_TRUE(parser.setDescriptor(kQmkConsumerDescriptor.data(),
                                     kQmkConsumerDescriptor.size()));

    auto events = applyReport(parser, {0x04, 0xE9, 0x00});
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].hid_usage, hid_usage::CONSUMER_VOLUME_UP);
    EXPECT_TRUE(events[0].pressed);

    events = applyReport(parser, {0x04, 0x00, 0x00}, 2000);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].hid_usage, hid_usage::CONSUMER_VOLUME_UP);
    EXPECT_FALSE(events[0].pressed);
}

TEST(HidConsumer, QmkConsumerMatchDistinguishesBootKeyboardReport) {
    hid_consumer::ReportParser parser;
    ASSERT_TRUE(parser.setDescriptor(kQmkConsumerDescriptor.data(),
                                     kQmkConsumerDescriptor.size()));

    const uint8_t consumer[3] = {0x04, 0xE9, 0x00};
    EXPECT_TRUE(parser.matchesReport(consumer, sizeof(consumer)));

    // A boot keyboard report with LAlt held starts with 0x04 (the Consumer
    // report ID) but is 8 bytes long and must not be treated as Consumer.
    const uint8_t bootKeyboard[8] = {0x04, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00};
    EXPECT_FALSE(parser.matchesReport(bootKeyboard, sizeof(bootKeyboard)));
}

TEST(HidConsumer, QmkFallbackWithoutDescriptor) {
    hid_consumer::ReportParser parser;
    std::vector<KeyEvent> events;

    const uint8_t press[3] = {0x07, 0xE9, 0x00};
    EXPECT_TRUE(parser.applyQmkConsumer(press, sizeof(press), 1000, events));
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].hid_usage, hid_usage::CONSUMER_VOLUME_UP);
    EXPECT_TRUE(events[0].pressed);

    events.clear();
    const uint8_t release[3] = {0x07, 0x00, 0x00};
    EXPECT_TRUE(parser.applyQmkConsumer(release, sizeof(release), 2000, events));
    ASSERT_EQ(events.size(), 1u);
    EXPECT_FALSE(events[0].pressed);

    events.clear();
    const uint8_t mute[3] = {0x07, 0xE2, 0x00};
    EXPECT_TRUE(parser.applyQmkConsumer(mute, sizeof(mute), 3000, events));
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].hid_usage, hid_usage::CONSUMER_MUTE);
}
