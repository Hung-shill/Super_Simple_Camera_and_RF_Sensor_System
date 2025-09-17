/*
 * ESP32-CAM (AI-Thinker) + Edge Impulse + ESP-NOW
 * - RX: listens for trigger packets (wakes camera for ACTIVE_WINDOW_MS)
 * - Inference on small GRAYSCALE frames
 * - TX: sends OCCUPIED/VACANT status on state changes to receiver's MAC
 *
 * Core: Arduino-ESP32 2.0.4
 *
 * Car present if "car" confidence >= 0.70 for 5 s; clears if <= 0.50 for 3 s.
 *
 * REORGANIZED VERSION - Now using PlatformIO structure
 */

#include <Arduino.h>
#include <parkingpal5478069-project-1_inferencing.h>

// ===== Project Modules =====
#include "config.h"
#include "esp_now_comm.h"  // From shared library
#include "camera_ei.h"
#include "car_detection.h"
#include "power_mgmt.h"

// =================== GLOBAL VARIABLES ===================
// (Now defined in respective modules)

// =================== ARDUINO SETUP ===================

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\nESP32-CAM + EI + ESP-NOW (PlatformIO VERSION)");

  // Initialize LED pin
  pinMode(LED_PIN, OUTPUT);
  led_off();

  // Initialize power management
  setup_power_management();

  // Allocate EI input buffer (GRAY)
  delay(3000);
  size_t sz_ei = (size_t)EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT; // 1 Bpp gray
  ei_input_gray = (uint8_t*)ps_malloc(sz_ei);
  if (!ei_input_gray) ei_input_gray = (uint8_t*)heap_caps_malloc(sz_ei, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!ei_input_gray) ei_input_gray = (uint8_t*)heap_caps_malloc(sz_ei, MALLOC_CAP_DEFAULT);

  if (!ei_input_gray) {
    Serial.printf("ERR: EI buffer alloc failed (%u bytes)\n", (unsigned)sz_ei);
  } else {
    Serial.printf("EI gray buffer: %p (%u bytes)\n", (void*)ei_input_gray, (unsigned)sz_ei);
  }

  // Initialize car detection state machine
  init_car_detection();

  // Initialize ESP-NOW communication
  if (!init_now_rx_tx()) {
    Serial.println("WARN: ESP-NOW RX/TX not initialized");
  }

  Serial.println("System ready. Waiting for ESP-NOW trigger...");
}

void loop() {
  static uint32_t frame_idx = 0;
  const uint32_t now = millis();

  // Check if we're in active window (triggered by ESP-NOW)
  if ((int32_t)(g_trigger_until_ms - now) > 0) {
    // Initialize camera if not already done
    if (!cam_inited) {
      if (!ei_camera_init()) {
        Serial.println("Camera init failed");
        g_trigger_until_ms = 0;
        led_off();
        return;
      }
      Serial.println("Camera ON");
      print_detection_labels(); // Print labels for classification models
    }

    // Build EI signal view reading from ei_input_gray
    ei::signal_t signal;
    signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
    signal.get_data = &ei_camera_get_data;

    // Capture and process frame
    if (!ei_camera_capture()) {
      Serial.println("Capture failed");
      delay(20);
      return;
    }

    // Run Edge Impulse classifier
    ei_impulse_result_t result = { 0 };
    EI_IMPULSE_ERROR e = run_classifier(&signal, &result, ei_debug_nn);
    if (e != EI_IMPULSE_OK) {
      Serial.printf("Classifier err=%d\n", e);
      delay(10);
      return;
    }

    // Print timing information
    Serial.printf("[#%lu] DSP=%dms CLS=%dms ANOM=%dms\n",
                  (unsigned long)++frame_idx,
                  result.timing.dsp, result.timing.classification, result.timing.anomaly);

    // Extract car confidence and update state machine
    float car_conf = extract_car_confidence(result);
    update_car_detection(car_conf);

    // Print detection results (bounding boxes for object detection)
    print_detection_results(result);

  } else {
    // Active window expired - turn off camera and enter low power mode
    if (cam_inited) {
      ei_camera_deinit();
      Serial.println("Camera OFF");
      led_off();
    }
    idle_low_power();
  }
}

// =================== SENSOR VALIDATION ===================
#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_CAMERA
  #error "Invalid model for current sensor"
#endif