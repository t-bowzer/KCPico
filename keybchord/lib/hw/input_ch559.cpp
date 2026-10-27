#include "input_ch559.h"

// Compiled only for the CH559 UART-bridge build.
#if !defined(KEYBCHORD_NATIVE) && defined(KEYBCHORD_CH559)

#include <Arduino.h>
#include "pico/time.h"   // time_us_64()
#include "pins.h"

// LED SET_REPORT relay command: 0xED followed by the 1-byte LED output report
// (must match firmware/ch559/uart.h CH559_CMD_LED).
static constexpr uint8_t CMD_LED = 0xED;


bool InputCh559::begin() {
    Serial1.setTX(PIN_CH559_TX);
    Serial1.setRX(PIN_CH559_RX);
    Serial1.setFIFOSize(2048);
    Serial1.begin(CH559_UART_BAUD, SERIAL_8N1);
    return true;
}

std::vector<KeyEvent> InputCh559::poll() {
    while (Serial1.available() > 0) {
        int c = Serial1.read();
        if (c < 0) break;

        ch559::Message msg;
        if (!parser_.feed(static_cast<uint8_t>(c), msg)) continue;

        controllerReady_ = true;

        switch (msg.type) {
            case ch559::MSG_CONNECTED:
                connected_ = true;
                reportReceived_ = false;
                report_.reset();
                break;
            case ch559::MSG_DISCONNECTED:
                connected_ = false;
                reportReceived_ = false;
                report_.reset();
                break;
            case ch559::MSG_DEVICE_POLL:
                // The CH559 firmware forces the boot protocol, so reports carry
                // no report ID (report_id = 0).
                if (msg.hid_usage == ch559::HID_USAGE_KEYBOARD) {
                    connected_ = true;
                    reportReceived_ = true;
                    report_.apply(msg.payload.data(),
                                  static_cast<uint16_t>(msg.payload.size()),
                                  0, time_us_64(), eventQueue_);
                }
                break;
            default:
                break;
        }
    }

    std::vector<KeyEvent> events;
    std::swap(events, eventQueue_);
    return events;
}

bool InputCh559::setLed(uint8_t led_usage, bool on) {
    switch (led_usage) {
        case 0x01: if (on) led_state_ |= 0x01; else led_state_ &= ~0x01; break;
        case 0x02: if (on) led_state_ |= 0x02; else led_state_ &= ~0x02; break;
        case 0x03: if (on) led_state_ |= 0x04; else led_state_ &= ~0x04; break;
        default: return false;
    }

    Serial1.write(CMD_LED);
    Serial1.write(led_state_);
    return true;
}

#endif // !KEYBCHORD_NATIVE && KEYBCHORD_CH559
