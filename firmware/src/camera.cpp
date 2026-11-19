#ifdef ENABLE_CAMERA

#include "camera.h"

#include <Arduino.h>
#include <esp_camera.h>

// XIAO ESP32-S3 Sense camera connector pinout (Seeed's CAMERA_MODEL_XIAO_ESP32S3).
// No conflict with the PDM mic (GPIO 41/42) or the user LED (GPIO 21).
#define CAM_PIN_PWDN  -1
#define CAM_PIN_RESET -1
#define CAM_PIN_XCLK  10
#define CAM_PIN_SIOD  40
#define CAM_PIN_SIOC  39
#define CAM_PIN_D7    48
#define CAM_PIN_D6    11
#define CAM_PIN_D5    12
#define CAM_PIN_D4    14
#define CAM_PIN_D3    16
#define CAM_PIN_D2    18
#define CAM_PIN_D1    17
#define CAM_PIN_D0    15
#define CAM_PIN_VSYNC 38
#define CAM_PIN_HREF  47
#define CAM_PIN_PCLK  13

static camera_fb_t *currentFrame = nullptr;

bool initCamera() {
  camera_config_t config = {};
  config.pin_pwdn = CAM_PIN_PWDN;
  config.pin_reset = CAM_PIN_RESET;
  config.pin_xclk = CAM_PIN_XCLK;
  config.pin_sccb_sda = CAM_PIN_SIOD;
  config.pin_sccb_scl = CAM_PIN_SIOC;
  config.pin_d7 = CAM_PIN_D7;
  config.pin_d6 = CAM_PIN_D6;
  config.pin_d5 = CAM_PIN_D5;
  config.pin_d4 = CAM_PIN_D4;
  config.pin_d3 = CAM_PIN_D3;
  config.pin_d2 = CAM_PIN_D2;
  config.pin_d1 = CAM_PIN_D1;
  config.pin_d0 = CAM_PIN_D0;
  config.pin_vsync = CAM_PIN_VSYNC;
  config.pin_href = CAM_PIN_HREF;
  config.pin_pclk = CAM_PIN_PCLK;
  config.xclk_freq_hz = 20000000;
  config.ledc_timer = LEDC_TIMER_0;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.pixel_format = PIXFORMAT_JPEG;
  // VGA keeps uploads ~30–60 KB: enough for face detection and ELA, small
  // enough to survive the marginal Wi-Fi links seen in testing.
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 12;  // 0–63, lower = better quality
  config.fb_count = 1;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;  // always the newest frame, not a stale queued one

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[camera] init failed: 0x%x (is the Sense expansion board attached?)\n", err);
    return false;
  }

  // The first frames after init are often under-exposed while auto-exposure
  // settles; grab and drop a few.
  for (int i = 0; i < 3; i++) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) esp_camera_fb_return(fb);
    delay(100);
  }
  Serial.println("[camera] ready (VGA JPEG)");
  return true;
}

bool captureJpeg(const uint8_t **jpg, size_t *len) {
  releaseFrame();
  currentFrame = esp_camera_fb_get();
  if (!currentFrame) {
    Serial.println("[camera] capture failed");
    return false;
  }
  *jpg = currentFrame->buf;
  *len = currentFrame->len;
  return true;
}

void releaseFrame() {
  if (currentFrame) {
    esp_camera_fb_return(currentFrame);
    currentFrame = nullptr;
  }
}

#endif  // ENABLE_CAMERA
