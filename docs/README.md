# ESP32-CAM + EMF RF Sensor Parking Detection System

This project implements a complete edge-computing parking detection system with dual-sensing technology:

## System Overview

**Two-Unit System:**
1. **EMF RF Sensor** - Detects vehicle RF emissions (phones, electronics)
2. **ESP32-CAM** - Provides visual confirmation using Edge Impulse ML

**Communication:** ESP-NOW wireless protocol connects both units

## System Architecture

The code has been reorganized into modular components:

### ESP32-CAM Modules

1. **config.h** - System configuration and pin definitions
2. **esp_now_comm.h/.cpp** - ESP-NOW wireless communication (shared)
3. **camera_ei.h/.cpp** - Camera control and Edge Impulse integration
4. **car_detection.h/.cpp** - Car presence state machine
5. **power_mgmt.h/.cpp** - Power management functions
6. **esp32_camera.ino** - Main camera application

### EMF RF Sensor Modules

1. **emf_config.h** - EMF sensor configuration
2. **emf_sensor.h/.cpp** - AD8317 RF power detection & signal processing
3. **emf_web.h** - Web interface for monitoring
4. **EMF_reorganized.ino** - Main EMF sensor application
5. **EMF.ino** - Original EMF code (for reference)

### Key Features

- **Dual-sensor approach**: RF detection + visual confirmation
- **Adaptive RF threshold**: Uses MAD (Median Absolute Deviation) for robust detection
- **Edge Impulse ML**: On-device car detection inference
- **Power efficient**: Automatic camera shutdown and low-power idle modes
- **State-based detection**: Hysteresis prevents false triggering
- **Web monitoring**: Real-time RF levels and system status
- **Bi-directional communication**: Status updates flow between sensors

## Configuration

### ESP32-CAM Settings (`config.h`):
- `NOW_CHANNEL`: ESP-NOW channel (0 = follow STA channel)
- `EMF_SENSOR_MAC[]`: EMF sensor MAC address for status updates
- `CAR_ON_THRESH`: ML confidence threshold → OCCUPIED (70%)
- `CAR_OFF_THRESH`: ML confidence threshold → VACANT (50%)
- `CAR_ON_HOLD_MS`: Hold time before triggering OCCUPIED (5s)
- `CAR_OFF_HOLD_MS`: Hold time before clearing to VACANT (3s)
- `ACTIVE_WINDOW_MS`: Camera active time after RF trigger (20s)

### EMF RF Sensor Settings (`emf_config.h`):
- `WIFI_SSID/PASS`: WiFi credentials for web interface
- `PIN_VOUT`: ADC pin for AD8317 RF sensor (GPIO 34)
- `slope_V_per_dB`: AD8317 calibration (-22 mV/dB)
- `K_NOISE`: Adaptive threshold multiplier (7.0x MAD)
- `N_ON`: Consecutive samples needed to trigger (5 seconds)
- `NOW_COOLDOWN_MS`: Minimum time between triggers (30s)

## System Operation Flow

### 1. RF Detection (EMF Sensor)
- Continuously monitors RF power levels using AD8317 sensor
- Applies 5-point median filter to reduce noise spikes
- Uses adaptive threshold based on MAD (Median Absolute Deviation)
- Triggers ESP32-CAM when RF activity detected for 5+ consecutive seconds

### 2. Visual Confirmation (ESP32-CAM)
- Receives RF trigger via ESP-NOW
- Activates camera and runs Edge Impulse ML inference
- Applies hysteresis-based state machine:
  - **VACANT → OCCUPIED**: ≥70% confidence for 5 consecutive seconds
  - **OCCUPIED → VACANT**: ≤50% confidence for 3 consecutive seconds
- Sends status updates back to EMF sensor

### 3. Power Management
**ESP32-CAM:**
- **Active**: Camera on, processing frames during 20s trigger window
- **Idle**: Camera off, low-power ESP-NOW listening mode

**EMF Sensor:**
- **Interactive**: Full speed when web interface accessed (160 MHz)
- **Background**: Power-efficient monitoring (80 MHz, modem sleep)

## Dependencies

See `libraries.txt` for complete installation instructions.

## Hardware Setup

- ESP32-CAM (AI-Thinker module)
- External antenna recommended for better range
- FTDI adapter for programming
- Connect GPIO0 to GND for programming mode

## Setup Instructions

### 1. Hardware Setup
**EMF RF Sensor:**
- ESP32 Dev Module with AD8317 RF power detector
- Connect AD8317 VOUT to GPIO 34
- External antenna recommended

**ESP32-CAM:**
- AI-Thinker ESP32-CAM module
- FTDI adapter for programming
- External antenna recommended

### 2. Software Configuration
1. Install dependencies (see `libraries.txt`)
2. Configure your Edge Impulse model name in ESP32-CAM includes
3. Update MAC addresses in both systems:
   - EMF sensor: Set `CAM_MAC[]` in `src/emf_sensor/EMF.ino` to match your ESP32-CAM
   - ESP32-CAM: Set `EMF_SENSOR_MAC[]` in `src/shared/esp_now_comm.cpp` to match your EMF sensor
4. Configure WiFi credentials in `src/emf_sensor/emf_config.h`

### 3. Upload and Operation
1. Upload `src/emf_sensor/EMF_reorganized.ino` to EMF sensor ESP32
2. Upload `src/esp32_cam/esp32_camera.ino` to ESP32-CAM
3. Power on EMF sensor first (provides web interface)
4. Power on ESP32-CAM (waits for RF triggers)
5. Monitor system via web interface at EMF sensor IP address

**Note:** Due to the organized folder structure, make sure your Arduino IDE can find the include files. You may need to add the `src/` folders to your Arduino libraries path or copy the header files to the sketch folders.

## Web Interface

The EMF sensor provides a real-time web dashboard showing:
- Live RF power measurements and thresholds
- Camera status (OCCUPIED/VACANT) with confidence levels
- System logs and downloadable CSV data
- Manual trigger testing capability