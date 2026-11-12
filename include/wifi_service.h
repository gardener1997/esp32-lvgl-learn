#pragma once

#include <WiFi.h>

void wifi_service_begin();

bool wifi_service_is_connected();

IPAddress wifi_service_local_ip();
