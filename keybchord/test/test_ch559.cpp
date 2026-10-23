#include <gtest/gtest.h>
#include <vector>
#include "base.h"
#include "ch559_protocol.h"
#include "hid_report.h"


namespace {

void appendFrame(std::vector<uint8_t>& buf, uint8_t msgtype, uint8_t type,
                 uint8_t device, uint8_t endpoint,
                 const std::vector<uint8_t>& payload) {
    buf.push_back(ch559::FRAME_START);
    buf.push_back(static_cast<uint8_t>(payload.size() & 0xFF));
    buf.push_back(static_cast<uint8_t>((payload.size() >> 8) & 0xFF));
    buf.push_back(msgtype);
    buf.push_back(type);
    buf.push_back(device);
    buf.push_back(endpoint);
    buf.push_back(0x11); buf.push_back(0x22); buf.push_back(0x33); buf.push_back(0x44);
    for (uint8_t b : payload) buf.push_back(b);
    buf.push_back(ch559::FRAME_END);
}

std::vector<ch559::Message> parseAll(const std::vector<uint8_t>& bytes) {
    ch559::FrameParser p;
    std::vector<ch559::Message> out;
    for (uint8_t b : bytes) {
        ch559::Message m;
        if (p.feed(b, m)) out.push_back(m);
    }
    return out;
}

}  // namespace


TEST(Ch559FrameParser, ParsesDevicePollFrame) {
    std::vector<uint8_t> report = {0x00, 0x00, 0x04, 0x05, 0x06, 0x00, 0x00, 0x00};
    std::vector<uint8_t> bytes;
    appendFrame(bytes, ch559::MSG_DEVICE_POLL, ch559::HID_USAGE_KEYBOARD,
                0x00, 0x01, report);

    auto msgs = parseAll(bytes);
    ASSERT_EQ(msgs.size(), 1u);
    EXPECT_EQ(msgs[0].type, ch559::MSG_DEVICE_POLL);
    EXPECT_EQ(msgs[0].hid_usage, ch559::HID_USAGE_KEYBOARD);
    EXPECT_EQ(msgs[0].device, 0x00);
    EXPECT_EQ(msgs[0].endpoint, 0x01);
    ASSERT_EQ(msgs[0].payload.size(), 8u);
    EXPECT_EQ(msgs[0].payload[2], 0x04);
}

TEST(Ch559FrameParser, ParsesZeroLengthConnectedFrame) {
    std::vector<uint8_t> bytes;
    appendFrame(bytes, ch559::MSG_CONNECTED, 0x01, 0x01, 0x01, {});

    auto msgs = parseAll(bytes);
    ASSERT_EQ(msgs.size(), 1u);
    EXPECT_EQ(msgs[0].type, ch559::MSG_CONNECTED);
    EXPECT_TRUE(msgs[0].payload.empty());
}

TEST(Ch559FrameParser, ResyncsOnLeadingGarbage) {
    std::vector<uint8_t> bytes = {0x01, 0x02, 0x0A, 0xFF};
    std::vector<uint8_t> report = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    appendFrame(bytes, ch559::MSG_DEVICE_POLL, ch559::HID_USAGE_KEYBOARD,
                0x00, 0x01, report);

    auto msgs = parseAll(bytes);
    ASSERT_EQ(msgs.size(), 1u);
    EXPECT_EQ(msgs[0].type, ch559::MSG_DEVICE_POLL);
}

TEST(Ch559FrameParser, ParsesTwoConsecutiveFrames) {
    std::vector<uint8_t> bytes;
    appendFrame(bytes, ch559::MSG_CONNECTED, 0x01, 0x01, 0x01, {});
    std::vector<uint8_t> report = {0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00};
    appendFrame(bytes, ch559::MSG_DEVICE_POLL, ch559::HID_USAGE_KEYBOARD,
                0x00, 0x01, report);

    auto msgs = parseAll(bytes);
    ASSERT_EQ(msgs.size(), 2u);
    EXPECT_EQ(msgs[0].type, ch559::MSG_CONNECTED);
    EXPECT_EQ(msgs[1].type, ch559::MSG_DEVICE_POLL);
}

TEST(HidBootReport, KeyPressAndRelease) {
    hid_report::BootReportParser p;
    std::vector<KeyEvent> out;

    uint8_t press[8] = {0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00};
    EXPECT_TRUE(p.apply(press, 8, 0, 1000, out));
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].hid_usage, 0x04);
    EXPECT_TRUE(out[0].pressed);

    uint8_t release[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    EXPECT_TRUE(p.apply(release, 8, 0, 2000, out));
    ASSERT_EQ(out.size(), 2u);
    EXPECT_EQ(out[1].hid_usage, 0x04);
    EXPECT_FALSE(out[1].pressed);
}

TEST(HidBootReport, ModifierPressAndRelease) {
    hid_report::BootReportParser p;
    std::vector<KeyEvent> out;

    uint8_t press[8] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    EXPECT_TRUE(p.apply(press, 8, 0, 1000, out));
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].hid_usage, 0xE1);  // LCtrl
    EXPECT_TRUE(out[0].pressed);
    EXPECT_EQ(out[0].modifiers, 0x01);

    uint8_t release[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    EXPECT_TRUE(p.apply(release, 8, 0, 2000, out));
    ASSERT_EQ(out.size(), 2u);
    EXPECT_EQ(out[1].hid_usage, 0xE1);
    EXPECT_FALSE(out[1].pressed);
}

TEST(HidBootReport, SkipsReportIdPrefix) {
    hid_report::BootReportParser p;
    std::vector<KeyEvent> out;

    uint8_t report[9] = {0x02, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00};
    EXPECT_TRUE(p.apply(report, 9, 0x02, 1000, out));
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].hid_usage, 0x04);
    EXPECT_TRUE(out[0].pressed);
}

TEST(HidBootReport, RejectsShortReport) {
    hid_report::BootReportParser p;
    std::vector<KeyEvent> out;
    uint8_t shortReport[4] = {0x00, 0x00, 0x04, 0x00};
    EXPECT_FALSE(p.apply(shortReport, 4, 0, 1000, out));
    EXPECT_TRUE(out.empty());
}
