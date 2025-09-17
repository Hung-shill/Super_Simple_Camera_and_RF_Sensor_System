/*
 * EMF RF Sensor + ESP32-CAM Communication System
 * REORGANIZED VERSION using modular structure
 *
 * This system:
 * 1. Monitors RF levels using AD8317 sensor
 * 2. Triggers ESP32-CAM via ESP-NOW when RF activity detected
 * 3. Receives car detection status from ESP32-CAM
 * 4. Provides web interface for monitoring
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_bt.h>
#include <time.h>

// ===== Project Modules =====
#include "emf_config.h"
#include "emf_sensor.h"
#include "emf_web.h"
#include "../shared/esp_now_comm.h"  // Shared with ESP32-CAM

// =================== MAIN SYSTEM LOGIC ===================
void setup() {
  delay(3000);
  Serial.begin(115200);
  delay(100);
  Serial.println("\nEMF RF Sensor + ESP32-CAM System (REORGANIZED VERSION)");

  // Initialize sensor
  init_sensor();

  // Initialize file system and logging
  init_logging();

  // Initialize WiFi (STA first, AP fallback)
  setup_wifi();

  // Initialize time synchronization
  setup_time_sync();

  // Initialize ESP-NOW for camera communication
  if (!init_now_rx_tx()) {
    Serial.println("ESP-NOW unavailable; continuing without camera communication");
  }

  // Initialize web server
  init_web_server();

  Serial.println("EMF system ready!");
  print_connection_info();
}

void loop() {
  uint32_t t0 = millis();

  // Read and process RF sensor data
  float dbm_median = read_dbm_median();
  update_signal_processing(dbm_median);

  // Check for trigger condition
  if (check_trigger_condition()) {
    Serial.printf("AUTO trigger: Δ=%.3f ≥ dyn_thr=%.3f for %d s\n",
                  delta_db, get_dynamic_threshold(), N_ON);
    send_camera_trigger(delta_db);
  }

  // Generate time and logging data
  uint32_t epochNow;
  String iso = isoTimeNow(&epochNow);
  uint32_t uptimeMs = millis() - bootMs;

  // Calculate minute statistics
  double minuteAvg = (minuteCount ? minuteSumDbm / minuteCount : NAN);

  // Log data to CSV
  bool trigger_ready = check_trigger_condition();
  String noteToWrite = pendingNote;
  pendingNote = "";

  writeCSVLine(iso, epochNow, uptimeMs, g_adc, g_v, dbm_median, voltsToDbm(emaV),
               minuteMinDbm, minuteMaxDbm, (float)minuteAvg,
               delta_db, get_dynamic_threshold(), mad_est,
               noteToWrite, trigger_ready);

  // Update web interface snapshot
  update_json_snapshot(g_adc, g_v, dbm_median, voltsToDbm(emaV),
                      iso, epochNow, uptimeMs);

  // Debug output every 5 seconds
  print_debug_status();

  // Handle power management based on web activity
  manage_power_modes();

  // Handle web clients until next sample period
  uint32_t targetEnd = t0 + PERIOD_MS;
  while ((int32_t)(millis() - targetEnd) < 0) {
    handle_web_clients();
    delay(1);
  }

  // Handle minute summary logging
  handle_minute_summary(epochNow, uptimeMs, dbm_median);
}

// =================== HELPER FUNCTIONS ===================
void setup_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("Connecting to SSID: %s ...\n", WIFI_SSID);

  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000UL) {
    delay(200);
    Serial.print('.');
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nSTA connected: %s (ch=%d)\n",
                  WiFi.localIP().toString().c_str(), WiFi.channel());

    // Configure for power efficiency
    WiFi.setSleep(true);
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    btStop();
    setCpuFrequencyMhz(80);
  } else {
    Serial.println("\nSTA connect failed; enabling SoftAP fallback.");
    WiFi.mode(WIFI_MODE_AP);
    bool apOK = WiFi.softAP(AP_SSID, strlen(AP_PASS) ? AP_PASS : nullptr);
    Serial.printf("SoftAP %s at %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
    if (!apOK) Serial.println("SoftAP start failed.");
  }
}

void print_connection_info() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("Web interface: http://%s/\n", WiFi.localIP().toString().c_str());
  }
  if (WiFi.getMode() & WIFI_MODE_AP) {
    Serial.printf("AP interface: http://%s/\n", WiFi.softAPIP().toString().c_str());
  }
}

void send_camera_trigger(float delta_db) {
  uint32_t now = millis();
  if (now - last_now_ms < NOW_COOLDOWN_MS) return;

  trigger_msg_t msg{delta_db, now};
  esp_err_t r = esp_now_send(EMF_SENSOR_MAC, (uint8_t*)&msg, sizeof(msg));

  if (r == ESP_OK) {
    last_now_ms = now;
    Serial.printf("Trigger → CAM: Δ=%.3f dB (thr=%.3f)\n",
                  delta_db, get_dynamic_threshold());
  } else {
    Serial.printf("Trigger send ERR=%d\n", r);
  }
}

void print_debug_status() {
  static uint32_t lastDbg = 0;
  if (millis() - lastDbg > 5000) {
    lastDbg = millis();
    Serial.printf("[DBG] dbm_med=%.2f delta=%.3f dyn_thr=%.3f mad=%.3f streak=%d | CAM:%s conf=%.2f age=%ums\n",
                  g_dbm, delta_db, get_dynamic_threshold(), mad_est, on_streak,
                  cam_present ? "OCCUPIED" : "VACANT", cam_conf,
                  (cam_last_recv_ms==0) ? 0 : (millis()-cam_last_recv_ms));
  }
}

void manage_power_modes() {
  bool interactive = (millis() - lastHttpMs) < INTERACTIVE_MS;
  if (interactive) {
    esp_wifi_set_ps(WIFI_PS_NONE);
    setCpuFrequencyMhz(160);
  } else {
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    setCpuFrequencyMhz(80);
  }
}

void handle_minute_summary(uint32_t epochNow, uint32_t uptimeMs, float dbm_median) {
  if (millis() - lastMinuteTick >= 60000UL) {
    double minuteAvg = (minuteCount ? minuteSumDbm / minuteCount : NAN);

    writeCSVLine(isoTimeNow(&epochNow), epochNow, uptimeMs,
                 g_adc, g_v, dbm_median, voltsToDbm(emaV),
                 minuteMinDbm, minuteMaxDbm, (float)minuteAvg,
                 delta_db, get_dynamic_threshold(), mad_est,
                 "MINUTE_SUMMARY", check_trigger_condition());

    // Reset minute statistics
    minuteCount = 0;
    minuteSumDbm = 0.0;
    minuteMinDbm = 1e9;
    minuteMaxDbm = -1e9;
    lastMinuteTick += 60000UL;
  }
}