#include "camera_ei.h"

// =================== GLOBAL VARIABLES ===================
bool cam_inited = false;
bool ei_debug_nn = false;
uint8_t* ei_input_gray = nullptr;

// Camera configuration (GRAYSCALE + small frame)
camera_config_t camcfg = {
  .pin_pwdn  = PWDN_GPIO_NUM,
  .pin_reset = RESET_GPIO_NUM,
  .pin_xclk  = XCLK_GPIO_NUM,
  .pin_sscb_sda = SIOD_GPIO_NUM,
  .pin_sscb_scl = SIOC_GPIO_NUM,
  .pin_d7 = Y9_GPIO_NUM, .pin_d6 = Y8_GPIO_NUM,
  .pin_d5 = Y7_GPIO_NUM, .pin_d4 = Y6_GPIO_NUM,
  .pin_d3 = Y5_GPIO_NUM, .pin_d2 = Y4_GPIO_NUM,
  .pin_d1 = Y3_GPIO_NUM, .pin_d0 = Y2_GPIO_NUM,
  .pin_vsync = VSYNC_GPIO_NUM, .pin_href = HREF_GPIO_NUM, .pin_pclk = PCLK_GPIO_NUM,
  .xclk_freq_hz = CAM_XCLK_HZ,
  .ledc_timer   = LEDC_TIMER_0,
  .ledc_channel = LEDC_CHANNEL_0,
  .pixel_format = PIXFORMAT_GRAYSCALE,  // grayscale output
  .frame_size   = CAM_FRAMESIZE,        // small frame
  .jpeg_quality = 12,                   // ignored for GRAYSCALE
  .fb_count     = CAM_FB_COUNT,
  .fb_location  = CAMERA_FB_IN_PSRAM,
  .grab_mode    = CAMERA_GRAB_WHEN_EMPTY,
};

// =================== CAMERA CONTROL ===================
bool ei_camera_init() {
  if (cam_inited) return true;
  delay(3000);  // settle before init
  esp_err_t err = esp_camera_init(&camcfg);
  if (err != ESP_OK) {
    Serial.printf("esp_camera_init err=0x%x\n", err);
    return false;
  }
  cam_inited = true;
  return true;
}

void ei_camera_deinit() {
  if (!cam_inited) return;
  esp_err_t err = esp_camera_deinit();
  if (err != ESP_OK) Serial.println("Camera deinit failed");
  cam_inited = false;
}

// =================== IMAGE PROCESSING ===================
void resize_gray_nn(const uint8_t* src, int sw, int sh, uint8_t* dst, int dw, int dh) {
  for (int y = 0; y < dh; ++y) {
    int sy = (int)((uint32_t)y * sh / dh);
    const uint8_t* srow = src + sy * sw;
    uint8_t* drow = dst + y * dw;
    for (int x = 0; x < dw; ++x) {
      int sx = (int)((uint32_t)x * sw / dw);
      drow[x] = srow[sx];
    }
  }
}

/*
 * Capture one frame (GRAYSCALE small):
 *  - fb = GRAYSCALE (Wsrc x Hsrc, 1 Bpp)
 *  - If (Wsrc,Hsrc) == (EI_W,EI_H) copy directly.
 *  - Else resize (nearest) fb->buf → ei_input_gray.
 */
bool ei_camera_capture() {
  if (!cam_inited || !ei_input_gray) return false;

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) return false;

  bool ok = false;

  if (fb->format == PIXFORMAT_GRAYSCALE) {
    const int sw = fb->width;
    const int sh = fb->height;
    const int dw = (int)EI_CLASSIFIER_INPUT_WIDTH;
    const int dh = (int)EI_CLASSIFIER_INPUT_HEIGHT;

    if ((size_t)fb->len < (size_t)sw * sh) {
      esp_camera_fb_return(fb);
      return false;
    }

    if (sw == dw && sh == dh) {
      // Direct copy, no resize
      memcpy(ei_input_gray, fb->buf, (size_t)sw * sh);
      ok = true;
    } else {
      // Resize directly from camera buffer → EI buffer
      resize_gray_nn((const uint8_t*)fb->buf, sw, sh, ei_input_gray, dw, dh);
      ok = true;
    }
  }

  esp_camera_fb_return(fb);
  return ok;
}

// =================== EDGE IMPULSE DATA INTERFACE ===================
// EI expects packed 0xRRGGBB per pixel.
// We read gray from ei_input_gray and pack R=G=B=gray.
int ei_camera_get_data(size_t offset, size_t length, float* out_ptr) {
  const uint8_t* src = ei_input_gray + offset;  // 1 byte per pixel
  for (size_t i = 0; i < length; i++) {
    uint8_t g = src[i];
    out_ptr[i] = (float)((g << 16) | (g << 8) | g);
  }
  return 0;
}