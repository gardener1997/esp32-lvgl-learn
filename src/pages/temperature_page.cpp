#include <Arduino.h>
#include <lvgl.h>
#include <math.h>

#include "pages.h"
#include "time_service.h"

// ==================== Colors ====================

static constexpr uint32_t COLOR_BG = 0x101820;
static constexpr uint32_t COLOR_CARD = 0x1C2A35;
static constexpr uint32_t COLOR_GREEN = 0x4CAF50;
static constexpr uint32_t COLOR_ORANGE = 0xFFB74D;
static constexpr uint32_t COLOR_BLUE = 0x64B5F6;
static constexpr uint32_t COLOR_GRAY = 0xAAB8C2;
static constexpr uint32_t COLOR_RED = 0xEF5350;

// ==================== UI Objects ====================

static lv_obj_t *temperature_label = nullptr;
static lv_obj_t *status_label = nullptr;
static lv_obj_t *max_label = nullptr;
static lv_obj_t *min_label = nullptr;

static lv_obj_t *temperature_chart = nullptr;
static lv_chart_series_t *temperature_series = nullptr;
static lv_obj_t *clock_label = nullptr;

// ==================== Temperature Data ====================

static float current_temperature = NAN;
static float max_temperature = -1000.0;
static float min_temperature = 1000.0;

// ==================== UI Helpers ====================

static lv_obj_t *create_card(
    lv_obj_t *parent,
    int32_t width,
    int32_t height)
{
    lv_obj_t *card = lv_obj_create(parent);

    lv_obj_set_size(card, width, height);

    lv_obj_set_style_bg_color(
        card, lv_color_hex(COLOR_CARD), LV_PART_MAIN);

    lv_obj_set_style_border_width(
        card, 0, LV_PART_MAIN);

    lv_obj_set_style_radius(
        card, 12, LV_PART_MAIN);

    lv_obj_set_style_pad_all(
        card, 0, LV_PART_MAIN);

    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    return card;
}

// ==================== Clock ====================

static void update_clock(lv_timer_t *timer)
{
    lv_obj_t *label = static_cast<lv_obj_t *>(
        lv_timer_get_user_data(timer)
    );

    struct tm timeinfo;
    char buffer[32];

    if (time_service_get_local_time(&timeinfo)) {
        strftime(
            buffer,
            sizeof(buffer),
            "%Y-%m-%d %H:%M:%S",
            &timeinfo
        );
    } else {
        snprintf(buffer, sizeof(buffer), "Syncing...");
    }

    lv_label_set_text(label, buffer);
}

static void clock_timer_deleted(lv_event_t *event)
{
    lv_timer_t *timer = static_cast<lv_timer_t *>(
        lv_event_get_user_data(event)
    );

    lv_timer_delete(timer);
}

// ==================== Reset Statistics ====================

void reset_temperature_stats()
{
    if (isnan(current_temperature)) {
        return;
    }

    max_temperature = current_temperature;
    min_temperature = current_temperature;

    char buffer[24];

    snprintf(
        buffer, sizeof(buffer),
        "%.1f °C", current_temperature);

    lv_label_set_text(max_label, buffer);
    lv_label_set_text(min_label, buffer);

    Serial.println("Temperature statistics reset.");
}

static void reset_stats_callback(lv_event_t *event)
{
    reset_temperature_stats();
}

// ==================== Update Temperature ====================

void update_temperature_page(float temperature, bool connected)
{
    if (!temperature_label || !status_label) {
        return;
    }

    if (!connected) {
        lv_label_set_text(temperature_label, "--.- °C");
        lv_label_set_text(status_label, "Sensor disconnected");

        lv_obj_set_style_text_color(
            status_label,
            lv_color_hex(COLOR_RED),
            LV_PART_MAIN
        );

        return;
    }

    current_temperature = temperature;

    char buffer[24];

    snprintf(
        buffer, sizeof(buffer),
        "%.1f °C", temperature);

    lv_label_set_text(temperature_label, buffer);

    if (temperature > max_temperature) {
        max_temperature = temperature;
    }

    if (temperature < min_temperature) {
        min_temperature = temperature;
    }

    snprintf(
        buffer, sizeof(buffer),
        "%.1f °C", max_temperature);

    lv_label_set_text(max_label, buffer);

    snprintf(
        buffer, sizeof(buffer),
        "%.1f °C", min_temperature);

    lv_label_set_text(min_label, buffer);

    // Update chart
    int16_t chart_value = static_cast<int16_t>(
        lroundf(temperature * 10.0f));

    lv_chart_set_next_value(
        temperature_chart,
        temperature_series,
        chart_value
    );

    lv_obj_invalidate(temperature_chart);

    lv_label_set_text(status_label, "Sensor connected");

    lv_obj_set_style_text_color(
        status_label,
        lv_color_hex(COLOR_GREEN),
        LV_PART_MAIN
    );
}

