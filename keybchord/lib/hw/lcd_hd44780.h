#pragma once

#include <cstdint>
#include <string>

#include "base.h"


// LCD1602 over a PCF8574 I2C backpack (spec 2.2), driven directly in 4-bit mode
// over I2C (no external LCD library). Boot auto-probe of the PCF8574 address
// (0x27 then 0x3F); if neither ACKs, begin() returns false and the factory falls
// back to the null adapter (NFR-5).
class LcdHd44780 : public LcdAdapter {
public:
    LcdHd44780();
    ~LcdHd44780() override;

    bool begin() override;
    void write(const std::string& line1, const std::string& line2) override;
    void clear() override;

private:
    void pulseEnable(uint8_t data);
    void write4Bits(uint8_t nibble, bool rs);
    void send(uint8_t value, bool rs);
    void command(uint8_t value);
    void writeData(uint8_t value);
    void setCursor(uint8_t col, uint8_t row);
    void print(const std::string& s);

    uint8_t addr_       = 0;
    uint8_t backlight_  = 0x08;  // PCF8574 P3, active-high
    bool    present_    = false;
};
