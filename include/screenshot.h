#pragma once

#include <lvgl.h>

bool screenshot_save_bmp(lv_display_t *display);

void screenshot_capture_flush(
    const lv_area_t *area,
    const uint8_t *px_map
);
