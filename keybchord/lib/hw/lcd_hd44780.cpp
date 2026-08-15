#include "lcd_hd44780.h"

#ifndef KEYBCHORD_NATIVE

#include <Arduino.h>
#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>

#include "debug_log.h"
#include "pins.h"


static bool i2cProbe(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

LcdHd44780::LcdHd44780() = default;
LcdHd44780::~LcdHd44780() = default;

bool LcdHd44780::begin() {
    Wire.setSDA(PIN_I2C_SDA);
    Wire.setSCL(PIN_I2C_SCL);
    Wire.begin();

    uint8_t addr = 0;
    if (i2cProbe(0x27)) {
        addr = 0x27;
    } else if (i2cProbe(0x3F)) {
        addr = 0x3F;
    } else {
        logWarn("LCD: no PCF8574 at 0x27/0x3F; using null adapter");
        return false;
    }

    lcd_ = std::make_unique<hd44780_I2Cexp>(addr);
    lcd_->begin(16, 2);
    lcd_->clear();
    present_ = true;
    return true;
}

void LcdHd44780::write(const std::string& line1, const std::string& line2) {
    if (!present_ || !lcd_) return;
    lcd_->setCursor(0, 0);
    lcd_->print(line1.c_str());
    lcd_->setCursor(0, 1);
    lcd_->print(line2.c_str());
}

void LcdHd44780::clear() {
    if (present_ && lcd_) lcd_->clear();
}

#endif  // KEYBCHORD_NATIVE
