#ifndef CONFIG_H
#define CONFIG_H

// =================== SYSTEM CONFIGURATION ===================
#define NOW_CHANNEL           0            // Follow STA channel (matches EMF sensor)
#define ACTIVE_WINDOW_MS      20000        // How long to run after RF trigger
#define LED_PIN               4            // AI-Thinker white flash LED
#define LED_ACTIVE_HIGH       1            // 1 = HIGH turns LED ON
#define CAM_XCLK_HZ           10000000     // 10 MHz (try 8 MHz if unstable)

// Camera frame size options:
//   FRAMESIZE_96X96  (96x96)   -> Perfect match for EI model (96x96, no resize needed) ✅
//   FRAMESIZE_QQVGA  (160x120) -> small, resize to EI input
//   FRAMESIZE_QVGA   (320x240) -> bigger; use only if needed
#define CAM_FRAMESIZE         FRAMESIZE_96X96  // Matches Edge Impulse model exactly
#define CAM_FB_COUNT          1

// Power management: 0 = safe modem-sleep (recommended)
#define SLEEP_MODE            0

// =================== CAR DETECTION THRESHOLDS ===================
#define CAR_ON_THRESH         0.70f        // 70% confidence to set OCCUPIED
#define CAR_ON_HOLD_MS        5000         // hold 5 seconds to set OCCUPIED
#define CAR_OFF_THRESH        0.50f        // <=50% confidence to clear to VACANT
#define CAR_OFF_HOLD_MS       3000         // hold 3 seconds to clear

// =================== ESP-NOW ADDRESSING ===================
// EMF RF Sensor MAC address (where we send status updates)
extern uint8_t EMF_SENSOR_MAC[6];
// This should match the MAC address in EMF.ino's CAM_MAC array

// =================== AI-THINKER PIN MAP ===================
#define CAMERA_MODEL_AI_THINKER
#if defined(CAMERA_MODEL_AI_THINKER)
  #define PWDN_GPIO_NUM     32
  #define RESET_GPIO_NUM    -1
  #define XCLK_GPIO_NUM      0
  #define SIOD_GPIO_NUM     26
  #define SIOC_GPIO_NUM     27
  #define Y9_GPIO_NUM       35
  #define Y8_GPIO_NUM       34
  #define Y7_GPIO_NUM       39
  #define Y6_GPIO_NUM       36
  #define Y5_GPIO_NUM       21
  #define Y4_GPIO_NUM       19
  #define Y3_GPIO_NUM       18
  #define Y2_GPIO_NUM        5
  #define VSYNC_GPIO_NUM    25
  #define HREF_GPIO_NUM     23
  #define PCLK_GPIO_NUM     22
#else
  #error "Select AI-Thinker or add your camera pin map."
#endif

#endif // CONFIG_H