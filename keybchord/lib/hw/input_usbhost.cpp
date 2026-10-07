#include "input_usbhost.h"

#ifndef KEYBCHORD_NATIVE

#include <Arduino.h>
#include <cstdio>
#include "pio_usb.h"
#include "host/usbh.h"
#include "host/hcd.h"
#include "class/hid/hid_host.h"
#include "pico/time.h"
#include "debug_log.h"

static InputUsbHost* g_inputInstance = nullptr;

// Set in tuh_event_hook_cb (ISR context) and logged from poll() so we never
// touch the serial port from an interrupt. Lets us see whether the PIO-USB
// host detects a device's D+/D- pull-up at all — the first step in enumerating.
static volatile bool g_attachFlag = false;
static volatile bool g_removeFlag = false;

extern "C" {

// Invoked on every HCD event (attach/remove/xfer-complete), often from the SOF
// timer ISR. Only set flags here; poll() logs them in task context.
void tuh_event_hook_cb(uint8_t rhport, uint32_t eventid, bool in_isr) {
    (void)rhport;
    (void)in_isr;
    if (eventid == HCD_EVENT_DEVICE_ATTACH) {
        g_attachFlag = true;
    } else if (eventid == HCD_EVENT_DEVICE_REMOVE) {
        g_removeFlag = true;
    }
}

// Device-level enumeration diagnostics: log when the device/config descriptors
// are read and when the device mounts/unmounts. This distinguishes "the device
// never enumerates" (no lines) from "the device enumerates but no HID
// interface is recognized as a keyboard" (mount + HID lines, no key events).
void tuh_enum_descriptor_device_cb(uint8_t daddr, const tusb_desc_device_t* desc) {
#if KB_LOG_ON
    if (!g_debugLogEnabled) return;
    if (desc) {
        char buf[96];
        snprintf(buf, sizeof(buf),
                 "USB dev desc dev=%u vid=0x%04X pid=0x%04X class=%u sub=%u proto=%u ncfg=%u ep0=%u",
                 daddr, desc->idVendor, desc->idProduct, desc->bDeviceClass,
                 desc->bDeviceSubClass, desc->bDeviceProtocol,
                 desc->bNumConfigurations, desc->bMaxPacketSize0);
        logInfo(buf);
    }
#endif
}

bool tuh_enum_descriptor_configuration_cb(uint8_t daddr, uint8_t cfg_index,
                                          const tusb_desc_configuration_t* desc) {
#if KB_LOG_ON
    if (!g_debugLogEnabled) return true;
    if (desc) {
        char buf[96];
        snprintf(buf, sizeof(buf),
                 "USB cfg desc dev=%u cfg=%u total=%u nif=%u maxpower=%umA",
                 daddr, cfg_index, desc->wTotalLength, desc->bNumInterfaces,
                 desc->bMaxPower * 2u);
        logInfo(buf);
    }
#endif
    return true;
}

void tuh_mount_cb(uint8_t daddr) {
#if KB_LOG_ON
    if (!g_debugLogEnabled) return;
    char buf[48];
    snprintf(buf, sizeof(buf), "USB device mounted dev=%u", daddr);
    logInfo(buf);
#endif
}

void tuh_umount_cb(uint8_t daddr) {
#if KB_LOG_ON
    if (!g_debugLogEnabled) return;
    char buf[48];
    snprintf(buf, sizeof(buf), "USB device unmounted dev=%u", daddr);
    logInfo(buf);
#endif
}

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance,
                      uint8_t const* desc_report, uint16_t desc_len) {
    if (g_inputInstance) {
        g_inputInstance->onMount(dev_addr, instance, desc_report, desc_len);
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

void InputUsbHost::onMount(uint8_t dev_addr, uint8_t instance,
                           const uint8_t* desc_report, uint16_t desc_len) {
    // tuh_hid_mount_cb fires once per HID interface, not once per device.
    // Composite keyboards (QMK/VIA boards like the Keychron V6) expose several
    // HID interfaces: boot keyboard, mouse, system/consumer control, NKRO
    // keyboard, and a VIA raw-HID interface. We must track the *keyboard*
    // interface specifically; latching the last-mounted interface would leave
    // us polling the wrong one and dropping every real key report.
    uint8_t protocol = tuh_hid_interface_protocol(dev_addr, instance);
    uint8_t report_id = 0;
    bool is_keyboard = false;

#if KB_LOG_ON
    if (g_debugLogEnabled) {
        char buf[64];
        snprintf(buf, sizeof(buf), "HID itf mount dev=%u itf=%u proto=%u",
                 dev_addr, instance, protocol);
        logInfo(buf);
    }
#endif

    if (protocol == HID_ITF_PROTOCOL_KEYBOARD) {
        is_keyboard = true;   // boot keyboard: 8-byte boot-format reports, no ID
    } else if (desc_report && desc_len) {
        // Report-protocol keyboards (common with 2.4 GHz wireless dongles,
        // which usually enumerate with bInterfaceSubClass != boot) prefix each
        // interrupt report with a Report ID. Detect them via the report
        // descriptor's desktop/keyboard usage and remember that ID so
        // onReport() can skip it.
        tuh_hid_report_info_t infos[8];
        uint8_t n = tuh_hid_parse_report_descriptor(infos, 8, desc_report, desc_len);
        for (uint8_t i = 0; i < n; i++) {
            if (infos[i].usage_page == HID_USAGE_PAGE_DESKTOP &&
                infos[i].usage == HID_USAGE_DESKTOP_KEYBOARD) {
                is_keyboard = true;
                report_id = infos[i].report_id;
                break;
            }
        }
    }

    if (!is_keyboard) return;  // ignore mouse/consumer/system-control/VIA interfaces

    // Prefer the boot keyboard (report ID 0). A QMK board also exposes an NKRO
    // keyboard interface (report ID 0x02) that mounts alongside the boot
    // interface; if we already latched the boot keyboard, let it win.
    if (mounted_ && !(report_id == 0 && report_id_ != 0)) {
        return;
    }

    mounted_  = true;
    dev_addr_ = dev_addr;
    instance_ = instance;
    report_id_ = report_id;
    reportReceived_ = false;

#if KB_LOG_ON
    char buf[64];
    snprintf(buf, sizeof(buf), "HID mount dev=%u itf=%u proto=%u rid=0x%02X",
             dev_addr, instance, protocol, report_id);
    logInfo(buf);
#endif

    tuh_hid_set_protocol(dev_addr, instance, HID_PROTOCOL_BOOT);
    tuh_hid_receive_report(dev_addr, instance);
}

void InputUsbHost::onUmount(uint8_t dev_addr, uint8_t instance) {
    if (dev_addr == dev_addr_ && instance == instance_) {
        mounted_ = false;
        report_id_ = 0;
        reportReceived_ = false;
        for (auto& k : prev_keys_) k = 0;
        prev_mods_ = 0;
        led_state_ = 0;
    }
}

void InputUsbHost::onReport(uint8_t dev_addr, uint8_t instance,
                            const uint8_t* report, uint16_t len) {
    if (dev_addr != dev_addr_ || instance != instance_) {
        tuh_hid_receive_report(dev_addr, instance);
        return;
    }

    // A report-protocol keyboard prefixes the report with its Report ID; drop
    // it so the rest parses as the standard 8-byte boot-format layout.
    uint8_t off = 0;
    if (report_id_ != 0 && len > 0 && report[0] == report_id_) {
        off = 1;
    }

    if (len < off + 8) {
        tuh_hid_receive_report(dev_addr, instance);
        return;
    }

    reportReceived_ = true;

    uint8_t modifiers = report[off];
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
    for (size_t i = 0; i < MAX_KEYS && (off + 2 + i) < len; i++) {
        currentKeys[i] = report[off + 2 + i];
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

#if KB_LOG_ON
    if (g_attachFlag) {
        g_attachFlag = false;
        logInfo("USB port attach (device D+/D- pull-up detected)");
    }
    if (g_removeFlag) {
        g_removeFlag = false;
        logInfo("USB port remove");
    }
#endif

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