// ==================== Create Temperature Page ====================

lv_obj_t *create_temperature_page()
{
    lv_obj_t *screen = lv_obj_create(NULL);

    lv_obj_set_style_bg_color(
        screen, lv_color_hex(COLOR_BG), LV_PART_MAIN);

    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t *title = lv_label_create(screen);

    lv_label_set_text(title, "TEMPERATURE MONITOR");

    lv_obj_set_style_text_color(
        title, lv_color_white(), LV_PART_MAIN);

    lv_obj_set_style_text_font(
        title, &lv_font_montserrat_14, LV_PART_MAIN);

    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

    // Clock
    clock_label = lv_label_create(screen);

    lv_label_set_text(clock_label, "Syncing...");

    lv_obj_set_style_text_color(
        clock_label,
        lv_color_hex(COLOR_BLUE),
        LV_PART_MAIN
        );

    lv_obj_set_style_text_font(
        clock_label,
        &lv_font_montserrat_12,
        LV_PART_MAIN
        );

    lv_obj_align(clock_label, LV_ALIGN_TOP_MID, 0, 22);

    // Update clock every second.
    lv_timer_t *clock_timer = lv_timer_create(
        update_clock,
        1000,
        clock_label
        );

    // Update immediately instead of waiting one second.
    update_clock(clock_timer);

    // Delete timer when the page is destroyed.
    lv_obj_add_event_cb(
        screen,
        clock_timer_deleted,
        LV_EVENT_DELETE,
        clock_timer
        );

    // Current temperature card
    lv_obj_t *main_card = create_card(screen, 220, 88);

    lv_obj_align(main_card, LV_ALIGN_TOP_MID, 0, 42);

    lv_obj_t *current_title = lv_label_create(main_card);

    lv_label_set_text(current_title, "CURRENT TEMPERATURE");

    lv_obj_set_style_text_color(
        current_title, lv_color_hex(COLOR_GRAY), LV_PART_MAIN);

    lv_obj_set_style_text_font(
        current_title, &lv_font_montserrat_12, LV_PART_MAIN);

    lv_obj_align(current_title, LV_ALIGN_TOP_MID, 0, 10);

    temperature_label = lv_label_create(main_card);

    lv_label_set_text(temperature_label, "--.- °C");

    lv_obj_set_style_text_color(
        temperature_label, lv_color_hex(COLOR_GREEN), LV_PART_MAIN);

    lv_obj_set_style_text_font(
        temperature_label, &lv_font_montserrat_28, LV_PART_MAIN);

    lv_obj_align(temperature_label, LV_ALIGN_CENTER, 0, 13);

    // Sensor status
    status_label = lv_label_create(screen);

    lv_label_set_text(status_label, "Initializing...");

    lv_obj_set_style_text_color(
        status_label, lv_color_hex(COLOR_GRAY), LV_PART_MAIN);

    lv_obj_set_style_text_font(
        status_label, &lv_font_montserrat_12, LV_PART_MAIN);

    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 132);

    // MAX card
    lv_obj_t *max_card = create_card(screen, 98, 45);

    lv_obj_align(max_card, LV_ALIGN_TOP_LEFT, 16, 157);

    lv_obj_t *max_title = lv_label_create(max_card);

    lv_label_set_text(max_title, "MAX");

    lv_obj_set_style_text_color(
        max_title, lv_color_hex(COLOR_GRAY), LV_PART_MAIN);

    lv_obj_set_style_text_font(
        max_title, &lv_font_montserrat_10, LV_PART_MAIN);

    lv_obj_align(max_title, LV_ALIGN_TOP_LEFT, 9, 5);

    max_label = lv_label_create(max_card);

    lv_label_set_text(max_label, "--.- °C");

    lv_obj_set_style_text_color(
        max_label, lv_color_hex(COLOR_ORANGE), LV_PART_MAIN);

    lv_obj_set_style_text_font(
        max_label, &lv_font_montserrat_12, LV_PART_MAIN);

    lv_obj_align(max_label, LV_ALIGN_BOTTOM_RIGHT, -7, -5);

    // MIN card
    lv_obj_t *min_card = create_card(screen, 98, 45);

    lv_obj_align(min_card, LV_ALIGN_TOP_RIGHT, -16, 157);

    lv_obj_t *min_title = lv_label_create(min_card);

    lv_label_set_text(min_title, "MIN");

    lv_obj_set_style_text_color(
        min_title, lv_color_hex(COLOR_GRAY), LV_PART_MAIN);

    lv_obj_set_style_text_font(
        min_title, &lv_font_montserrat_10, LV_PART_MAIN);

    lv_obj_align(min_title, LV_ALIGN_TOP_LEFT, 9, 5);

    min_label = lv_label_create(min_card);

    lv_label_set_text(min_label, "--.- °C");

    lv_obj_set_style_text_color(
        min_label, lv_color_hex(COLOR_BLUE), LV_PART_MAIN);

    lv_obj_set_style_text_font(
        min_label, &lv_font_montserrat_12, LV_PART_MAIN);

    lv_obj_align(min_label, LV_ALIGN_BOTTOM_RIGHT, -7, -5);

    // Temperature chart card
    lv_obj_t *chart_card = create_card(screen, 220, 65);

    lv_obj_align(chart_card, LV_ALIGN_TOP_MID, 0, 207);

    lv_obj_t *chart_title = lv_label_create(chart_card);

    lv_label_set_text(chart_title, "TEMPERATURE TREND");

    lv_obj_set_style_text_color(
        chart_title, lv_color_hex(COLOR_GRAY), LV_PART_MAIN);

    lv_obj_set_style_text_font(
        chart_title, &lv_font_montserrat_10, LV_PART_MAIN);

    lv_obj_align(chart_title, LV_ALIGN_TOP_LEFT, 9, 5);

    // Reset button
    lv_obj_t *reset_button = lv_button_create(chart_card);

    lv_obj_set_size(reset_button, 72, 23);

    lv_obj_align(reset_button, LV_ALIGN_TOP_RIGHT, -7, 2);

    lv_obj_set_style_bg_color(
        reset_button, lv_color_hex(0x287A9A), LV_PART_MAIN);

    lv_obj_set_style_radius(
        reset_button, 6, LV_PART_MAIN);

    lv_obj_t *reset_text = lv_label_create(reset_button);

    lv_label_set_text(reset_text, "Reset Stats");

    lv_obj_set_style_text_font(
        reset_text, &lv_font_montserrat_10, LV_PART_MAIN);

    lv_obj_center(reset_text);

    lv_obj_add_event_cb(
        reset_button,
        reset_stats_callback,
        LV_EVENT_CLICKED,
        nullptr
    );

    // Chart
    temperature_chart = lv_chart_create(chart_card);

    lv_obj_set_size(temperature_chart, 184, 35);

    lv_obj_align(temperature_chart, LV_ALIGN_BOTTOM_MID, 0, -2);

    lv_chart_set_type(temperature_chart, LV_CHART_TYPE_LINE);

    lv_chart_set_point_count(temperature_chart, 30);

    lv_chart_set_update_mode(
        temperature_chart, LV_CHART_UPDATE_MODE_SHIFT);

    lv_chart_set_range(
        temperature_chart,
        LV_CHART_AXIS_PRIMARY_Y,
        -100,
        500
    );

    lv_chart_set_div_line_count(temperature_chart, 3, 5);

    lv_obj_set_style_bg_color(
        temperature_chart, lv_color_hex(COLOR_CARD), LV_PART_MAIN);

    lv_obj_set_style_border_width(
        temperature_chart, 0, LV_PART_MAIN);

    lv_obj_set_style_line_color(
        temperature_chart, lv_color_hex(0x344653), LV_PART_MAIN);

    lv_obj_set_style_line_width(
        temperature_chart, 1, LV_PART_MAIN);

    temperature_series = lv_chart_add_series(
        temperature_chart,
        lv_color_hex(COLOR_GREEN),
        LV_CHART_AXIS_PRIMARY_Y
    );

    lv_chart_set_all_value(
        temperature_chart,
        temperature_series,
        LV_CHART_POINT_NONE
    );

    lv_chart_refresh(temperature_chart);

    return screen;
}

