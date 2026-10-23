#pragma once

#include "base.h"
#include "hid_report.h"


class InputUsbHost : public InputAdapter {
public:
    bool begin() override;
    std::vector<KeyEvent> poll() override;
    bool setLed(uint8_t led_usage, bool on) override;
    bool connected() const override { return mounted_; }
    bool reportReceived() const override { return reportReceived_; }

    void onMount(uint8_t dev_addr, uint8_t instance,
                 const uint8_t* desc_report, uint16_t desc_len);
    void onUmount(uint8_t dev_addr, uint8_t instance);
    void onReport(uint8_t dev_addr, uint8_t instance, const uint8_t* report, uint16_t len);

private:
    bool    mounted_  = false;
    bool    reportReceived_ = false;
    uint8_t dev_addr_  = 0;
    uint8_t instance_  = 0;
    uint8_t report_id_ = 0;   // keyboard input report ID (0 = none, boot protocol)
    uint8_t led_state_ = 0;

    hid_report::BootReportParser report_;

    std::vector<KeyEvent> eventQueue_;
};


