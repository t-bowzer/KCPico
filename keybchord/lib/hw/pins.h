#pragma once

#define PIN_DIN_TX    8
#define PIN_I2C_SDA   4
#define PIN_I2C_SCL   5

// CH559 USB-host coprocessor UART bridge (Serial1 / UART0).
#define PIN_CH559_TX    0   // Pico TX -> CH559 RXD1 (pin 27)
#define PIN_CH559_RX    1   // Pico RX <- CH559 TXD1 (pin 28)
#define CH559_UART_BAUD 115200
