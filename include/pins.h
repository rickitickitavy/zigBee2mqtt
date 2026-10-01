#pragma once

// ESP32-C6-DevKitC-1: GPIO8 is onboard RGB (WS2812) and a strapping pin.
// Do not use GPIO8 as the boot-AP button.
constexpr int PIN_BOOT_BUTTON = 9;
// DevKitC onboard WS2812 (GPIO8) is unused for status. External red LEDs:
constexpr int PIN_STATUS_RGB = 8;
constexpr int PIN_LED1 = 18;
constexpr int PIN_LED2 = 19;
constexpr int PIN_LED3 = 20;
constexpr int PIN_LED4 = 21;
constexpr int PIN_LED5 = 2;
constexpr int PIN_LED6 = 3;

// Role strap (C6 strapping pin): hold through reset. LOW = Wi-Fi host, HIGH = Zigbee slave.
constexpr int PIN_BOARD_ROLE = 15;

// Host GPIO11 → slave EN (active LOW pulse). Idle HIGH. Host only.
constexpr int PIN_SLAVE_RST = 11;

// Inter-chip SPI (same GPIOs on both boards) + slave→host IRQ.
constexpr int PIN_SPI_MISO = 4;
constexpr int PIN_SPI_MOSI = 5;
constexpr int PIN_SPI_SCK = 6;
constexpr int PIN_SPI_CS = 7;
constexpr int PIN_SPI_IRQ = 10;
