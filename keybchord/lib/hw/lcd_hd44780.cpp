#include "lcd_hd44780.h"

#ifndef KEYBCHORD_NATIVE

#include <Arduino.h>
#include <Wire.h>

#include "debug_log.h"
#include "pins.h"


namespace {

// PCF8574 -> HD44780 bit mapping (standard I2C backpack):
//   P0=RS  P1=RW  P2=EN  P3=backlight  P4=D4  P5=D5  P6=D6  P7=D7
constexpr uint8_t RS = 0x01;
constexpr uint8_t EN = 0x04;

// HD44780 command bytes.
constexpr uint8_t CMD_CLEAR        = 0x01;
constexpr uint8_t CMD_HOME         = 0x80;  // + DDRAM address
constexpr uint8_t CMD_ENTRY        = 0x06;  // increment, no shift
constexpr uint8_t CMD_DISPLAY_OFF  = 0x08;
constexpr uint8_t CMD_DISPLAY_ON   = 0x0C;  // display on, cursor off, no blink
constexpr uint8_t CMD_FUNCTION     = 0x28;  // 4-bit, 2 lines, 5x8 dots

bool i2cProbe(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

} // namespace


LcdHd44780::LcdHd44780() = default;
LcdHd44780::~LcdHd44780() = default;

bool LcdHd44780::begin() {
    Wire.setSDA(PIN_I2C_SDA);
    Wire.setSCL(PIN_I2C_SCL);
    Wire.begin();

    if (i2cProbe(0x27)) {
        addr_ = 0x27;
    } else if (i2cProbe(0x3F)) {
        addr_ = 0x3F;
    } else {
        logWarn("LCD: no PCF8574 at 0x27/0x3F; using null adapter");
        return false;
    }

    // 4-bit mode init sequence (HD44780 datasheet), for a 2-line 5x8 display.
    delay(50);
    write4Bits(0x03, false);
    delayMicroseconds(4500);
    write4Bits(0x03, false);
    delayMicroseconds(150);
    write4Bits(0x03, false);
    delayMicroseconds(150);
    write4Bits(0x02, false);   // switch to 4-bit mode
    delayMicroseconds(150);

    command(CMD_FUNCTION);     // 4-bit, 2 lines, 5x8
    command(CMD_DISPLAY_OFF);
    command(CMD_CLEAR);
    command(CMD_ENTRY);
    command(CMD_DISPLAY_ON);

    present_ = true;
    return true;
}

void LcdHd44780::pulseEnable(uint8_t data) {
    Wire.beginTransmission(addr_);
    Wire.write(data | EN);
    Wire.endTransmission();
    delayMicroseconds(1);
    Wire.beginTransmission(addr_);
    Wire.write(data & ~EN);
    Wire.endTransmission();
    delayMicroseconds(50);
}

void LcdHd44780::write4Bits(uint8_t nibble, bool rs) {
    uint8_t data = static_cast<uint8_t>((nibble & 0x0F) << 4) | backlight_;
    if (rs) data |= RS;
    pulseEnable(data);
}

void LcdHd44780::send(uint8_t value, bool rs) {
    write4Bits(value >> 4, rs);
    write4Bits(value & 0x0F, rs);
}

void LcdHd44780::command(uint8_t value) {
    send(value, false);
    if (value == CMD_CLEAR || value == CMD_HOME) delay(2);
}

void LcdHd44780::writeData(uint8_t value) {
    send(value, true);
}

void LcdHd44780::setCursor(uint8_t col, uint8_t row) {
    static const uint8_t kRowOffset[2] = {0x00, 0x40};
    command(CMD_HOME | (col + kRowOffset[row & 1]));
}

void LcdHd44780::print(const std::string& s) {
    for (unsigned char c : s) {
        uint8_t v = c;
        if (v < 0x20 || v > 0x7E) v = 0x20;   // non-printable -> space
        writeData(v);
    }
}

void LcdHd44780::write(const std::string& line1, const std::string& line2) {
    if (!present_) return;
    setCursor(0, 0);
    print(line1);
    setCursor(0, 1);
    print(line2);
}

void LcdHd44780::clear() {
    if (!present_) return;
    command(CMD_CLEAR);
}


#endif // !KEYBCHORD_NATIVE
