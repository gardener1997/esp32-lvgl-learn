#include <Arduino.h>
#include <SD.h>
#include <lvgl.h>
#include <stdlib.h>

#include "time_service.h"
#include "screenshot.h"

static constexpr int SCREEN_WIDTH = 240;
static constexpr int SCREEN_HEIGHT = 320;
static constexpr int CAPTURE_BUFFER_LINES = 16;

static constexpr uint32_t BMP_HEADER_SIZE = 54;
static constexpr uint32_t BMP_ROW_SIZE = SCREEN_WIDTH * 3;
static constexpr uint32_t BMP_IMAGE_SIZE =
    BMP_ROW_SIZE * SCREEN_HEIGHT;
static constexpr uint32_t BMP_FILE_SIZE =
    BMP_HEADER_SIZE + BMP_IMAGE_SIZE;

static char screenshot_path[64];

static File screenshot_file;

static bool capture_active = false;
static bool capture_error = false;

static uint8_t *capture_buffer = nullptr;

static int expected_next_y = 0;
static uint32_t capture_flush_count = 0;

static bool create_screenshot_filename()
{
    struct tm timeinfo;

    if (!time_service_get_local_time(&timeinfo)) {
        Serial.println("Screenshot: system time not synchronized");
        return false;
    }

    char timestamp[32];

    strftime(
        timestamp,
        sizeof(timestamp),
        "%Y%m%d_%H%M%S",
        &timeinfo
    );

    snprintf(
        screenshot_path,
        sizeof(screenshot_path),
        "/screenshot_%s.bmp",
        timestamp
    );

    // 防止同一秒内连续截图覆盖。
    int suffix = 1;

    while (SD.exists(screenshot_path)) {
        snprintf(
            screenshot_path,
            sizeof(screenshot_path),
            "/screenshot_%s_%02d.bmp",
            timestamp,
            suffix++
        );
    }

    Serial.printf(
        "Screenshot filename: %s\n",
        screenshot_path
    );

    return true;
}

static void write_u16(uint8_t *buffer, int offset, uint16_t value)
{
    buffer[offset] = value & 0xFF;
    buffer[offset + 1] = (value >> 8) & 0xFF;
}

static void write_u32(uint8_t *buffer, int offset, uint32_t value)
{
    buffer[offset] = value & 0xFF;
    buffer[offset + 1] = (value >> 8) & 0xFF;
    buffer[offset + 2] = (value >> 16) & 0xFF;
    buffer[offset + 3] = (value >> 24) & 0xFF;
}

static bool write_bmp_header()
{
    uint8_t header[BMP_HEADER_SIZE] = {};

    header[0] = 'B';
    header[1] = 'M';

    write_u32(header, 2, BMP_FILE_SIZE);
    write_u32(header, 10, BMP_HEADER_SIZE);
    write_u32(header, 14, 40);

    write_u32(header, 18, SCREEN_WIDTH);

    // Top-down BMP：从上往下保存像素。
    write_u32(header, 22,
              static_cast<uint32_t>(-SCREEN_HEIGHT));

    write_u16(header, 26, 1);
    write_u16(header, 28, 24);

    write_u32(header, 34, BMP_IMAGE_SIZE);
    write_u32(header, 38, 2835);
    write_u32(header, 42, 2835);

    return screenshot_file.write(header, sizeof(header))
        == sizeof(header);
}

static bool prepare_bmp_file()
{
    screenshot_file = SD.open(screenshot_path, FILE_WRITE);

    if (!screenshot_file) {
        return false;
    }

    return write_bmp_header();
}

void screenshot_capture_flush(
    const lv_area_t *area,
    const uint8_t *px_map)
{
    if (!capture_active || capture_error) {
        return;
    }

    int width = area->x2 - area->x1 + 1;
    int height = area->y2 - area->y1 + 1;

    // 要求全宽、连续刷新区域。
    if (area->x1 != 0 ||
        area->x2 != SCREEN_WIDTH - 1 ||
        area->y1 != expected_next_y ||
        width != SCREEN_WIDTH ||
        height <= 0 ||
        height > CAPTURE_BUFFER_LINES ||
        area->y2 >= SCREEN_HEIGHT) {

        Serial.printf(
            "Screenshot: unsupported area "
            "x=%d..%d y=%d..%d\n",
            area->x1, area->x2,
            area->y1, area->y2
        );

        capture_error = true;
        return;
    }

    const uint16_t *pixels =
        reinterpret_cast<const uint16_t *>(px_map);

    // RGB565 转 BGR888，不翻转 X 或 Y。
    for (int row = 0; row < height; row++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            uint16_t color = pixels[row * width + x];

            uint8_t red = (color >> 11) & 0x1F;
            uint8_t green = (color >> 5) & 0x3F;
            uint8_t blue = color & 0x1F;

            int offset = row * BMP_ROW_SIZE + x * 3;

            capture_buffer[offset] =
                (blue << 3) | (blue >> 2);

            capture_buffer[offset + 1] =
                (green << 2) | (green >> 4);

            capture_buffer[offset + 2] =
                (red << 3) | (red >> 2);
        }
    }

    // 整个刷新区域一次性顺序写入。
    uint32_t bytes_to_write = height * BMP_ROW_SIZE;

    if (screenshot_file.write(capture_buffer, bytes_to_write)
        != bytes_to_write) {
        capture_error = true;
        return;
    }

    expected_next_y += height;
    capture_flush_count++;
}

bool screenshot_save_bmp(lv_display_t *display)
{
    if (!display || SD.cardType() == CARD_NONE) {
        Serial.println("Screenshot: display or SD unavailable");
        return false;
    }

    if (capture_active) {
        return false;
    }

    if (!create_screenshot_filename()) {
        return false;
    }

    capture_buffer = static_cast<uint8_t *>(
        malloc(BMP_ROW_SIZE * CAPTURE_BUFFER_LINES)
    );

    if (!capture_buffer) {
        Serial.println("Screenshot: memory allocation failed");
        return false;
    }

    Serial.println("Capturing screen...");

    if (!prepare_bmp_file()) {
        Serial.println("Screenshot: file preparation failed");

        screenshot_file.close();
        free(capture_buffer);
        capture_buffer = nullptr;

        return false;
    }

    capture_error = false;
    capture_flush_count = 0;
    expected_next_y = 0;
    capture_active = true;

    lv_obj_t *screen = lv_display_get_screen_active(display);

    lv_obj_invalidate(screen);
    lv_refr_now(display);

    capture_active = false;

    screenshot_file.flush();
    screenshot_file.close();

    bool success =
        !capture_error &&
        expected_next_y == SCREEN_HEIGHT &&
        capture_flush_count > 0;

    free(capture_buffer);
    capture_buffer = nullptr;

    if (!success) {
        SD.remove(screenshot_path);

        Serial.println("Screenshot failed");
        Serial.printf(
            "Captured rows: %d/%d\n",
            expected_next_y,
            SCREEN_HEIGHT
        );

        return false;
    }

    Serial.printf("Screenshot saved: %s\n", screenshot_path);

    return true;
}

