
#include <Arduino.h>
#include <lvgl.h>

#include "pages.h"
#include "settings.h"

#define BACKLIGHT_PIN 21

static lv_obj_t *brightness_label = nullptr;
static lv_obj_t *brightness_slider = nullptr;

static lv_obj_t *refresh_label = nullptr;
static lv_obj_t *refresh_buttons[4] = {
    nullptr, nullptr, nullptr, nullptr
};

struct RefreshOption {
    uint32_t interval;
    const char *label;
};

static const RefreshOption refresh_options[] = {
    {500, "0.5s"},
    {1000, "1s"},
    {2000, "2s"},
    {5000, "5s"}
};

// Set screen brightness: 0-100
static void set_brightness(int brightness)
{
    brightness = constrain(brightness, 0, 100);

    int pwm_value = map(brightness, 0, 100, 0, 255);

    analogWrite(BACKLIGHT_PIN, pwm_value);
}

// Brightness slider event
static void brightness_changed(lv_event_t *event)
{
    lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(event);

    int brightness = lv_slider_get_value(slider);

    set_brightness(brightness);

    char text[32];
    snprintf(text, sizeof(text), "Brightness: %d%%", brightness);

    lv_label_set_text(brightness_label, text);

    // Save only when the user releases the slider.
    if (lv_event_get_code(event) == LV_EVENT_RELEASED) {
        save_brightness(brightness);
    }
}

// Update refresh interval button appearance
static void update_refresh_buttons()
{
    uint32_t current_interval =
        get_temperature_refresh_interval();

    for (int i = 0; i < 4; i++) {
        bool selected =
            refresh_options[i].interval == current_interval;

        lv_obj_set_style_bg_color(
            refresh_buttons[i],
            lv_color_hex(selected ? 0x287A9A : 0x344653),
            LV_PART_MAIN
        );
    }

    char text[40];

    snprintf(
        text,
        sizeof(text),
        "Refresh interval: %s",
        current_interval == 500 ? "0.5s" :
        current_interval == 1000 ? "1s" :
        current_interval == 2000 ? "2s" : "5s"
    );

    lv_label_set_text(refresh_label, text);
}

// Refresh interval button event
static void refresh_interval_changed(lv_event_t *event)
{
    const RefreshOption *option =
        static_cast<const RefreshOption *>(
            lv_event_get_user_data(event)
        );

    set_temperature_refresh_interval(option->interval);

    update_refresh_buttons();
}

static lv_obj_t *create_card(
    lv_obj_t *parent,
    int x,
    int y,
    int width,
    int height)
{
    lv_obj_t *card = lv_obj_create(parent);

    lv_obj_set_size(card, width, height);
    lv_obj_set_pos(card, x, y);

    lv_obj_set_style_bg_color(
        card, lv_color_hex(0x1D2935), LV_PART_MAIN);

    lv_obj_set_style_border_color(
        card, lv_color_hex(0x304050), LV_PART_MAIN);

    lv_obj_set_style_border_width(
        card, 1, LV_PART_MAIN);

    lv_obj_set_style_radius(
        card, 8, LV_PART_MAIN);

    lv_obj_set_style_pad_all(
        card, 12, LV_PART_MAIN);

    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    return card;
}

