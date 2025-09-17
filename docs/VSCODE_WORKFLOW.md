# VSCode + PlatformIO Development Workflow

## 🚀 Quick Start

Your project is now fully configured for VSCode development with PlatformIO!

### Prerequisites

1. **Install VSCode Extensions** (recommended extensions already configured):
   - PlatformIO IDE (`platformio.platformio-ide`)
   - C/C++ Extension Pack (`ms-vscode.cpptools-extension-pack`)

### Project Structure

```
camTest/
├── camTest.code-workspace          # VSCode workspace configuration
├── esp32_cam_project/              # ESP32-CAM PlatformIO project
│   ├── platformio.ini              # Build configuration
│   ├── src/main.cpp               # Main application
│   └── lib/                       # Local libraries
├── emf_sensor_project/            # EMF sensor PlatformIO project
│   ├── platformio.ini             # Build configuration
│   ├── src/main.cpp              # Main application
│   └── lib/                      # Local libraries
├── shared_libs/                   # Shared ESP-NOW communication
└── lib/                          # Edge Impulse library
```

## 🔧 Building and Deploying

### Option 1: Using VSCode Tasks (Recommended)

**Open the workspace:**

```bash
code camTest.code-workspace
```

**Build commands (Ctrl+Shift+P → "Tasks: Run Task"):**

- `Build Both Projects` - Builds both ESP32-CAM and EMF sensor (default build task)
- `PlatformIO: Build ESP32-CAM` - Build ESP32-CAM only
- `PlatformIO: Build EMF Sensor` - Build EMF sensor only

**Upload commands:**

- `PlatformIO: Upload ESP32-CAM` - Upload to ESP32-CAM
- `PlatformIO: Upload EMF Sensor` - Upload to EMF sensor

**Serial monitor:**

- `PlatformIO: Monitor ESP32-CAM` - Open serial monitor for ESP32-CAM
- `PlatformIO: Monitor EMF Sensor` - Open serial monitor for EMF sensor

### Option 2: Using PlatformIO Panel

1. Open VSCode with the workspace file
2. Click on the PlatformIO icon in the sidebar
3. Expand project folders (esp32_cam_project / emf_sensor_project)
4. Use buttons: Build, Upload, Monitor, Clean

### Option 3: Terminal Commands

If you have PlatformIO CLI installed:

```bash
# ESP32-CAM
cd esp32_cam_project
pio run                    # Build
pio run --target upload    # Upload
pio device monitor         # Serial monitor

# EMF Sensor
cd emf_sensor_project
pio run                    # Build
pio run --target upload    # Upload
pio device monitor         # Serial monitor
```

## 🐛 Debugging

VSCode debugging is pre-configured for both projects:

1. Set breakpoints in your code
2. Press F5 or go to Run & Debug panel
3. Select configuration:
   - "Debug ESP32-CAM"
   - "Debug EMF Sensor"

**Note:** Hardware debugging requires compatible ESP32 development boards with JTAG.

## ⚙️ Project Configuration

### ESP32-CAM Settings (`esp32_cam_project/platformio.ini`)

```ini
[env:esp32cam]
platform = espressif32
board = esp32cam
framework = arduino

monitor_speed = 115200
upload_speed = 921600

board_build.partitions = huge_app.csv
board_build.arduino.memory_type = qio_opi

build_flags =
    -DCORE_DEBUG_LEVEL=0
    -DBOARD_HAS_PSRAM
    -mfix-esp32-psram-cache-issue
    -DARDUINO_ESP32_DEV
    -DESP32_CAM
```

### EMF Sensor Settings (`emf_sensor_project/platformio.ini`)

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino

monitor_speed = 115200
upload_speed = 921600

build_flags =
    -DCORE_DEBUG_LEVEL=0
    -DARDUINO_ESP32_DEV
    -DEMF_SENSOR
```

## 📚 Library Management

### Edge Impulse Library

- Location: `lib/parkingpal5478069-project-1_inferencing/`
- Automatically linked to ESP32-CAM project via `lib_extra_dirs`

### Shared Libraries

- ESP-NOW communication: `shared_libs/esp_now_comm/`
- Automatically available to both projects

### Adding New Libraries

**Method 1: PlatformIO Library Manager**

1. Open PlatformIO panel
2. Click "Libraries"
3. Search and install

**Method 2: platformio.ini**

```ini
lib_deps =
    # Arduino libraries
    ArduinoJson
    # GitHub libraries
    https://github.com/user/repo.git
```

## 🔌 Hardware Connection

### ESP32-CAM Upload Mode

1. Connect GPIO0 to GND
2. Press reset button
3. Upload firmware
4. Disconnect GPIO0 from GND
5. Press reset button

### Serial Monitor Ports

- macOS: `/dev/cu.usbserial*`
- Windows: `COM*`
- Linux: `/dev/ttyUSB*`

## 🧪 Testing Workflow

1. **Build both projects:**

   - Use `Build Both Projects` task
   - Check for compilation errors

2. **Upload firmware:**

   - ESP32-CAM: Set boot mode, upload, reset
   - EMF sensor: Direct upload

3. **Monitor serial output:**

   - Open monitors for both devices
   - Check initialization messages

4. **System integration test:**
   - Trigger EMF sensor with RF source
   - Verify ESP-NOW communication
   - Check camera activation and ML inference

## 🚨 Troubleshooting

### Build Errors

- **Library not found:** Check `lib_extra_dirs` in platformio.ini
- **Include errors:** Verify header paths and library installation
- **Memory errors:** Check PSRAM settings for ESP32-CAM

### Upload Issues

- **Port access:** Close serial monitors before upload
- **Boot mode:** ESP32-CAM requires manual boot mode for upload
- **Permissions:** macOS/Linux may need `chmod 666 /dev/ttyUSB*`

### Runtime Issues

- **Serial output:** Check baud rate (115200)
- **ESP-NOW:** Verify MAC addresses match in both projects
- **Camera:** Check pin connections and power supply

## 💡 Development Tips

1. **Use IntelliSense:** VSCode provides full autocomplete for ESP32 APIs
2. **Git integration:** Built-in source control with VSCode
3. **Multi-project:** Switch between projects using workspace folders
4. **Live templates:** Use PlatformIO code snippets
5. **Terminal integration:** Built-in terminal for commands

Your ESP32 parking detection system is now ready for professional embedded development in VSCode! 🎉
