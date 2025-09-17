#ifndef CAMERA_EI_H
#define CAMERA_EI_H

#include <Arduino.h>
#include "esp_camera.h"
#include <parkingpal5478069-project-1_inferencing.h>
#include <edge-impulse-sdk/dsp/image/image.hpp>
// #include "dummy_ei.h"  // Temporary dummy - now using real Edge Impulse
#include "config.h"

// =================== CAMERA CONTROL ===================
bool ei_camera_init();
void ei_camera_deinit();
bool ei_camera_capture();

// =================== IMAGE PROCESSING ===================
void resize_gray_nn(const uint8_t* src, int sw, int sh, uint8_t* dst, int dw, int dh);
int ei_camera_get_data(size_t offset, size_t length, float* out_ptr);

// =================== GLOBAL VARIABLES ===================
extern bool cam_inited;
extern bool ei_debug_nn;
extern uint8_t* ei_input_gray;
extern camera_config_t camcfg;

// =================== LED CONTROL ===================
static inline void led_on()  { digitalWrite(LED_PIN, LED_ACTIVE_HIGH ? HIGH : LOW); }
static inline void led_off() { digitalWrite(LED_PIN, LED_ACTIVE_HIGH ? LOW  : HIGH); }

#endif // CAMERA_EI_H