#include <Arduino.h>

void setup() {
}

void loop() {
    delay(1000);
}

#if defined(ARDUINO_ARCH_RP2040)
void setup1() {
}

void loop1() {
    delay(1000);
}
#endif
