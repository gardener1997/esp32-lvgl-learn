#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

#include "sd_storage.h"

static constexpr int SD_CS = 5;
static constexpr int SD_SCK = 18;
static constexpr int SD_MISO = 19;
static constexpr int SD_MOSI = 23;

static SPIClass sd_spi(HSPI);

bool init_sd_card()
{
    Serial.println("Initializing SD card...");

    sd_spi.begin(
        SD_SCK,
        SD_MISO,
        SD_MOSI,
        SD_CS
    );

    if (!SD.begin(SD_CS, sd_spi, 10000000)) {
        Serial.println("SD card initialization failed!");
        return false;
    }

    Serial.println("SD card initialized!");

    Serial.printf(
        "Card Size: %llu MB\n",
        SD.cardSize() / (1024ULL * 1024ULL)
    );

    return true;
}

bool test_sd_card()
{
    File file = SD.open("/test.txt", FILE_WRITE);

    if (!file) {
        Serial.println("Failed to create test file!");
        return false;
    }

    file.println("Hello from ESP32!");
    file.close();

    file = SD.open("/test.txt", FILE_READ);

    if (!file) {
        Serial.println("Failed to read test file!");
        return false;
    }

    Serial.println("File content:");

    while (file.available()) {
        Serial.write(file.read());
    }

    file.close();

    Serial.println();
    Serial.println("SD card test successful!");

    return true;
}
