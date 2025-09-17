#ifndef CAR_DETECTION_H
#define CAR_DETECTION_H

#include <Arduino.h>
#include <parkingpal5478069-project-1_inferencing.h>
// #include "dummy_ei.h"  // Temporary dummy - now using real Edge Impulse
#include "config.h"
#include "../shared/esp_now_comm.h"

// =================== CAR PRESENCE STATE MACHINE ===================
void init_car_detection();
void update_car_detection(float car_conf);
bool is_car_present();
void print_detection_labels();

// =================== INFERENCE PROCESSING ===================
float extract_car_confidence(const ei_impulse_result_t& result);
void print_detection_results(const ei_impulse_result_t& result);

// =================== GLOBAL VARIABLES ===================
extern volatile bool car_present;
extern uint32_t car_above_since_ms;
extern uint32_t car_below_since_ms;

#endif // CAR_DETECTION_H