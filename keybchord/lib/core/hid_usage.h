#pragma once

#include <cstdint>

// HID usage pages and usages used by the configurable keymap and the
// Consumer Control report parser. Keyboard usages are on page 0x07; media
// keys/volume live on the Consumer page 0x0C.
namespace hid_usage {

constexpr uint8_t PAGE_KEYBOARD = 0x07;
constexpr uint8_t PAGE_CONSUMER = 0x0C;

// Keyboard/Keypad page usages.
constexpr uint16_t KEYBOARD_MUTE        = 0x7F;
constexpr uint16_t KEYBOARD_VOLUME_UP   = 0x80;
constexpr uint16_t KEYBOARD_VOLUME_DOWN = 0x81;

// Consumer page usages (common keyboard media keys).
constexpr uint16_t CONSUMER_POWER          = 0x30;
constexpr uint16_t CONSUMER_SLEEP          = 0x32;
constexpr uint16_t CONSUMER_MUTE           = 0xE2;
constexpr uint16_t CONSUMER_VOLUME_UP      = 0xE9;
constexpr uint16_t CONSUMER_VOLUME_DOWN    = 0xEA;
constexpr uint16_t CONSUMER_PLAY_PAUSE     = 0xCD;
constexpr uint16_t CONSUMER_STOP           = 0xB7;
constexpr uint16_t CONSUMER_PREV_TRACK     = 0xB6;
constexpr uint16_t CONSUMER_NEXT_TRACK     = 0xB5;
constexpr uint16_t CONSUMER_REWIND         = 0xB4;
constexpr uint16_t CONSUMER_FAST_FORWARD   = 0xB3;
constexpr uint16_t CONSUMER_EJECT          = 0xB8;
constexpr uint16_t CONSUMER_MEDIA_SELECT   = 0x183;
constexpr uint16_t CONSUMER_MAIL           = 0x18A;
constexpr uint16_t CONSUMER_CALCULATOR     = 0x192;
constexpr uint16_t CONSUMER_BROWSER_SEARCH = 0x221;
constexpr uint16_t CONSUMER_BROWSER_HOME   = 0x223;
constexpr uint16_t CONSUMER_BROWSER_BACK   = 0x224;
constexpr uint16_t CONSUMER_BROWSER_FORWARD = 0x225;
constexpr uint16_t CONSUMER_BROWSER_STOP   = 0x226;
constexpr uint16_t CONSUMER_BROWSER_REFRESH = 0x227;
constexpr uint16_t CONSUMER_BROWSER_FAVORITES = 0x22A;

} // namespace hid_usage
