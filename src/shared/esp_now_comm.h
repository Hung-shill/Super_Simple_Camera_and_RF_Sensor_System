#ifndef ESP_NOW_COMM_H
#define ESP_NOW_COMM_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "config.h"

// =================== MESSAGE STRUCTURES ===================
// Trigger packet (must match sender)
typedef struct {
  float    delta_db;
  uint32_t ms;
} trigger_msg_t;

// Status packet we will SEND on state changes
typedef struct {
  uint8_t  version;     // for forward compatibility
  uint8_t  occupied;    // 1 = OCCUPIED, 0 = VACANT
  float    car_conf;    // last 'car' confidence used for transition
  uint32_t ms;          // millis() at state change
} status_msg_t;

// =================== FUNCTION DECLARATIONS ===================
bool init_now_rx_tx();
void now_send_status(uint8_t occupied, float conf);
void onNowRecv(const uint8_t* mac, const uint8_t* data, int len);
void now_on_send(const uint8_t* mac_addr, esp_now_send_status_t status);
bool now_add_peer_if_needed(const uint8_t peer_mac[6]);

// =================== GLOBAL VARIABLES ===================
extern volatile uint32_t g_trigger_until_ms;
extern bool g_peer_added;
extern uint8_t EMF_SENSOR_MAC[6];

#endif // ESP_NOW_COMM_H