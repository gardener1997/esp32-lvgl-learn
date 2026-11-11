#pragma once

#include <lvgl.h>
#include <stdint.h>

// Page creation
lv_obj_t *create_temperature_page();
lv_obj_t *create_system_page();
lv_obj_t *create_settings_page();
lv_obj_t *create_weather_page();

// Temperature page updates
void update_temperature_page(float temperature, bool connected);
void reset_temperature_stats();

// Temperature refresh interval
void set_temperature_refresh_interval(uint32_t interval_ms);
uint32_t get_temperature_refresh_interval();

