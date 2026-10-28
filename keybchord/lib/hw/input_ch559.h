#pragma once

#include <array>

#include "base.h"
#include "ch559_protocol.h"
#include "hid_consumer.h"
#include "hid_report.h"


// Keyboard input adapter that receives raw HID reports from the CH559 USB-host
// coprocessor over a UART bridge. The CH559 enumerates the keyboard and forwards
// DEVICE_POLL frames (0xFE-delimited, see ch559_protocol.h); this adapter
// re-synchronises on frame boundaries and feeds the payloads through the shared
// boot-report parser.
class InputCh559 : public InputAdapter {
public:
    bool begin() override;
    std::vector<KeyEvent> poll() override;
    bool setLed(uint8_t led_usage, bool on) override;
    bool connected() const override { return connected_; }
    bool reportReceived() const override { return reportReceived_; }
    bool controllerReady() const override { return controllerReady_; }

private:
    static constexpr size_t kMaxHidDevices = 8;

    ch559::FrameParser      parser_;
    hid_report::BootReportParser report_;
    std::array<hid_consumer::ReportParser, kMaxHidDevices> consumerParsers_;
    std::vector<KeyEvent>   eventQueue_;

    bool    connected_ = false;
    bool    reportReceived_ = false;
    bool    controllerReady_ = false;
    uint8_t led_state_ = 0;
};
