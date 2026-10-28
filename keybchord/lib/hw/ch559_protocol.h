#pragma once

#include <cstdint>
#include <vector>

// CH559 <-> RP2040 UART framing protocol (atc1441 fork, see
// hardware-test-plan-ch559.md §4.4 and firmware/ch559/uart.h).
//
// Each message is delimited by 0xFE ... 0x0A:
//   0xFE, lenL, lenH, msgtype, type, device, endpoint, v0, v1, v2, v3,
//   payload[len], 0x0A
//
// Pure C++ (no Arduino) so the parser is unit-testable under KEYBCHORD_NATIVE.

namespace ch559 {

// Message types (match firmware/ch559/uart.h MSG_TYPE_*).
constexpr uint8_t MSG_CONNECTED     = 0x01;
constexpr uint8_t MSG_DISCONNECTED  = 0x02;
constexpr uint8_t MSG_ERROR         = 0x03;
constexpr uint8_t MSG_DEVICE_POLL   = 0x04;
constexpr uint8_t MSG_DEVICE_STRING = 0x05;
constexpr uint8_t MSG_DEVICE_INFO   = 0x06;
constexpr uint8_t MSG_HID_INFO      = 0x07;
constexpr uint8_t MSG_STARTUP       = 0x08;

// HID usage value carried in the `type` field of a DEVICE_POLL. The CH559
// reports keyboard interfaces as 0x06 and Consumer Control interfaces as 0x0C
// (the Consumer usage page), so the RP2040 can pick the right report parser.
constexpr uint8_t HID_USAGE_KEYBOARD = 0x06;
constexpr uint8_t HID_USAGE_CONSUMER = 0x0C;

constexpr uint8_t FRAME_START = 0xFE;
constexpr uint8_t FRAME_END   = 0x0A;
constexpr size_t  HEADER_LEN  = 10;  // bytes after FRAME_START, before payload

struct Message {
    uint8_t type = 0;        // message type (MSG_*)
    uint8_t hid_usage = 0;   // HID usage (for DEVICE_POLL)
    uint8_t device = 0;
    uint8_t endpoint = 0;
    std::vector<uint8_t> payload;
};

// Streaming parser that re-syncs on FRAME_START and consumes a single byte at a
// time. `feed` returns true exactly once per completed frame, populating `out`.
class FrameParser {
public:
    void reset() {
        state_ = State::Sync;
        header_pos_ = 0;
        length_ = 0;
        payload_pos_ = 0;
        building_ = Message{};
    }

    bool feed(uint8_t b, Message& out) {
        switch (state_) {
            case State::Sync:
                if (b == FRAME_START) {
                    state_ = State::Header;
                    header_pos_ = 0;
                    building_ = Message{};
                }
                return false;

            case State::Header:
                header_[header_pos_++] = b;
                if (header_pos_ < HEADER_LEN) return false;
                length_ = static_cast<uint16_t>(header_[0]) |
                          (static_cast<uint16_t>(header_[1]) << 8);
                building_.type = header_[2];
                building_.hid_usage = header_[3];
                building_.device = header_[4];
                building_.endpoint = header_[5];
                // header_[6..9] are the 4 vendor/product ID bytes (unused here).
                building_.payload.clear();
                building_.payload.reserve(length_);
                payload_pos_ = 0;
                state_ = (length_ == 0) ? State::Term : State::Payload;
                return false;

            case State::Payload:
                building_.payload.push_back(b);
                if (++payload_pos_ >= length_) {
                    state_ = State::Term;
                }
                return false;

            case State::Term:
                state_ = State::Sync;
                if (b == FRAME_END) {
                    out = std::move(building_);
                    return true;
                }
                return false;
        }
        return false;
    }

private:
    enum class State { Sync, Header, Payload, Term };

    State state_ = State::Sync;
    uint8_t header_[HEADER_LEN]{};
    uint8_t header_pos_ = 0;
    uint16_t length_ = 0;
    uint16_t payload_pos_ = 0;
    Message building_;
};

}  // namespace ch559
