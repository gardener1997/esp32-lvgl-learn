#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <lvgl.h>
#include <math.h>

#include "weather.h"

// ==================== Configuration ====================

static constexpr char WEATHER_URL[] =
    "https://api.open-meteo.com/v1/forecast"
    "?latitude=31.9375"
    "&longitude=103.128356"
    "&current=temperature_2m,relative_humidity_2m,"
    "apparent_temperature,weather_code,wind_speed_10m"
    "&daily=weather_code,temperature_2m_max,temperature_2m_min"
    "&forecast_days=3"
    "&timezone=Asia%2FShanghai";

// ==================== Colors ====================

static constexpr uint32_t COLOR_BG = 0x101820;
static constexpr uint32_t COLOR_CARD = 0x1C2A35;
static constexpr uint32_t COLOR_BLUE = 0x64B5F6;
static constexpr uint32_t COLOR_GREEN = 0x4CAF50;
static constexpr uint32_t COLOR_GRAY = 0xAAB8C2;

// ==================== Weather Data ====================

struct WeatherData {
    bool valid = false;

    float temperature = 0;
    float apparent_temperature = 0;
    float humidity = 0;
    float wind_speed = 0;

    int weather_code = 0;

    char dates[3][12] = {};
    float daily_max[3] = {};
    float daily_min[3] = {};
    int daily_code[3] = {};
};

static WeatherData weather_data;

static portMUX_TYPE weather_mux =
    portMUX_INITIALIZER_UNLOCKED;

// ==================== UI Objects ====================

static lv_obj_t *temperature_label = nullptr;
static lv_obj_t *condition_label = nullptr;
static lv_obj_t *humidity_label = nullptr;
static lv_obj_t *wind_label = nullptr;

static lv_obj_t *forecast_labels[3] = {
    nullptr, nullptr, nullptr
};

// ==================== Weather Description ====================

static const char *weather_description(int code)
{
    switch (code) {
        case 0:
            return "Clear sky";

        case 1:
            return "Mainly clear";

        case 2:
            return "Partly cloudy";

        case 3:
            return "Overcast";

        case 45:
        case 48:
            return "Fog";

        case 51:
        case 53:
        case 55:
            return "Drizzle";

        case 56:
        case 57:
            return "Freezing drizzle";

        case 61:
        case 63:
        case 65:
            return "Rain";

        case 66:
        case 67:
            return "Freezing rain";

        case 71:
        case 73:
        case 75:
        case 77:
            return "Snow";

        case 80:
        case 81:
        case 82:
            return "Rain showers";

        case 85:
        case 86:
            return "Snow showers";

        case 95:
        case 96:
        case 99:
            return "Thunderstorm";

        default:
            return "Unknown";
    }
}

// ==================== Fetch Weather ====================

static bool fetch_weather()
{
    WiFiClientSecure client;

    // Prototype: skip certificate verification.
    // This is acceptable for a non-sensitive public weather request.
    client.setInsecure();

    HTTPClient http;

    http.setTimeout(12000);

    if (!http.begin(client, WEATHER_URL)) {
        Serial.println("Weather: HTTP begin failed");
        return false;
    }

    Serial.println("Fetching weather...");

    int http_code = http.GET();

    if (http_code != HTTP_CODE_OK) {
        Serial.printf(
            "Weather HTTP error: %d\n",
            http_code
        );

        http.end();
        return false;
    }

    String response = http.getString();

    http.end();

    JsonDocument doc;

    DeserializationError error =
        deserializeJson(doc, response);

    if (error) {
        Serial.print("Weather JSON error: ");
        Serial.println(error.c_str());
        return false;
    }

    JsonObjectConst current = doc["current"];
    JsonObjectConst daily = doc["daily"];

    if (current.isNull() || daily.isNull()) {
        Serial.println("Weather: Invalid JSON structure");
        return false;
    }

    JsonArrayConst times = daily["time"];
    JsonArrayConst max_values =
        daily["temperature_2m_max"];
    JsonArrayConst min_values =
        daily["temperature_2m_min"];
    JsonArrayConst codes = daily["weather_code"];

    if (times.size() < 3 ||
        max_values.size() < 3 ||
        min_values.size() < 3 ||
        codes.size() < 3) {

        Serial.println("Weather: Incomplete forecast data");
        return false;
    }

    WeatherData fresh;

    fresh.temperature =
        current["temperature_2m"].as<float>();

    fresh.apparent_temperature =
        current["apparent_temperature"].as<float>();

    fresh.humidity =
        current["relative_humidity_2m"].as<float>();

    fresh.wind_speed =
        current["wind_speed_10m"].as<float>();

    fresh.weather_code =
        current["weather_code"].as<int>();

    for (int i = 0; i < 3; i++) {
        const char *date = times[i] | "";

        snprintf(
            fresh.dates[i],
            sizeof(fresh.dates[i]),
            "%s",
            date
        );

        fresh.daily_max[i] =
            max_values[i].as<float>();

        fresh.daily_min[i] =
            min_values[i].as<float>();

        fresh.daily_code[i] =
            codes[i].as<int>();
    }

    fresh.valid = true;

    portENTER_CRITICAL(&weather_mux);
    weather_data = fresh;
    portEXIT_CRITICAL(&weather_mux);

    Serial.printf(
        "Weather updated: %.1f C, humidity %.0f%%\n",
        fresh.temperature,
        fresh.humidity
    );

    return true;
}

