#include "input_usbhost.h"

#ifndef KEYBCHORD_NATIVE

#include <Arduino.h>
#include "pio_usb.h"
#include "host/usbh.h"
#include "class/hid/hid_host.h"
#include "pico/time.h"
#include "debug_log.h"

static InputUsbHost* g_inputInstance = nullptr;

extern "C" {

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance,
                      uint8_t const* desc_report, uint16_t desc_len) {
    (void)desc_report;
    (void)desc_len;
    if (g_inputInstance) {
        g_inputInstance->onMount(dev_addr, instance);
    }
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    if (g_inputInstance) {
        g_inputInstance->onUmount(dev_addr, instance);
    }
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance,
                                uint8_t const* report, uint16_t len) {
    if (g_inputInstance) {
        g_inputInstance->onReport(dev_addr, instance, report, len);
    }
}

// Reports the actual result of a keyboard LED SET_REPORT (0xF8-style control
// transfer). `len` is the report length on success, 0 on failure/stall — this
// distinguishes "queued" (tuh_hid_set_report returned true) from "delivered".
void tuh_hid_set_report_complete_cb(uint8_t dev_addr, uint8_t idx,
                                    uint8_t report_id, uint8_t report_type,
                                    uint16_t len) {
    (void)idx;
    (void)report_id;
    (void)report_type;
    logLedComplete(dev_addr, len);
}

} // extern "C"


bool InputUsbHost::begin() {
    g_inputInstance = this;

    pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
    pio_cfg.pin_dp = PIO_USB_DP_PIN_DEFAULT;
    tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);

    if (!tuh_init(1)) {
        return false;
    }
    return true;
}

void InputUsbHost::onMount(uint8_t dev_addr, uint8_t instance) {
    mounted_  = true;
    dev_addr_ = dev_addr;
    instance_ = instance;

    tuh_hid_set_protocol(dev_addr, instance, HID_PROTOCOL_BOOT);
    tuh_hid_receive_report(dev_addr, instance);
}

void InputUsbHost::onUmount(uint8_t dev_addr, uint8_t instance) {
    if (dev_addr == dev_addr_ && instance == instance_) {
        mounted_ = false;
        for (auto& k : prev_keys_) k = 0;
        prev_mods_ = 0;
        led_state_ = 0;
    }
}

void InputUsbHost::onReport(uint8_t dev_addr, uint8_t instance,
                            const uint8_t* report, uint16_t len) {
    if (dev_addr != dev_addr_ || instance != instance_ || len < 8) {
        tuh_hid_receive_report(dev_addr, instance);
        return;
    }

    uint8_t modifiers = report[0];
    uint64_t received_us = time_us_64();
    uint8_t mod_changed = modifiers ^ prev_mods_;
    for (int bit = 0; bit < 8; bit++) {
        if (mod_changed & (1 << bit)) {
            bool pressed = modifiers & (1 << bit);
            uint8_t usage = modifierBitToUsage(bit);
            pushEvent(usage, pressed, modifiers, received_us);
        }
    }
    prev_mods_ = modifiers;

    uint8_t currentKeys[MAX_KEYS];
    for (size_t i = 0; i < MAX_KEYS && (i + 2) < len; i++) {
        currentKeys[i] = report[i + 2];
    }

    for (size_t i = 0; i < MAX_KEYS; i++) {
        if (currentKeys[i] == 0) continue;
        bool found = false;
        for (size_t j = 0; j < MAX_KEYS; j++) {
            if (prev_keys_[j] == currentKeys[i]) { found = true; break; }
        }
        if (!found) pushEvent(currentKeys[i], true, modifiers, received_us);
    }

    for (size_t i = 0; i < MAX_KEYS; i++) {
        if (prev_keys_[i] == 0) continue;
        bool found = false;
        for (size_t j = 0; j < MAX_KEYS; j++) {
            if (currentKeys[j] == prev_keys_[i]) { found = true; break; }
        }
        if (!found) pushEvent(prev_keys_[i], false, modifiers, received_us);
    }

    for (size_t i = 0; i < MAX_KEYS; i++) prev_keys_[i] = currentKeys[i];
    tuh_hid_receive_report(dev_addr, instance);
}

std::vector<KeyEvent> InputUsbHost::poll() {
    tuh_task();
    std::vector<KeyEvent> events;
    std::swap(events, eventQueue_);
    return events;
}

bool InputUsbHost::setLed(uint8_t led_usage, bool on) {
    if (!mounted_) return false;

    switch (led_usage) {
        case 0x01: if (on) led_state_ |= 0x01; else led_state_ &= ~0x01; break;
        case 0x02: if (on) led_state_ |= 0x02; else led_state_ &= ~0x02; break;
        case 0x03: if (on) led_state_ |= 0x04; else led_state_ &= ~0x04; break;
        default: return false;
    }

    return tuh_hid_set_report(dev_addr_, instance_, 0,
                              HID_REPORT_TYPE_OUTPUT, &led_state_, 1);
}

void InputUsbHost::pushEvent(uint8_t usage, bool pressed, uint8_t mods,
                             uint64_t received_us) {
    eventQueue_.push_back({usage, pressed, mods, received_us});
}

uint8_t InputUsbHost::modifierBitToUsage(int bit) {
    static const uint8_t map[8] = {
        0xE1, 0xE2, 0xE3, 0xE4,
        0xE5, 0xE6, 0xE7, 0xE8
    };
    return map[bit];
}



#endif // !KEYBCHORD_NATIVE
