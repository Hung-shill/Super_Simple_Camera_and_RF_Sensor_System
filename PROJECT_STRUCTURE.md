# ESP32-CAM + EMF RF Sensor - Project Structure

This project implements a dual-sensor parking detection system using professional folder organization and modular architecture.

## 📁 Project Structure

```
camTest/
├── CLAUDE.md                    # Claude Code configuration
├── PROJECT_STRUCTURE.md         # This file
├──
├── 📂 src/                      # Source code (organized by component)
│   ├── 📂 esp32_cam/           # ESP32-CAM visual detection unit
│   │   ├── esp32_camera.ino    # Main ESP32-CAM application
│   │   ├── config.h            # ESP32-CAM configuration constants
│   │   ├── camera_ei.h/.cpp    # Camera & Edge Impulse ML integration
│   │   ├── car_detection.h/.cpp # Car presence state machine
│   │   └── power_mgmt.h/.cpp   # Power management functions
│   │
│   ├── 📂 emf_sensor/          # EMF RF sensor unit
│   │   ├── EMF_reorganized.ino # Main EMF sensor application (new)
│   │   ├── EMF.ino            # Original EMF code (reference)
│   │   ├── emf_config.h       # EMF sensor configuration
│   │   ├── emf_sensor.h/.cpp  # AD8317 RF detection & processing
│   │   └── emf_web.h          # Web interface structure
│   │
│   └── 📂 shared/              # Shared/common modules
│       ├── esp_now_comm.h     # ESP-NOW communication protocol
│       └── esp_now_comm.cpp   # (Used by both ESP32-CAM and EMF sensor)
│
├── 📂 docs/                     # Documentation & guides
│   ├── README.md              # Complete system documentation
│   └── libraries.txt          # Dependencies & installation guide
│
└── 📂 hardware/                # Hardware documentation (future)
    └── (schematics, PCB layouts, etc.)
```

## 🎯 Component Architecture

### ESP32-CAM Unit (`src/esp32_cam/`)
**Role:** Visual confirmation using Edge Impulse ML
- **Main:** `esp32_camera.ino` - Application entry point
- **Camera:** `camera_ei.*` - Camera control & ML inference
- **Detection:** `car_detection.*` - State machine with hysteresis
- **Power:** `power_mgmt.*` - Sleep modes & power optimization
- **Config:** `config.h` - Pin definitions & thresholds

### EMF RF Sensor Unit (`src/emf_sensor/`)
**Role:** RF activity detection & web monitoring
- **Main:** `EMF_reorganized.ino` - New organized version
- **Legacy:** `EMF.ino` - Original code (kept for reference)
- **Sensor:** `emf_sensor.*` - AD8317 RF power detection
- **Config:** `emf_config.h` - RF thresholds & network settings
- **Web:** `emf_web.h` - Real-time monitoring interface

### Shared Components (`src/shared/`)
**Role:** Common functionality between units
- **Communication:** `esp_now_comm.*` - Bi-directional ESP-NOW messaging

### Documentation (`docs/`)
**Role:** Setup guides & technical documentation
- **Main Guide:** `README.md` - Complete system documentation
- **Dependencies:** `libraries.txt` - Required libraries & installation

## 🔧 File Include Paths

Due to the organized structure, include paths use relative references:

**ESP32-CAM includes:**
```cpp
#include "config.h"                    // Local config
#include "../shared/esp_now_comm.h"    // Shared communication
#include "camera_ei.h"                 // Local modules
```

**EMF Sensor includes:**
```cpp
#include "emf_config.h"                // Local config
#include "../shared/esp_now_comm.h"    // Shared communication
#include "emf_sensor.h"                // Local modules
```

## 📋 Best Practices Applied

✅ **Separation of Concerns:** Each component has its own folder
✅ **Shared Code:** Common functionality in `/shared` folder
✅ **Documentation:** Centralized in `/docs` with clear structure
✅ **Legacy Preservation:** Original code kept for reference
✅ **Proper Naming:** Clear, descriptive file and folder names
✅ **Scalability:** Easy to add new components or hardware variants

## 🚀 Development Workflow

1. **ESP32-CAM Development:** Work in `src/esp32_cam/`
2. **EMF Sensor Development:** Work in `src/emf_sensor/`
3. **Shared Changes:** Modify `src/shared/` (affects both units)
4. **Documentation:** Update files in `docs/`
5. **Hardware:** Add schematics/PCBs to `hardware/`

This structure follows modern embedded systems organization principles and makes the codebase maintainable and scalable for your mini Waymo parking detection project.