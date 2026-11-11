#include <Arduino.h>
#include <WiFi.h>
#include <lvgl.h>
#include <esp32-hal-adc.h>

#include "pages.h"

// ==================== Colors ====================

static constexpr uint32_t COLOR_BG = 0x101820;
static constexpr uint32_t COLOR_CARD = 0x1C2A35;
static constexpr uint32_t COLOR_GREEN = 0x4CAF50;
static constexpr uint32_t COLOR_BLUE = 0x64B5F6;
static constexpr uint32_t COLOR_ORANGE = 0xFFB74D;
static constexpr uint32_t COLOR_GRAY = 0xAAB8C2;

// ==================== UI Objects ====================

static lv_obj_t *uptime_label = nullptr;
static lv_obj_t *total_heap_label = nullptr;
static lv_obj_t *free_heap_label = nullptr;
static lv_obj_t *min_heap_label = nullptr;
static lv_obj_t *cpu_label = nullptr;
static lv_obj_t *flash_label = nullptr;
static lv_obj_t *chip_label = nullptr;
static lv_obj_t *wifi_label = nullptr;
static lv_obj_t *light_label = nullptr;

// ==================== UI Helpers ====================

static lv_obj_t *create_card(
    lv_obj_t *parent,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height)
{
    lv_obj_t *card = lv_obj_create(parent);

    lv_obj_set_size(card, width, height);
    lv_obj_set_pos(card, x, y);

    lv_obj_set_style_bg_color(
        card, lv_color_hex(COLOR_CARD), LV_PART_MAIN);

    lv_obj_set_style_border_width(
        card, 0, LV_PART_MAIN);

    lv_obj_set_style_radius(
        card, 10, LV_PART_MAIN);

    lv_obj_set_style_pad_all(
        card, 0, LV_PART_MAIN);

    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    return card;
}

static lv_obj_t *create_card_title(
    lv_obj_t *parent,
    const char *text)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_label_set_text(label, text);

    lv_obj_set_style_text_color(
        label, lv_color_hex(COLOR_GRAY), LV_PART_MAIN);

    lv_obj_set_style_text_font(
        label, &lv_font_montserrat_10, LV_PART_MAIN);

    lv_obj_align(
        label, LV_ALIGN_TOP_LEFT, 9, 7);

    return label;
}

static lv_obj_t *create_card_value(
    lv_obj_t *parent,
    const char *text,
    uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_label_set_text(label, text);

    lv_obj_set_style_text_color(
        label, lv_color_hex(color), LV_PART_MAIN);

    lv_obj_set_style_text_font(
        label, &lv_font_montserrat_14, LV_PART_MAIN);

    lv_obj_align(
        label, LV_ALIGN_BOTTOM_LEFT, 9, -7);

    return label;
}

// ==================== Update System Information ====================

static void update_system_info(lv_timer_t *timer)
{
    char buffer[48];

    // Uptime
    uint32_t total_seconds = millis() / 1000;

    uint32_t days = total_seconds / 86400;
    uint32_t hours = (total_seconds % 86400) / 3600;
    uint32_t minutes = (total_seconds % 3600) / 60;
    uint32_t seconds = total_seconds % 60;

    if (days > 0) {
        snprintf(
            buffer, sizeof(buffer),
            "%lu d %02lu:%02lu:%02lu",
            (unsigned long)days,
            (unsigned long)hours,
            (unsigned long)minutes,
            (unsigned long)seconds
        );
    } else {
        snprintf(
            buffer, sizeof(buffer),
            "%02lu:%02lu:%02lu",
            (unsigned long)hours,
            (unsigned long)minutes,
            (unsigned long)seconds
        );
    }

    lv_label_set_text(uptime_label, buffer);

    // Total heap
    snprintf(
        buffer, sizeof(buffer),
        "%lu KB",
        (unsigned long)(ESP.getHeapSize() / 1024)
    );
    lv_label_set_text(total_heap_label, buffer);

    // Free heap
    snprintf(
        buffer, sizeof(buffer),
        "%lu KB",
        (unsigned long)(ESP.getFreeHeap() / 1024)
    );
    lv_label_set_text(free_heap_label, buffer);

    // Minimum free heap
    snprintf(
        buffer, sizeof(buffer),
        "%lu KB",
        (unsigned long)(ESP.getMinFreeHeap() / 1024)
    );
    lv_label_set_text(min_heap_label, buffer);

    // CPU frequency
    snprintf(
        buffer, sizeof(buffer),
        "%u MHz",
        ESP.getCpuFreqMHz()
    );
    lv_label_set_text(cpu_label, buffer);

    // Flash size
    snprintf(
        buffer, sizeof(buffer),
        "%lu MB",
        (unsigned long)(ESP.getFlashChipSize() / (1024 * 1024))
    );
    lv_label_set_text(flash_label, buffer);

    // Chip information
    snprintf(
        buffer, sizeof(buffer),
        "%s  Rev.%d",
        ESP.getChipModel(),
        ESP.getChipRevision()
    );
    lv_label_set_text(chip_label, buffer);

    // Wi-Fi status
    if (WiFi.status() == WL_CONNECTED) {
        lv_label_set_text(wifi_label, "Wi-Fi Connected");

        lv_obj_set_style_text_color(
            wifi_label,
            lv_color_hex(COLOR_GREEN),
            LV_PART_MAIN
        );
    } else {
        lv_label_set_text(wifi_label, "Wi-Fi Disconnected");

        lv_obj_set_style_text_color(
            wifi_label,
            lv_color_hex(COLOR_ORANGE),
            LV_PART_MAIN
        );
    }

    // Ambient light sensor (GPIO34)
    analogSetPinAttenuation(34, ADC_0db);

    int light_value = analogRead(34);

    snprintf(
        buffer, sizeof(buffer),
        "LDR ADC: %d",
        light_value
        );
    lv_label_set_text(light_label, buffer);
}

