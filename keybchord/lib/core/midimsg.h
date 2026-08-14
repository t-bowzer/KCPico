#pragma once

#include <cstdint>
#include "base.h"

namespace midi {

constexpr uint8_t STATUS_NOTE_OFF        = 0x80;
constexpr uint8_t STATUS_NOTE_ON         = 0x90;
constexpr uint8_t STATUS_POLY_PRESSURE   = 0xA0;
constexpr uint8_t STATUS_CONTROL_CHANGE  = 0xB0;
constexpr uint8_t STATUS_PROGRAM_CHANGE  = 0xC0;
constexpr uint8_t STATUS_CHANNEL_PRESSURE = 0xD0;
constexpr uint8_t STATUS_PITCH_BEND      = 0xE0;

constexpr uint8_t SYSTEM_CLOCK    = 0xF8;
constexpr uint8_t SYSTEM_START    = 0xFA;
constexpr uint8_t SYSTEM_CONTINUE = 0xFB;
constexpr uint8_t SYSTEM_STOP     = 0xFC;

constexpr uint8_t CC_BANK_SELECT_MSB = 0;
constexpr uint8_t CC_VOLUME          = 7;
constexpr uint8_t CC_PAN             = 10;
constexpr uint8_t CC_SUSTAIN         = 64;
constexpr uint8_t CC_ALL_SOUND_OFF   = 120;
constexpr uint8_t CC_RESET_ALL       = 121;
constexpr uint8_t CC_ALL_NOTES_OFF   = 123;

constexpr uint8_t LED_NUM_LOCK    = 0x01;
constexpr uint8_t LED_CAPS_LOCK   = 0x02;
constexpr uint8_t LED_SCROLL_LOCK = 0x03;

constexpr uint8_t HID_USAGE_LCtrl  = 0xE1;
constexpr uint8_t HID_USAGE_LShift = 0xE2;
constexpr uint8_t HID_USAGE_LAlt   = 0xE3;
constexpr uint8_t HID_USAGE_LGui   = 0xE4;
constexpr uint8_t HID_USAGE_RCtrl  = 0xE5;
constexpr uint8_t HID_USAGE_RShift = 0xE6;
constexpr uint8_t HID_USAGE_RAlt   = 0xE7;
constexpr uint8_t HID_USAGE_RGui   = 0xE8;

// `channel` is 1-indexed (1..16, per MIDI spec and the Parameter Reference).
inline constexpr uint8_t channelStatus(uint8_t cmd, uint8_t channel) {
    return cmd | ((channel - 1) & 0x0F);
}

inline MidiMessage makeNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    return {channelStatus(STATUS_NOTE_ON, channel), note, velocity};
}

inline MidiMessage makeNoteOff(uint8_t channel, uint8_t note) {
    return {channelStatus(STATUS_NOTE_OFF, channel), note, 0};
}

inline MidiMessage makeCC(uint8_t channel, uint8_t cc, uint8_t value) {
    return {channelStatus(STATUS_CONTROL_CHANGE, channel), cc, value};
}

inline MidiMessage makeProgramChange(uint8_t channel, uint8_t program) {
    return {channelStatus(STATUS_PROGRAM_CHANGE, channel), program, 0};
}

inline MidiMessage makeSystem(uint8_t statusByte) {
    return {statusByte, 0, 0};
}

} // namespace midi

