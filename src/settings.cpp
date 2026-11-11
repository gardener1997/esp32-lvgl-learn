#include <Preferences.h>

#include "settings.h"

static constexpr char NVS_NAMESPACE[] = "device";

static constexpr uint8_t DEFAULT_BRIGHTNESS = 100;
static constexpr uint32_t DEFAULT_REFRESH_INTERVAL = 1000;

static bool is_valid_refresh_interval(uint32_t interval_ms)
{
    return interval_ms == 500 ||
           interval_ms == 1000 ||
           interval_ms == 2000 ||
           interval_ms == 5000;
}

DeviceSettings load_settings()
{
    DeviceSettings settings = {
        DEFAULT_BRIGHTNESS,
        DEFAULT_REFRESH_INTERVAL
    };

    Preferences preferences;

    if (!preferences.begin(NVS_NAMESPACE, true)) {
        return settings;
    }

    uint8_t brightness = preferences.getUChar(
        "brightness",
        DEFAULT_BRIGHTNESS
    );

    uint32_t refresh_interval = preferences.getUInt(
        "refresh",
        DEFAULT_REFRESH_INTERVAL
    );

    preferences.end();

    if (brightness <= 100) {
        settings.brightness = brightness;
    }

    if (is_valid_refresh_interval(refresh_interval)) {
        settings.temperature_refresh_interval_ms = refresh_interval;
    }

    return settings;
}

void save_brightness(uint8_t brightness)
{
    if (brightness > 100) {
        return;
    }

    Preferences preferences;

    if (!preferences.begin(NVS_NAMESPACE, false)) {
        return;
    }

    preferences.putUChar("brightness", brightness);

    preferences.end();
}

void save_temperature_refresh_interval(uint32_t interval_ms)
{
    if (!is_valid_refresh_interval(interval_ms)) {
        return;
    }

    Preferences preferences;

    if (!preferences.begin(NVS_NAMESPACE, false)) {
        return;
    }

    preferences.putUInt("refresh", interval_ms);

    preferences.end();
}
