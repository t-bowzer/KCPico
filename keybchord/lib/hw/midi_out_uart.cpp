#include "midi_out_uart.h"

#ifndef KEYBCHORD_NATIVE

#include <Arduino.h>
#include "pins.h"


bool MidiOutUart::begin() {
    Serial2.setTX(PIN_DIN_TX);
    Serial2.begin(31250, SERIAL_8N1);
    return true;
}

void MidiOutUart::send(const MidiMessage& msg) {
    uint8_t status = msg.status;

    if (status >= 0xF8) {
        Serial2.write(status);
        return;
    }

    Serial2.write(status);
    Serial2.write(msg.data1);

    uint8_t cmd = status & 0xF0;
    if (cmd != 0xC0 && cmd != 0xD0) {
        Serial2.write(msg.data2);
    }
}

void MidiOutUart::flush() {
    Serial2.flush();
}



#endif // !KEYBCHORD_NATIVE
