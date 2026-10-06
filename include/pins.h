#pragma once

#if defined(BOARD_ROLE_HOST)

// ESP32-S3-N16R8 host pin map (avoid octal flash/PSRAM GPIOs 26–37 and common straps).
constexpr int PIN_BOOT_BUTTON = 41;
constexpr int PIN_STATUS_RGB = -1;
constexpr int PIN_LED1 = 4;
constexpr int PIN_LED2 = 5;
constexpr int PIN_LED3 = 6;
constexpr int PIN_LED4 = 7;
constexpr int PIN_LED5 = 15;
constexpr int PIN_LED6 = 16;

constexpr int PIN_SLAVE_RST = 9;

constexpr int PIN_SPI_SCK = 12;
constexpr int PIN_SPI_MOSI = 11;
constexpr int PIN_SPI_MISO = 13;
constexpr int PIN_SPI_CS = 10;
constexpr int PIN_SPI_IRQ = 14;

#elif defined(BOARD_ROLE_SLAVE)

// ESP32-C6-DevKitC-1 slave: GPIO8 is onboard RGB (WS2812) and a strapping pin.
// Do not use GPIO8 as the boot-AP button.
constexpr int PIN_BOOT_BUTTON = 9;
constexpr int PIN_STATUS_RGB = 8;
constexpr int PIN_LED1 = 18;
constexpr int PIN_LED2 = 19;
constexpr int PIN_LED3 = 20;
constexpr int PIN_LED4 = 21;
constexpr int PIN_LED5 = 2;
constexpr int PIN_LED6 = 3;

// Slave does not drive reset; pin kept for header symmetry only.
constexpr int PIN_SLAVE_RST = 11;

constexpr int PIN_SPI_MISO = 4;
constexpr int PIN_SPI_MOSI = 5;
constexpr int PIN_SPI_SCK = 6;
constexpr int PIN_SPI_CS = 7;
constexpr int PIN_SPI_IRQ = 10;

#else
#error "Define BOARD_ROLE_HOST or BOARD_ROLE_SLAVE"
#endif