lv_obj_t *create_settings_page()
{
    lv_obj_t *screen = lv_obj_create(NULL);

    lv_obj_set_style_bg_color(
        screen, lv_color_hex(0x101820), LV_PART_MAIN);

    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    // Page title
    lv_obj_t *title = lv_label_create(screen);

    lv_label_set_text(title, "SETTINGS");

    lv_obj_set_style_text_color(
        title, lv_color_white(), LV_PART_MAIN);

    lv_obj_set_style_text_font(
        title, &lv_font_montserrat_14, LV_PART_MAIN);

    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    // Display card
    lv_obj_t *display_card = create_card(
        screen, 12, 48, 216, 105);

    lv_obj_t *display_title = lv_label_create(display_card);

    lv_label_set_text(display_title, "DISPLAY");

    lv_obj_set_style_text_color(
        display_title, lv_color_hex(0x4FC3F7), LV_PART_MAIN);

    lv_obj_set_style_text_font(
        display_title, &lv_font_montserrat_14, LV_PART_MAIN);

    lv_obj_align(display_title, LV_ALIGN_TOP_LEFT, 0, 0);

    // Load saved brightness
    DeviceSettings settings = load_settings();

    char brightness_text[32];

    snprintf(
        brightness_text,
        sizeof(brightness_text),
        "Brightness: %d%%",
        settings.brightness
        );

    // Brightness label
    brightness_label = lv_label_create(display_card);

    lv_label_set_text(brightness_label, brightness_text);

    lv_obj_set_style_text_color(
        brightness_label, lv_color_hex(0xAAB8C2), LV_PART_MAIN);

    lv_obj_set_style_text_font(
        brightness_label, &lv_font_montserrat_12, LV_PART_MAIN);

    lv_obj_align(brightness_label, LV_ALIGN_TOP_LEFT, 0, 28);

    // Brightness slider
    brightness_slider = lv_slider_create(display_card);

    lv_obj_set_width(brightness_slider, 185);

    lv_slider_set_range(brightness_slider, 0, 100);

    lv_slider_set_value(
        brightness_slider,
        settings.brightness,
        LV_ANIM_OFF
        );

    lv_obj_align(brightness_slider, LV_ALIGN_TOP_LEFT, 0, 58);

    lv_obj_set_style_bg_color(
        brightness_slider, lv_color_hex(0x304050), LV_PART_MAIN);

    lv_obj_set_style_bg_color(
        brightness_slider, lv_color_hex(0x4FC3F7), LV_PART_INDICATOR);

    lv_obj_set_style_bg_color(
        brightness_slider, lv_color_white(), LV_PART_KNOB);

    lv_obj_add_event_cb(
        brightness_slider,
        brightness_changed,
        LV_EVENT_VALUE_CHANGED,
        NULL
        );

    lv_obj_add_event_cb(
        brightness_slider,
        brightness_changed,
        LV_EVENT_RELEASED,
        NULL
        );

    // Temperature card
    lv_obj_t *temperature_card = create_card(
        screen, 12, 165, 216, 100);

    lv_obj_t *temperature_title = lv_label_create(temperature_card);

    lv_label_set_text(temperature_title, "TEMPERATURE");

    lv_obj_set_style_text_color(
        temperature_title, lv_color_hex(0x4FC3F7), LV_PART_MAIN);

    lv_obj_set_style_text_font(
        temperature_title, &lv_font_montserrat_14, LV_PART_MAIN);

    lv_obj_align(temperature_title, LV_ALIGN_TOP_LEFT, 0, 0);

    // Current refresh interval
    refresh_label = lv_label_create(temperature_card);

    lv_obj_set_style_text_color(
        refresh_label, lv_color_hex(0xAAB8C2), LV_PART_MAIN);

    lv_obj_set_style_text_font(
        refresh_label, &lv_font_montserrat_12, LV_PART_MAIN);

    lv_obj_align(refresh_label, LV_ALIGN_TOP_LEFT, 0, 27);

    // Refresh interval buttons
    for (int i = 0; i < 4; i++) {
        lv_obj_t *button = lv_button_create(temperature_card);

        refresh_buttons[i] = button;

        lv_obj_set_size(button, 43, 25);

        lv_obj_align(
            button,
            LV_ALIGN_TOP_LEFT,
            i * 49,
            48
        );

        lv_obj_set_style_radius(
            button, 5, LV_PART_MAIN);

        lv_obj_set_style_bg_color(
            button, lv_color_hex(0x344653), LV_PART_MAIN);

        lv_obj_t *label = lv_label_create(button);

        lv_label_set_text(label, refresh_options[i].label);

        lv_obj_set_style_text_font(
            label, &lv_font_montserrat_12, LV_PART_MAIN);

        lv_obj_center(label);

        lv_obj_add_event_cb(
            button,
            refresh_interval_changed,
            LV_EVENT_CLICKED,
            (void *)&refresh_options[i]
        );
    }

    // Initialize brightness
    set_brightness(settings.brightness);

    update_refresh_buttons();

    return screen;
}

