#include <Arduino.h>
#include <time.h>

#include "time_service.h"

static bool ntp_configured = false;

static constexpr time_t VALID_TIME_THRESHOLD = 1704067200;
// 2024-01-01 00:00:00 UTC

void time_service_begin()
{
    if (ntp_configured) {
        return;
    }

    configTime(
        8 * 3600,
        0,
        "pool.ntp.org",
        "time.nist.gov",
        "cn.pool.ntp.org"
    );

    ntp_configured = true;

    Serial.println("NTP: synchronization started");
}

bool time_service_is_synced()
{
    return time(nullptr) >= VALID_TIME_THRESHOLD;
}

bool time_service_get_local_time(struct tm *timeinfo)
{
    if (!timeinfo || !time_service_is_synced()) {
        return false;
    }

    time_t now = time(nullptr);
    localtime_r(&now, timeinfo);

    return true;
}
