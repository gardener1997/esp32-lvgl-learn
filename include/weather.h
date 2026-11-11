#pragma once

#include <lvgl.h>

// Create the weather page.
lv_obj_t *create_weather_page();

// Start the background weather fetching task.
void start_weather_service();
