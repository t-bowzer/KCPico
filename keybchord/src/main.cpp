#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) { delay(10); }
    Serial.println("KeybChord Pico — M1 Scaffolding");
}

void loop() {
    delay(100);
}

#if defined(ARDUINO_ARCH_RP2040)
void setup1() {
}

void loop1() {
    delay(100);
}
#endif