// ==================== Create System Page ====================

lv_obj_t *create_system_page()
{
    lv_obj_t *screen = lv_obj_create(NULL);

    lv_obj_set_style_bg_color(
        screen, lv_color_hex(COLOR_BG), LV_PART_MAIN);

    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t *title = lv_label_create(screen);

    lv_label_set_text(title, "SYSTEM MONITOR");

    lv_obj_set_style_text_color(
        title, lv_color_white(), LV_PART_MAIN);

    lv_obj_set_style_text_font(
        title, &lv_font_montserrat_14, LV_PART_MAIN);

    lv_obj_align(
        title, LV_ALIGN_TOP_MID, 0, 6);

    // Row 1
    lv_obj_t *uptime_card = create_card(
        screen, 12, 34, 104, 46);

    create_card_title(uptime_card, "UPTIME");
    uptime_label = create_card_value(
        uptime_card, "--:--:--", COLOR_GREEN);

    lv_obj_t *cpu_card = create_card(
        screen, 124, 34, 104, 46);

    create_card_title(cpu_card, "CPU");
    cpu_label = create_card_value(
        cpu_card, "-- MHz", COLOR_GREEN);

    // Row 2
    lv_obj_t *total_heap_card = create_card(
        screen, 12, 87, 104, 46);

    create_card_title(total_heap_card, "TOTAL HEAP");
    total_heap_label = create_card_value(
        total_heap_card, "-- KB", COLOR_BLUE);

    lv_obj_t *free_heap_card = create_card(
        screen, 124, 87, 104, 46);

    create_card_title(free_heap_card, "FREE HEAP");
    free_heap_label = create_card_value(
        free_heap_card, "-- KB", COLOR_BLUE);

    // Row 3
    lv_obj_t *min_heap_card = create_card(
        screen, 12, 140, 104, 46);

    create_card_title(min_heap_card, "MIN FREE HEAP");
    min_heap_label = create_card_value(
        min_heap_card, "-- KB", COLOR_ORANGE);

    lv_obj_t *flash_card = create_card(
        screen, 124, 140, 104, 46);

    create_card_title(flash_card, "FLASH SIZE");
    flash_label = create_card_value(
        flash_card, "-- MB", COLOR_BLUE);

    // Device information
    lv_obj_t *device_card = create_card(
        screen, 12, 194, 216, 94);

    create_card_title(device_card, "DEVICE INFORMATION");

    chip_label = lv_label_create(device_card);

    lv_label_set_text(chip_label, "ESP32");

    lv_obj_set_style_text_color(
        chip_label, lv_color_white(), LV_PART_MAIN);

    lv_obj_set_style_text_font(
        chip_label, &lv_font_montserrat_10, LV_PART_MAIN);

    lv_obj_align(
        chip_label, LV_ALIGN_TOP_LEFT, 9, 25);

    wifi_label = lv_label_create(device_card);

    lv_label_set_text(wifi_label, "Wi-Fi Disconnected");

    lv_obj_set_style_text_color(
        wifi_label, lv_color_hex(COLOR_ORANGE), LV_PART_MAIN);

    lv_obj_set_style_text_font(
        wifi_label, &lv_font_montserrat_10, LV_PART_MAIN);

    lv_obj_align(
        wifi_label, LV_ALIGN_TOP_LEFT, 9, 44);

    light_label = lv_label_create(device_card);

    lv_label_set_text(light_label, "LDR ADC: ----");

    lv_obj_set_style_text_color(
        light_label, lv_color_hex(COLOR_ORANGE), LV_PART_MAIN);

    lv_obj_set_style_text_font(
        light_label, &lv_font_montserrat_10, LV_PART_MAIN);

    lv_obj_align(
        light_label, LV_ALIGN_TOP_LEFT, 9, 64);

    // Initialize ambient light ADC
    pinMode(34, INPUT);
    analogReadResolution(12);
    analogSetPinAttenuation(34, ADC_0db);

    // Initial update
    update_system_info(nullptr);

    // Refresh every second
    lv_timer_create(
        update_system_info, 1000, nullptr);

    return screen;
}
