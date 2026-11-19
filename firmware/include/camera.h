#ifndef CAMERA_H
#define CAMERA_H

#include <cstddef>
#include <cstdint>

// OV2640/OV3660 on the XIAO ESP32-S3 Sense expansion board. Only compiled
// into the glasses_camera build (-DENABLE_CAMERA); the default voice-only
// build never touches the camera.

bool initCamera();

// Captures one JPEG frame. On success returns true and sets *jpg/*len to a
// frame buffer that stays valid until releaseFrame() — call it once the
// upload is done. Frames live only in RAM for the request that uses them,
// never written to flash (fresh-analysis rule, see CLAUDE.md).
bool captureJpeg(const uint8_t **jpg, size_t *len);
void releaseFrame();

#endif
