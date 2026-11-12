#include <Arduino.h>
#include <WiFi.h>

#include "wifi_config.h"
#include "wifi_service.h"

static void wifi_connect_task(void *parameter)
{
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    Serial.println("Connecting to WiFi...");

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // 最多等待 15 秒，但只阻塞当前 Wi-Fi 任务。
    for (int i = 0; i < 30; i++) {
        if (WiFi.status() == WL_CONNECTED) {
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi connected!");

        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());

        Serial.print("RSSI: ");
        Serial.println(WiFi.RSSI());
    } else {
        Serial.println("WiFi connection is still pending.");
        Serial.println("Auto reconnect is enabled.");
    }

    vTaskDelete(nullptr);
}

void wifi_service_begin()
{
    BaseType_t result = xTaskCreate(
        wifi_connect_task,
        "wifi_connect",
        4096,
        nullptr,
        1,
        nullptr
    );

    if (result != pdPASS) {
        Serial.println("Failed to create WiFi task!");
    }
}

bool wifi_service_is_connected()
{
    return WiFi.status() == WL_CONNECTED;
}

IPAddress wifi_service_local_ip()
{
    return WiFi.localIP();
}
