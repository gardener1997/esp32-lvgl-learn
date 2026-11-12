#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <lvgl.h>
#include <SD.h>

#include "pages.h"
#include "settings.h"
#include "weather.h"
#include "sd_storage.h"
#include "screenshot.h"
#include "wifi_service.h"
#include "time_service.h"

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

static constexpr uint32_t BUFFER_LINES = 15;

static uint8_t draw_buffer[
    SCREEN_WIDTH * BUFFER_LINES * 2
];

static lv_display_t *main_display = nullptr;

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

// ==================== Serial CLI ====================

static char serial_command_buffer[64];
static size_t serial_command_length = 0;

static bool serial_cli_mode = false;
static bool serial_cli_ignore_lf = false;

static float latest_temperature = 0.0f;
static bool latest_temperature_valid = false;

static void execute_serial_command(const char *command);
static void handle_serial_input();

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

    // Capture the rendered pixels when a screenshot is requested.
    screenshot_capture_flush(area, px_map);

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

        latest_temperature_valid = false;

        update_temperature_page(0.0f, false);

        if (!serial_cli_mode) {
            Serial.println("DS18B20 disconnected!");
        }
    } else {
        latest_temperature = temperature;
        latest_temperature_valid = true;

        update_temperature_page(temperature, true);

        if (!serial_cli_mode) {
            Serial.printf(
                "Temperature: %.1f C\n",
                temperature
            );
        }
    }

    temperature_sensor.requestTemperatures();
}

static void list_sd_root()
{
    if (SD.cardType() == CARD_NONE) {
        Serial.println("SD card not available");
        return;
    }

    File root = SD.open("/");

    if (!root || !root.isDirectory()) {
        Serial.println("Failed to open SD root directory");
        return;
    }

    Serial.println("\nSD card contents:");
    Serial.println("------------------------------");

    int count = 0;
    File entry = root.openNextFile();

    while (entry) {
        if (entry.isDirectory()) {
            Serial.printf("[DIR]  %s\n", entry.name());
        } else {
            Serial.printf(
                "[FILE] %-24s %lu bytes\n",
                entry.name(),
                (unsigned long)entry.size()
            );
        }

        count++;
        entry.close();

        entry = root.openNextFile();
    }

    root.close();

    Serial.println("------------------------------");
    Serial.printf("%d item(s)\n\n", count);
}

// ==================== Serial CLI ====================

static void execute_serial_command(const char *command)
{
    if (strcmp(command, "cli") == 0) {
      serial_cli_mode = true;

      Serial.println("Entering command mode.");
      Serial.println("Type 'help' for commands.");
      return;
    }
    else if (strcmp(command, "quit") == 0) {
      serial_cli_mode = false;

      Serial.println("Leaving command mode.");
      return;
    }
    if (strcmp(command, "help") == 0) {
        Serial.println();
        Serial.println("Available commands:");
        Serial.println("  cli   - Enter command mode");
        Serial.println("  help  - Show commands");
        Serial.println("  temp  - Show current temperature");
        Serial.println("  info  - Show device information");
        Serial.println("  sd     - Initialize SD card");
        Serial.println("  sdtest - Test SD card read/write");
        Serial.println("  screenshot - Capture screen to SD card");
        Serial.println("  ls         - List SD card files");
        Serial.println("  quit  - Exit command mode");
    }
    else if (strcmp(command, "temp") == 0) {
        if (latest_temperature_valid) {
            Serial.printf(
                "Temperature: %.1f C\n",
                latest_temperature
            );
        } else {
            Serial.println("Temperature sensor unavailable.");
        }
    }
    else if (strcmp(command, "info") == 0) {
        Serial.println();
        Serial.println("Device Information");
        Serial.println("------------------");

        Serial.printf(
            "Uptime: %lu seconds\n",
            millis() / 1000
        );

        Serial.printf(
            "Free Heap: %u bytes\n",
            ESP.getFreeHeap()
        );

        Serial.printf(
            "Temperature Refresh: %lu ms\n",
            (unsigned long)temperature_refresh_interval
        );

        Serial.printf(
            "WiFi: %s\n",
            wifi_service_is_connected()
                ? "Connected"
                : "Disconnected"
        );

        if (wifi_service_is_connected()) {
            Serial.print("IP: ");
            Serial.println(wifi_service_local_ip());
        }
    }
    else if (strcmp(command, "sd") == 0) {
      init_sd_card();
    }
    else if (strcmp(command, "sdtest") == 0) {
      test_sd_card();
    }
    else if (strcmp(command, "screenshot") == 0) {
      screenshot_save_bmp(main_display);
    }
    else if (strcmp(command, "ls") == 0) {
      list_sd_root();
    }
    else if (strlen(command) > 0) {
        Serial.print("Unknown command: ");
        Serial.println(command);
        Serial.println("Type 'help' for available commands.");
    }
}

static void handle_serial_input()
{
    while (Serial.available() > 0) {
        char c = Serial.read();

        if (serial_cli_ignore_lf) {
            serial_cli_ignore_lf = false;

            if (c == '\n') {
                continue;
            }
        }

        // Enter
        if (c == '\r' || c == '\n') {
            Serial.println();

            serial_command_buffer[serial_command_length] = '\0';

            execute_serial_command(serial_command_buffer);

            serial_command_length = 0;
            if (serial_cli_mode) {
              Serial.print("cli> ");
            }
            else {
              Serial.print("> ");
            }

            continue;
        }

        // Backspace
        if (c == '\b' || c == 127) {
            if (serial_command_length > 0) {
                serial_command_length--;
                Serial.print("\b \b");
            }

            continue;
        }

        // Ignore non-printable characters
        if (c < 32 || c > 126) {
            continue;
        }

        if (!serial_cli_mode) {
            serial_cli_mode = true;
        }

        // Prevent buffer overflow
        if (serial_command_length < sizeof(serial_command_buffer) - 1) {
            serial_command_buffer[serial_command_length++] = c;
            Serial.write(c);
        }
    }
}

// ==================== Setup ====================

void setup()
{
    Serial.begin(115200);

    wifi_service_begin();

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

    main_display = lv_display_create(
        SCREEN_WIDTH,
        SCREEN_HEIGHT
        );

    lv_display_set_color_format(
        main_display,
        LV_COLOR_FORMAT_RGB565
    );

    lv_display_set_buffers(
        main_display,
        draw_buffer,
        nullptr,
        sizeof(draw_buffer),
        LV_DISPLAY_RENDER_MODE_PARTIAL
    );

    lv_display_set_flush_cb(
        main_display,
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
        main_display
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

    Serial.println();
    Serial.println("Type 'cli' to enter command mode.");
    Serial.print("> ");
}

// ==================== Main Loop ====================

void loop()
{
    static uint32_t last_tick = 0;
    static bool time_service_started = false;

    handle_serial_input();

    // Wi-Fi 连接成功后，启动一次 NTP。
    if (!time_service_started &&
        wifi_service_is_connected()) {

        time_service_begin();
        time_service_started = true;
    }

    uint32_t now = millis();

    lv_tick_inc(now - last_tick);

    last_tick = now;

    lv_timer_handler();

    delay(5);
}
