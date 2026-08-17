#pragma once

#include "base.h"


class InputUsbHost : public InputAdapter {
public:
    bool begin() override;
    std::vector<KeyEvent> poll() override;
    bool setLed(uint8_t led_usage, bool on) override;
    bool connected() const override { return mounted_; }

    void onMount(uint8_t dev_addr, uint8_t instance);
    void onUmount(uint8_t dev_addr, uint8_t instance);
    void onReport(uint8_t dev_addr, uint8_t instance, const uint8_t* report, uint16_t len);

private:
    static constexpr size_t MAX_KEYS = 6;
    bool    mounted_  = false;
    uint8_t dev_addr_  = 0;
    uint8_t instance_  = 0;
    uint8_t prev_mods_ = 0;
    uint8_t prev_keys_[MAX_KEYS]{};
    uint8_t led_state_ = 0;

    std::vector<KeyEvent> eventQueue_;

    void pushEvent(uint8_t usage, bool pressed, uint8_t mods, uint64_t received_us);
    static uint8_t modifierBitToUsage(int bit);
};


