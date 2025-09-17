#ifndef EMF_SENSOR_H
#define EMF_SENSOR_H

#include <Arduino.h>
#include <driver/adc.h>
#include "emf_config.h"

// =================== SENSOR FUNCTIONS ===================
void init_sensor();
float read_filtered_voltage();
float read_dbm_median();
void update_signal_processing(float dbm_median);
float get_dynamic_threshold();
bool check_trigger_condition();

// =================== UTILITY FUNCTIONS ===================
float adcToVolts(int adc);
float voltsToDbm(float v);
float median5(const float *a);

// =================== GLOBAL VARIABLES ===================
extern float emaV;
extern float dbm_fast, dbm_slow;
extern float delta_db;
extern float mad_est;
extern float dyn_thr;
extern int on_streak;
extern uint32_t last_now_ms;

// Median filter ring buffer
extern float ring_dbm[5];
extern int ring_i;
extern bool ring_full;

// Minute statistics
extern uint32_t minuteCount;
extern double minuteSumDbm;
extern float minuteMinDbm;
extern float minuteMaxDbm;
extern uint32_t lastMinuteTick;

#endif // EMF_SENSOR_H