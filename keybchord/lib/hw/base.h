#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct KeyEvent {
    uint8_t hid_usage;
    bool pressed;
    uint8_t modifiers;   // HID modifier byte
    uint64_t received_us = 0;  // device timestamp when the report was received
};

struct MidiMessage {
    uint8_t status;
    uint8_t data1;
    uint8_t data2;
};

class InputAdapter {
public:
    virtual ~InputAdapter() = default;
    virtual bool begin() = 0;
    virtual std::vector<KeyEvent> poll() = 0;
    virtual bool setLed(uint8_t led_usage, bool on) = 0;

    // Whether the keyboard is currently enumerated (NFR-5). Default true so
    // null adapters and tests report "connected".
    virtual bool connected() const { return true; }
};

class MidiOutAdapter {
public:
    virtual ~MidiOutAdapter() = default;
    virtual bool begin() = 0;
    virtual void send(const MidiMessage& msg) = 0;
    virtual void flush() = 0;
};

class LcdAdapter {
public:
    virtual ~LcdAdapter() = default;
    virtual bool begin() = 0;
    virtual void write(const std::string& line1, const std::string& line2) = 0;
    virtual void clear() = 0;
};

class StorageAdapter {
public:
    virtual ~StorageAdapter() = default;
    virtual bool begin() = 0;
    virtual bool exists(const std::string& path) = 0;
    virtual std::string readFile(const std::string& path) = 0;
    virtual bool writeFile(const std::string& path, const std::string& data) = 0;
    virtual bool mkdir(const std::string& path) = 0;
};
