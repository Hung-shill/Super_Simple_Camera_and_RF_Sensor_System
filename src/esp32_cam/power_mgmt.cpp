#include "power_mgmt.h"

// =================== POWER MANAGEMENT FUNCTIONS ===================
void setup_power_management() {
  // Configure power management based on SLEEP_MODE setting
  Serial.printf("[PWR] Sleep mode: %d\n", SLEEP_MODE);

#if SLEEP_MODE == 0
  Serial.println("[PWR] Using safe modem sleep mode (recommended)");
#else
  Serial.println("[PWR] Using light sleep mode");
#endif
}

void idle_low_power() {
#if SLEEP_MODE == 0
  // Safe modem-sleep (recommended)
  WiFi.setSleep(true);
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
  delay(5);  // yield to allow other tasks
#else
  // Light sleep mode
  WiFi.setSleep(true);
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
  esp_sleep_enable_timer_wakeup(120000);  // ~120 ms
  esp_light_sleep_start();
#endif
}

void enter_deep_sleep(uint32_t sleep_time_ms) {
  Serial.printf("[PWR] Entering deep sleep for %u ms\n", sleep_time_ms);
  Serial.flush();

  esp_sleep_enable_timer_wakeup(sleep_time_ms * 1000); // Convert to microseconds
  esp_deep_sleep_start();
}