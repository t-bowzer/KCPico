#pragma once

#include "base.h"

class NullInputAdapter : public InputAdapter {
public:
    bool begin() override { return true; }
    std::vector<KeyEvent> poll() override { return {}; }
    bool setLed(uint8_t, bool) override { return true; }
};

class NullMidiOutAdapter : public MidiOutAdapter {
public:
    bool begin() override { return true; }
    void send(const MidiMessage&) override {}
    void flush() override {}
};

class NullLcdAdapter : public LcdAdapter {
public:
    bool begin() override { return true; }
    void write(const std::string&, const std::string&) override {}
    void clear() override {}
};

class StubStorageAdapter : public StorageAdapter {
public:
    bool begin() override { return true; }
    bool exists(const std::string& /*path*/) override { return false; }
    std::string readFile(const std::string& /*path*/) override { return ""; }
    bool writeFile(const std::string& /*path*/, const std::string& /*data*/) override { return true; }
    bool mkdir(const std::string& /*path*/) override { return true; }
};
