#pragma once

#include <stdint.h>

struct DeviceSettings {
    uint8_t brightness;
    uint32_t temperature_refresh_interval_ms;
};

// Load all settings from NVS.
DeviceSettings load_settings();

// Save individual settings.
void save_brightness(uint8_t brightness);
void save_temperature_refresh_interval(uint32_t interval_ms);

