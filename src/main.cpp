#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <lvgl.h>
#include <WiFi.h>

#include "pages.h"
#include "settings.h"
#include "wifi_config.h"
#include "weather.h"

// ==================== Hardware ====================

TFT_eSPI tft = TFT_eSPI();

SPIClass touch_spi(VSPI);
XPT2046_Touchscreen touch(33, 36);

static constexpr uint8_t ONE_WIRE_PIN = 22;

OneWire one_wire(ONE_WIRE_PIN);
DallasTemperature temperature_sensor(&one_wire);

// ==================== Display ====================

static constexpr uint32_t SCREEN_WIDTH = 240;
static constexpr uint32_t SCREEN_HEIGHT = 320;

static constexpr uint32_t BUFFER_LINES = 16;

static uint8_t draw_buffer[
    SCREEN_WIDTH * BUFFER_LINES * 2
];

// ==================== Pages ====================

static lv_obj_t *pages[4] = {
    nullptr,
    nullptr,
    nullptr,
    nullptr
};

// ==================== Temperature Settings ====================

static DeviceSettings device_settings = {
    100,
    1000
};

static uint32_t temperature_refresh_interval = 1000;

static lv_timer_t *temperature_timer = nullptr;

void set_temperature_refresh_interval(uint32_t interval_ms)
{
    temperature_refresh_interval = interval_ms;

    if (temperature_timer != nullptr) {
        lv_timer_set_period(temperature_timer, interval_ms);
    }

    save_temperature_refresh_interval(interval_ms);

    Serial.printf(
        "Temperature refresh interval: %lu ms\n",
        (unsigned long)interval_ms
    );
}

uint32_t get_temperature_refresh_interval()
{
    return temperature_refresh_interval;
}

// ==================== TFT Flush ====================

static void flush_callback(
    lv_display_t *display,
    const lv_area_t *area,
    uint8_t *px_map)
{
    uint32_t width = area->x2 - area->x1 + 1;
    uint32_t height = area->y2 - area->y1 + 1;

    tft.startWrite();

    tft.setAddrWindow(
        area->x1,
        area->y1,
        width,
        height
    );

    tft.pushColors(
        reinterpret_cast<uint16_t *>(px_map),
        width * height,
        true
    );

    tft.endWrite();

    lv_display_flush_ready(display);
}

// ==================== Touch Input ====================

