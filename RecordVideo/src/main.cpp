#include <Arduino.h>
#include "esp_camera.h"
#include <SPI.h>
#include <SD.h>

#define SD_CS 21
#define LED_PIN 14

camera_config_t config;

void deleteAllFiles() {

    File root = SD.open("/");

    while (true) {

        File entry = root.openNextFile();
        if (!entry) break;

        String path = "/" + String(entry.name());
        entry.close();

        SD.remove(path);
    }

    root.close();
}

void setup()
{
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);
    delay(2000);

    Serial.println("Starting");

    // ================= CAMERA CONFIG =================

    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;

    config.pin_d0 = 4;
    config.pin_d1 = 5;
    config.pin_d2 = 6;
    config.pin_d3 = 7;
    config.pin_d4 = 15;
    config.pin_d5 = 16;
    config.pin_d6 = 17;
    config.pin_d7 = 18;

    config.pin_xclk = -1;

    config.pin_pclk = 10;
    config.pin_vsync = 8;
    config.pin_href = 9;

    config.pin_sscb_sda = 38;
    config.pin_sscb_scl = 39;

    config.pin_pwdn = 41;
    config.pin_reset = 40;

    config.xclk_freq_hz = 24000000;
    config.pixel_format = PIXFORMAT_JPEG;

    // ===== СТАБИЛЬНЫЕ НАСТРОЙКИ =====

    config.frame_size = FRAMESIZE_SXGA; // 1600x1200
    config.jpeg_quality = 6;
    config.fb_count = 1;

    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_LATEST;

    if (esp_camera_init(&config) != ESP_OK)
    {
        Serial.println("Camera init failed");
        while (true);
    }

    Serial.println("Camera OK");

    // ===== ПРОГРЕВ СЕНСОРА =====

    delay(2000);

    for (int i = 0; i < 5; i++)
    {
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) esp_camera_fb_return(fb);
    }

    // ================= SD =================

    SPI.begin(12, 13, 11);

    if (!SD.begin(SD_CS))
    {
        Serial.println("SD init failed");
        while (true);
    }

    Serial.println("SD OK");

    deleteAllFiles();

    delay(2000);

    // ================= PHOTO LOOP =================

    for (int i = 1; i <= 10; i++)
    {
        Serial.printf("Taking photo %d\n", i);

        // flush старый кадр
        camera_fb_t *fb = esp_camera_fb_get();
        esp_camera_fb_return(fb);

        delay(100);

        fb = esp_camera_fb_get();

        if (!fb)
        {
            Serial.println("Capture failed");
            continue;
        }

        String path = "/photo" + String(i) + ".jpg";

        File file = SD.open(path, FILE_WRITE);

        if (file)
        {
            file.write(fb->buf, fb->len);
            file.close();

            Serial.println("Saved " + path);
            digitalWrite(LED_PIN, LOW);
            delay(200);
           digitalWrite(LED_PIN, HIGH);
        }
        else
        {
            Serial.println("File open failed");
        }

        esp_camera_fb_return(fb);

        delay(1800);
    }

    Serial.println("Done");
    digitalWrite(LED_PIN, LOW);
     delay(1000);
    digitalWrite(LED_PIN, HIGH);
}

void loop()
{
}