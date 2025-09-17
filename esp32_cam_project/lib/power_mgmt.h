#ifndef POWER_MGMT_H
#define POWER_MGMT_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_sleep.h>
#include "config.h"

// =================== POWER MANAGEMENT ===================
void idle_low_power();
void enter_deep_sleep(uint32_t sleep_time_ms);
void setup_power_management();

#endif // POWER_MGMT_H