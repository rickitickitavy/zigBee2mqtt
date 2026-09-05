#pragma once

// ESP32-C6-DevKitC-1: GPIO8 is onboard RGB (WS2812) and a strapping pin.
// Do not use GPIO8 as the boot-AP button.
constexpr int PIN_BOOT_BUTTON = 9;
// DevKitC onboard WS2812 is GPIO8 (Arduino already defines PIN_RGB_LED).
