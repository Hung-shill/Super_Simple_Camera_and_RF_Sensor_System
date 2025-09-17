#include "esp_now_comm.h"

// =================== GLOBAL VARIABLES ===================
volatile uint32_t g_trigger_until_ms = 0;
bool g_peer_added = false;
// EMF RF Sensor MAC - must match CAM_MAC in EMF.ino
uint8_t EMF_SENSOR_MAC[6] = { 0x78, 0x42, 0x1C, 0x6D, 0xBD, 0x68 };

// =================== ESP-NOW CALLBACKS ===================
void onNowRecv(const uint8_t* mac, const uint8_t* data, int len) {
  if (len < (int)sizeof(trigger_msg_t)) return;
  trigger_msg_t m;
  memcpy(&m, data, sizeof(m));
  g_trigger_until_ms = millis() + ACTIVE_WINDOW_MS;

  // LED visual acknowledgment
  digitalWrite(LED_PIN, LED_ACTIVE_HIGH ? HIGH : LOW);
  Serial.printf("[NOW] Trigger delta_db=%.3f\n", m.delta_db);
}

void now_on_send(const uint8_t* mac_addr, esp_now_send_status_t status) {
  Serial.printf("[NOW] TX → %02X:%02X:%02X:%02X:%02X:%02X  %s\n",
    mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5],
    status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

// =================== PEER MANAGEMENT ===================
bool now_add_peer_if_needed(const uint8_t peer_mac[6]) {
  if (g_peer_added) return true;

  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, peer_mac, 6);
  peer.channel = NOW_CHANNEL;     // must match channel
  peer.encrypt = false;

  if (esp_now_add_peer(&peer) == ESP_OK) {
    g_peer_added = true;
    Serial.println("[NOW] Peer added (TX target)");
    return true;
  }

  Serial.println("[NOW] add_peer failed");
  return false;
}

// =================== STATUS TRANSMISSION ===================
void now_send_status(uint8_t occupied, float conf) {
  if (!g_peer_added && !now_add_peer_if_needed(EMF_SENSOR_MAC)) return;

  status_msg_t m{1, occupied, conf, millis()};
  esp_err_t rc = esp_now_send(EMF_SENSOR_MAC, (const uint8_t*)&m, sizeof(m));
  if (rc != ESP_OK) {
    Serial.printf("[NOW] send rc=%d\n", (int)rc);
  }
}

// =================== INITIALIZATION ===================
bool init_now_rx_tx() {
  delay(3000);  // settle delay
  WiFi.mode(WIFI_STA);

  // Channel configuration (0 = follow STA channel to match EMF sensor)
  if (NOW_CHANNEL > 0) {
    esp_err_t r1 = esp_wifi_set_channel(NOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
    uint8_t p = 0;
    wifi_second_chan_t s = WIFI_SECOND_CHAN_NONE;
    esp_err_t r2 = esp_wifi_get_channel(&p, &s);
    Serial.printf("[NOW] set_ch=%d rc=%d; get→ch=%u sec=%d\n", NOW_CHANNEL, (int)r1, (unsigned)p, (int)s);
  } else {
    Serial.println("[NOW] Following STA channel (matches EMF sensor)");
  }

  if (esp_now_init() != ESP_OK) {
    Serial.println("[NOW] init fail");
    return false;
  }

  // Register RX callback for triggers
  if (esp_now_register_recv_cb(onNowRecv) != ESP_OK) {
    Serial.println("[NOW] recv_cb fail");
    return false;
  }

  // Register TX callback and add peer for status sends
  esp_now_register_send_cb(now_on_send);
  now_add_peer_if_needed(EMF_SENSOR_MAC);

  Serial.println("[NOW] RX+TX ready");
  return true;
}