// ==================== Background Task ====================

static void weather_task(void *parameter)
{
    while (true) {
        bool success = false;

        if (WiFi.status() == WL_CONNECTED) {
            success = fetch_weather();
        } else {
            Serial.println("Weather: Wi-Fi disconnected");
        }

        uint32_t wait_time = success
            ? 30UL * 60UL * 1000UL
            : 60UL * 1000UL;

        vTaskDelay(pdMS_TO_TICKS(wait_time));
    }
}

void start_weather_service()
{
    BaseType_t result = xTaskCreate(
        weather_task,
        "weather_task",
        8192,
        nullptr,
        1,
        nullptr
    );

    if (result != pdPASS) {
        Serial.println("Failed to create weather task");
    }
}

// ==================== Number Formatting ====================

static void format_one_decimal(
    char *buffer,
    size_t size,
    float value,
    const char *unit)
{
    int value10 = (int)lroundf(value * 10.0f);

    int whole = abs(value10) / 10;
    int decimal = abs(value10) % 10;

    snprintf(
        buffer,
        size,
        "%s%d.%d %s",
        value10 < 0 ? "-" : "",
        whole,
        decimal,
        unit
    );
}

// ==================== Update Weather UI ====================

static void update_weather_ui(lv_timer_t *timer)
{
    WeatherData data;

    portENTER_CRITICAL(&weather_mux);
    data = weather_data;
    portEXIT_CRITICAL(&weather_mux);

    if (!data.valid) {
        lv_label_set_text(
            temperature_label,
            "--.- C"
        );

        lv_label_set_text(
            condition_label,
            "Waiting for weather data..."
        );

        lv_label_set_text(humidity_label, "--%");
        lv_label_set_text(wind_label, "-- km/h");

        return;
    }

    char buffer[64];

    // Current temperature
    format_one_decimal(
        buffer,
        sizeof(buffer),
        data.temperature,
        "C"
    );

    lv_label_set_text(
        temperature_label,
        buffer
    );

    // Weather condition
    lv_label_set_text(
        condition_label,
        weather_description(data.weather_code)
    );

    // Humidity
    snprintf(
        buffer,
        sizeof(buffer),
        "%d%%",
        (int)lroundf(data.humidity)
    );

    lv_label_set_text(
        humidity_label,
        buffer
    );

    // Wind speed
    format_one_decimal(
        buffer,
        sizeof(buffer),
        data.wind_speed,
        "km/h"
    );

    lv_label_set_text(
        wind_label,
        buffer
    );

    // Three-day forecast
    for (int i = 0; i < 3; i++) {
        const char *date = data.dates[i];

        snprintf(
            buffer,
            sizeof(buffer),
            "%c%c/%c%c  %-12s %d/%d C",
            date[5], date[6],
            date[8], date[9],
            weather_description(data.daily_code[i]),
            (int)lroundf(data.daily_min[i]),
            (int)lroundf(data.daily_max[i])
        );

        lv_label_set_text(
            forecast_labels[i],
            buffer
        );
    }
}

// ==================== UI Helpers ====================

static lv_obj_t *create_weather_card(
    lv_obj_t *parent,
    int x,
    const char *title)
{
    lv_obj_t *card = lv_obj_create(parent);

    lv_obj_set_size(card, 104, 48);
    lv_obj_set_pos(card, x, 132);

    lv_obj_set_style_bg_color(
        card,
        lv_color_hex(COLOR_CARD),
        LV_PART_MAIN
    );

    lv_obj_set_style_border_width(
        card,
        0,
        LV_PART_MAIN
    );

    lv_obj_set_style_radius(
        card,
        10,
        LV_PART_MAIN
    );

    lv_obj_set_style_pad_all(
        card,
        0,
        LV_PART_MAIN
    );

    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_label = lv_label_create(card);

    lv_label_set_text(title_label, title);

    lv_obj_set_style_text_color(
        title_label,
        lv_color_hex(COLOR_GRAY),
        LV_PART_MAIN
    );

    lv_obj_set_style_text_font(
        title_label,
        &lv_font_montserrat_10,
        LV_PART_MAIN
    );

    lv_obj_align(
        title_label,
        LV_ALIGN_TOP_LEFT,
        9,
        6
    );

    return card;
}

