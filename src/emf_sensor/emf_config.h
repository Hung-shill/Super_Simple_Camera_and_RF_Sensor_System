#ifndef EMF_CONFIG_H
#define EMF_CONFIG_H

// =================== NETWORK CONFIGURATION ===================
constexpr const char* WIFI_SSID = "Dunsmuir";
constexpr const char* WIFI_PASS = "N3w-F1@_2025!";
constexpr const char* AP_SSID   = "AD8317-Logger";
constexpr const char* AP_PASS   = "esp32logger";

// =================== RF SENSOR CONFIGURATION ===================
constexpr int   PIN_VOUT   = 34;
constexpr float VREF       = 3.3f;
constexpr int   ADC_MAX    = 4095;
constexpr adc_attenuation_t ADC_ATTEN = ADC_11db;
constexpr uint32_t PERIOD_MS = 1000;     // 1 Hz sampling

// AD8317 calibration (tune these!)
extern float slope_V_per_dB;
extern float intercept_dBm;

// =================== SIGNAL PROCESSING PARAMETERS ===================
// Voltage EMA smoothing
constexpr float ALPHA_V = 0.2f;

// Fast/slow EMA on median-filtered dBm
constexpr float A_FAST = 0.50f;          // faster foreground
constexpr float A_SLOW = 0.005f;         // slower baseline

// Adaptive threshold parameters
constexpr float ALPHA_MAD   = 0.05f;     // MAD update speed
constexpr float K_NOISE     = 7.0f;      // how many MADs above noise
constexpr float MIN_FLOOR_DB= 0.70f;     // absolute floor (dB)

// Rising-edge trigger timing
constexpr int N_ON = (int)(5000UL / PERIOD_MS);  // ~5 seconds

// ESP-NOW cooldown
constexpr uint32_t NOW_COOLDOWN_MS = 30000;      // 30 second cooldown

// Web interface timing
constexpr uint32_t INTERACTIVE_MS = 5000;        // Interactive mode timeout

// =================== FILE PATHS ===================
constexpr const char* CSV_PATH = "/ad8317_log.csv";

#endif // EMF_CONFIG_H