#include "emf_sensor.h"
#include <math.h>

// =================== GLOBAL VARIABLES ===================
// AD8317 calibration
float slope_V_per_dB = -0.022f;          // −22 mV/dB
float intercept_dBm  =  15.0f;           // +15 dBm

// Signal processing variables
float emaV = NAN;
float dbm_fast = NAN, dbm_slow = NAN;
float delta_db = 0.0f;
float mad_est = 0.0f;
float dyn_thr = MIN_FLOOR_DB;
int on_streak = 0;
uint32_t last_now_ms = 0;

// Median filter (5-point) over dBm
float ring_dbm[5];
int ring_i = 0;
bool ring_full = false;

// Minute statistics
uint32_t minuteCount  = 0;
double   minuteSumDbm = 0.0;
float    minuteMinDbm = 1e9;
float    minuteMaxDbm = -1e9;
uint32_t lastMinuteTick = 0;

// =================== UTILITY FUNCTIONS ===================
float adcToVolts(int adc) {
  return (adc * VREF) / ADC_MAX;
}

float voltsToDbm(float v) {
  return (v / slope_V_per_dB) + intercept_dBm;
}

float median5(const float *a) {
  float b[5];
  memcpy(b, a, 5*sizeof(float));
  // Simple insertion sort
  for (int i=1; i<5; i++) {
    float key = b[i];
    int j = i-1;
    while(j >= 0 && b[j] > key) {
      b[j+1] = b[j];
      j--;
    }
    b[j+1] = key;
  }
  return b[2]; // Return median (middle element)
}

// =================== SENSOR FUNCTIONS ===================
void init_sensor() {
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_VOUT, ADC_ATTEN);

  // Initialize timing
  lastMinuteTick = millis();

  Serial.println("EMF sensor initialized");
}

float read_filtered_voltage() {
  // Oversample for noise reduction
  const int N = 64;
  uint32_t acc = 0;
  for (int i = 0; i < N; ++i) {
    acc += analogRead(PIN_VOUT);
    delayMicroseconds(300);
  }
  int adc = acc / N;
  float v = adcToVolts(adc);

  // Apply EMA filter to voltage
  if (isnan(emaV)) emaV = v;
  emaV = (1 - ALPHA_V) * emaV + ALPHA_V * v;

  return v;
}

float read_dbm_median() {
  float v = read_filtered_voltage();
  float dbm_raw = voltsToDbm(v);

  // Apply median filter on dBm
  ring_dbm[ring_i++] = dbm_raw;
  if (ring_i == 5) {
    ring_i = 0;
    ring_full = true;
  }

  return ring_full ? median5(ring_dbm) : dbm_raw;
}

void update_signal_processing(float dbm_median) {
  // Fast/slow EMA on median dBm
  if (isnan(dbm_fast)) {
    dbm_fast = dbm_median;
    dbm_slow = dbm_median;
  }
  dbm_fast = (1.0f - A_FAST) * dbm_fast + A_FAST * dbm_median;
  dbm_slow = (1.0f - A_SLOW) * dbm_slow + A_SLOW * dbm_median;
  delta_db = fabsf(dbm_fast - dbm_slow);

  // MAD proxy (abs dev from slow baseline) → adaptive threshold
  float dev = fabsf(dbm_median - dbm_slow);
  mad_est = (1.0f - ALPHA_MAD) * mad_est + ALPHA_MAD * dev;
  dyn_thr = max(MIN_FLOOR_DB, K_NOISE * mad_est);

  // Update minute statistics
  minuteCount++;
  minuteSumDbm += dbm_median;
  if (dbm_median < minuteMinDbm) minuteMinDbm = dbm_median;
  if (dbm_median > minuteMaxDbm) minuteMaxDbm = dbm_median;
}

float get_dynamic_threshold() {
  return dyn_thr;
}

bool check_trigger_condition() {
  // Rising-edge streak logic using dynamic threshold
  if (delta_db >= dyn_thr) {
    on_streak++;
  } else {
    on_streak = 0;
  }

  return (on_streak >= N_ON);
}