static lv_obj_t *create_weather_value(
    lv_obj_t *parent,
    const char *initial_text)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_label_set_text(label, initial_text);

    lv_obj_set_style_text_color(
        label,
        lv_color_hex(COLOR_BLUE),
        LV_PART_MAIN
    );

    lv_obj_set_style_text_font(
        label,
        &lv_font_montserrat_14,
        LV_PART_MAIN
    );

    lv_obj_align(
        label,
        LV_ALIGN_BOTTOM_LEFT,
        9,
        -5
    );

    return label;
}

// ==================== Create Weather Page ====================

lv_obj_t *create_weather_page()
{
    lv_obj_t *screen = lv_obj_create(nullptr);

    lv_obj_set_style_bg_color(
        screen,
        lv_color_hex(COLOR_BG),
        LV_PART_MAIN
    );

    lv_obj_clear_flag(
        screen,
        LV_OBJ_FLAG_SCROLLABLE
    );

    // Title
    lv_obj_t *title = lv_label_create(screen);

    lv_label_set_text(title, "WEATHER");

    lv_obj_set_style_text_color(
        title,
        lv_color_white(),
        LV_PART_MAIN
    );

    lv_obj_set_style_text_font(
        title,
        &lv_font_montserrat_14,
        LV_PART_MAIN
    );

    lv_obj_align(
        title,
        LV_ALIGN_TOP_MID,
        0,
        8
    );

    // City
    lv_obj_t *city = lv_label_create(screen);

    lv_label_set_text(
        city,
        "FUCHOU, SICHUAN"
    );

    lv_obj_set_style_text_color(
        city,
        lv_color_hex(COLOR_GRAY),
        LV_PART_MAIN
    );

    lv_obj_set_style_text_font(
        city,
        &lv_font_montserrat_12,
        LV_PART_MAIN
    );

    lv_obj_align(
        city,
        LV_ALIGN_TOP_MID,
        0,
        30
    );

    // Current temperature
    temperature_label = lv_label_create(screen);

    lv_label_set_text(
        temperature_label,
        "--.- C"
    );

    lv_obj_set_style_text_color(
        temperature_label,
        lv_color_hex(COLOR_BLUE),
        LV_PART_MAIN
    );

    lv_obj_set_style_text_font(
        temperature_label,
        &lv_font_montserrat_32,
        LV_PART_MAIN
    );

    lv_obj_align(
        temperature_label,
        LV_ALIGN_TOP_MID,
        0,
        52
    );

    // Weather condition
    condition_label = lv_label_create(screen);

    lv_label_set_text(
        condition_label,
        "Waiting for weather data..."
    );

    lv_obj_set_style_text_color(
        condition_label,
        lv_color_white(),
        LV_PART_MAIN
    );

    lv_obj_set_style_text_font(
        condition_label,
        &lv_font_montserrat_12,
        LV_PART_MAIN
    );

    lv_obj_align(
        condition_label,
        LV_ALIGN_TOP_MID,
        0,
        108
    );

    // Humidity card
    lv_obj_t *humidity_card = create_weather_card(
        screen,
        12,
        "HUMIDITY"
    );

    humidity_label = create_weather_value(
        humidity_card,
        "--%"
    );

    // Wind card
    lv_obj_t *wind_card = create_weather_card(
        screen,
        124,
        "WIND SPEED"
    );

    wind_label = create_weather_value(
        wind_card,
        "-- km/h"
    );

    // Forecast title
    lv_obj_t *forecast_title = lv_label_create(screen);

    lv_label_set_text(
        forecast_title,
        "3-DAY FORECAST"
    );

    lv_obj_set_style_text_color(
        forecast_title,
        lv_color_white(),
        LV_PART_MAIN
    );

    lv_obj_set_style_text_font(
        forecast_title,
        &lv_font_montserrat_12,
        LV_PART_MAIN
    );

    lv_obj_align(
        forecast_title,
        LV_ALIGN_TOP_LEFT,
        14,
        190
    );

    // Forecast rows
    for (int i = 0; i < 3; i++) {
        forecast_labels[i] = lv_label_create(screen);

        lv_label_set_text(
            forecast_labels[i],
            "--/--  Loading..."
        );

        lv_obj_set_style_text_color(
            forecast_labels[i],
            lv_color_hex(COLOR_GRAY),
            LV_PART_MAIN
        );

        lv_obj_set_style_text_font(
            forecast_labels[i],
            &lv_font_montserrat_10,
            LV_PART_MAIN
        );

        lv_obj_align(
            forecast_labels[i],
            LV_ALIGN_TOP_LEFT,
            14,
            212 + i * 19
        );
    }

    // Refresh UI from cached data every 2 seconds.
    lv_timer_create(
        update_weather_ui,
        2000,
        nullptr
    );

    return screen;
}
