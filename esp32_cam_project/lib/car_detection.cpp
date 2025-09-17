#include "car_detection.h"

// =================== GLOBAL VARIABLES ===================
volatile bool car_present = false;   // public flag you can read elsewhere
uint32_t car_above_since_ms = 0;     // first time we saw >= ON threshold
uint32_t car_below_since_ms = 0;     // first time we saw <= OFF threshold

// =================== INITIALIZATION ===================
void init_car_detection() {
  car_present = false;
  car_above_since_ms = 0;
  car_below_since_ms = 0;
}

// =================== CAR PRESENCE STATE MACHINE ===================
void update_car_detection(float car_conf) {
  const uint32_t now_ms = millis();

  Serial.printf("[EI] car_conf=%.2f  present=%d\n", car_conf, (int)car_present);

  // --- Rising to OCCUPIED (>=threshold for hold time) ---
  if (car_conf >= CAR_ON_THRESH) {
    if (car_above_since_ms == 0) car_above_since_ms = now_ms;
    car_below_since_ms = 0;

    if (!car_present && (now_ms - car_above_since_ms) >= CAR_ON_HOLD_MS) {
      car_present = true;
      Serial.println("[STATE] CAR → OCCUPIED (>=70% for 5s)");
      now_send_status(/*occupied=*/1, /*conf=*/car_conf);
    }
  } else {
    // --- Falling toward VACANT ---
    car_above_since_ms = 0;

    if (car_present) {
      if (car_conf <= CAR_OFF_THRESH) {
        if (car_below_since_ms == 0) car_below_since_ms = now_ms;

        if ((now_ms - car_below_since_ms) >= CAR_OFF_HOLD_MS) {
          car_present = false;
          car_below_since_ms = 0;
          Serial.println("[STATE] CAR → VACANT (<=50% for 3s)");
          now_send_status(/*occupied=*/0, /*conf=*/car_conf);
        }
      } else {
        car_below_since_ms = 0;
      }
    }
  }
}

// =================== INFERENCE PROCESSING ===================
float extract_car_confidence(const ei_impulse_result_t& result) {
  float car_conf = 0.f;

#if EI_CLASSIFIER_OBJECT_DETECTION == 1
  // Object detection project - look for "car" bounding boxes
  for (uint32_t i = 0; i < result.bounding_boxes_count; i++) {
    const auto &bb = result.bounding_boxes[i];
    if (bb.value == 0) continue;
    if (strcmp(bb.label, "car") == 0 && bb.value > car_conf) {
      car_conf = bb.value;
    }
  }
#else
  // Classification project - look for "car" class
  for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    if (strcmp(ei_classifier_inferencing_categories[i], "car") == 0) {
      car_conf = result.classification[i].value;
      break;
    }
  }
#endif

  return car_conf;
}

void print_detection_results(const ei_impulse_result_t& result) {
#if EI_CLASSIFIER_OBJECT_DETECTION == 1
  // Print bounding boxes for object detection
  for (uint32_t i = 0; i < result.bounding_boxes_count; i++) {
    const auto &bb = result.bounding_boxes[i];
    if (bb.value == 0) continue;
    Serial.printf("BB %s (%.2f) [x:%u y:%u w:%u h:%u]\n",
                  bb.label, bb.value, bb.x, bb.y, bb.width, bb.height);
  }
#endif
}

void print_detection_labels() {
#if EI_CLASSIFIER_OBJECT_DETECTION != 1
  // Print labels once for classification models
  Serial.print("Labels: ");
  for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    Serial.print(ei_classifier_inferencing_categories[i]);
    if (i + 1 < EI_CLASSIFIER_LABEL_COUNT) Serial.print(", ");
  }
  Serial.println();
#endif
}

// =================== PUBLIC INTERFACE ===================
bool is_car_present() {
  return car_present;
}