static void touch_read_callback(
    lv_indev_t *indev,
    lv_indev_data_t *data)
{
    if (touch.touched()) {
        TS_Point point = touch.getPoint();

        int16_t x = map(
            point.y,
            3686,
            329,
            0,
            SCREEN_WIDTH - 1
        );

        int16_t y = map(
            point.x,
            225,
            3771,
            0,
            SCREEN_HEIGHT - 1
        );

        x = constrain(x, 0, SCREEN_WIDTH - 1);
        y = constrain(y, 0, SCREEN_HEIGHT - 1);

        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// ==================== Navigation ====================

static void navigation_callback(lv_event_t *event)
{
    lv_obj_t *target = static_cast<lv_obj_t *>(
        lv_event_get_user_data(event)
    );

    lv_screen_load(target);
}

static void create_navigation(lv_obj_t *screen, int active_index)
{
    lv_obj_t *bar = lv_obj_create(screen);

    lv_obj_set_size(bar, 240, 46);

    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_set_style_bg_color(
        bar, lv_color_hex(0x1C2A35), LV_PART_MAIN);

    lv_obj_set_style_border_width(
        bar, 0, LV_PART_MAIN);

    lv_obj_set_style_radius(
        bar, 0, LV_PART_MAIN);

    lv_obj_set_style_pad_all(
        bar, 0, LV_PART_MAIN);

    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    const char *labels[] = {
        "Temp",
        "System",
        "Weather",
        "Settings"
    };

    const int positions[] = {
        3, 61, 119, 179
    };

    const int widths[] = {
        54, 54, 56, 58
    };

    for (int i = 0; i < 4; i++) {
        lv_obj_t *button = lv_button_create(bar);

        lv_obj_set_size(
            button,
            widths[i],
            34
        );

        lv_obj_align(
            button,
            LV_ALIGN_LEFT_MID,
            positions[i],
            0
        );

        lv_obj_set_style_radius(
            button, 8, LV_PART_MAIN);

        lv_obj_set_style_bg_color(
            button,
            lv_color_hex(i == active_index
                ? 0x287A9A
                : 0x344653),
            LV_PART_MAIN
        );

        lv_obj_t *label = lv_label_create(button);

        lv_label_set_text(label, labels[i]);

        lv_obj_set_style_text_font(
            label,
            &lv_font_montserrat_10,
            LV_PART_MAIN
        );

        lv_obj_center(label);

        lv_obj_add_event_cb(
            button,
            navigation_callback,
            LV_EVENT_CLICKED,
            pages[i]
        );
    }
}

// ==================== DS18B20 Timer ====================

static void temperature_timer_callback(lv_timer_t *timer)
{
    float temperature =
        temperature_sensor.getTempCByIndex(0);

    if (temperature == DEVICE_DISCONNECTED_C ||
        temperature < -55.0 ||
        temperature > 125.0) {

        update_temperature_page(0.0f, false);

        Serial.println("DS18B20 disconnected!");
    } else {
        update_temperature_page(temperature, true);

        Serial.printf(
            "Temperature: %.1f C\n",
            temperature
        );
    }

    // Start next conversion without blocking LVGL.
    temperature_sensor.requestTemperatures();
}

// ==================== Wi-Fi ====================

static void wifi_connect_task(void *parameter)
{
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    Serial.println("Connecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // 最多等待 15 秒，但只阻塞当前 Wi-Fi 任务
    for (int i = 0; i < 30; i++)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("WiFi connected!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());

        Serial.print("RSSI: ");
        Serial.println(WiFi.RSSI());
    }
    else
    {
        Serial.println("WiFi connection is still pending.");
        Serial.println("Auto reconnect is enabled.");
    }

    vTaskDelete(nullptr);
}

static void start_wifi_task()
{
    BaseType_t result = xTaskCreate(
        wifi_connect_task,
        "wifi_connect",
        4096,
        nullptr,
        1,
        nullptr
    );

    if (result != pdPASS)
    {
        Serial.println("Failed to create WiFi task!");
    }
}

// ==================== Setup ====================

void setup()
{
    Serial.begin(115200);

    start_wifi_task();

    // Backlight
    // Load saved settings
    device_settings = load_settings();

    temperature_refresh_interval =
      device_settings.temperature_refresh_interval_ms;

    // Backlight
    pinMode(21, OUTPUT);

    analogWrite(
        21,
        map(device_settings.brightness, 0, 100, 0, 255)
        );

    // TFT
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);

    // Touch
    touch_spi.begin(25, 39, 32, 33);
    touch.begin(touch_spi);

    Serial.println("Touch initialized!");

    // DS18B20
    temperature_sensor.begin();

    temperature_sensor.setResolution(10);
    temperature_sensor.setWaitForConversion(false);

    int device_count = temperature_sensor.getDeviceCount();

    Serial.printf(
        "DS18B20 devices found: %d\n",
        device_count
    );

    temperature_sensor.requestTemperatures();

    // LVGL
    lv_init();

    lv_display_t *display = lv_display_create(
        SCREEN_WIDTH,
        SCREEN_HEIGHT
    );

    lv_display_set_color_format(
        display,
        LV_COLOR_FORMAT_RGB565
    );

    lv_display_set_buffers(
        display,
        draw_buffer,
        nullptr,
        sizeof(draw_buffer),
        LV_DISPLAY_RENDER_MODE_PARTIAL
    );

    lv_display_set_flush_cb(
        display,
        flush_callback
    );

    // Create all pages
    pages[0] = create_temperature_page();
    pages[1] = create_system_page();
    pages[2] = create_weather_page();
    pages[3] = create_settings_page();

    // Add navigation to every page
    for (int i = 0; i < 4; i++) {
      create_navigation(pages[i], i);
    }

    // Show temperature page first
    lv_screen_load(pages[0]);

    // Touch input
    lv_indev_t *indev = lv_indev_create();

    lv_indev_set_type(
        indev,
        LV_INDEV_TYPE_POINTER
    );

    lv_indev_set_display(
        indev,
        display
    );

    lv_indev_set_read_cb(
        indev,
        touch_read_callback
    );

    // Create temperature timer using current setting
    temperature_timer = lv_timer_create(
        temperature_timer_callback,
        temperature_refresh_interval,
        nullptr
    );

    // Start background weather service
    start_weather_service();

    Serial.println("Dashboard initialized!");
}

// ==================== Main Loop ====================

void loop()
{
    static uint32_t last_tick = 0;

    uint32_t now = millis();

    lv_tick_inc(now - last_tick);

    last_tick = now;

    lv_timer_handler();

    delay(5);
}
