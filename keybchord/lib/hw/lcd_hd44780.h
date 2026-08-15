#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "base.h"

class hd44780_I2Cexp;


// LCD1602 over a PCF8574 I2C backpack (spec 2.2). Boot auto-probe of the
// PCF8574 address (0x27 then 0x3F); if neither ACKs, begin() returns false and
// the factory falls back to the null adapter (NFR-5).
class LcdHd44780 : public LcdAdapter {
public:
    LcdHd44780();
    ~LcdHd44780() override;

    bool begin() override;
    void write(const std::string& line1, const std::string& line2) override;
    void clear() override;

private:
    std::unique_ptr<hd44780_I2Cexp> lcd_;
    bool present_ = false;
};
