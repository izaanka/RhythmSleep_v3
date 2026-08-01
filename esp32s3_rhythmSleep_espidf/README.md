# ESP32-S3 RhythmSleep ESP-IDF Official Build & Flash Guide

This directory contains the official **ESP-IDF** version of the RhythmSleep system, using Espressif's official **`espressif/usb_host_uac`** USB Audio Class driver for USB-C DAC soundcard audio playback.

---

## 💻 Step 1: Install ESP-IDF (Linux, macOS, Windows)

### 🐧 LINUX (Ubuntu / Debian / Fedora)

Open a terminal and execute:

```bash
# 1. Install system prerequisites
sudo apt update
sudo apt install -y git wget flex bison gperf python3 python3-pip python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0

# 2. Create ESP directory and clone ESP-IDF v5.1.2
mkdir -p ~/esp
cd ~/esp
git clone -b v5.1.2 --recursive https://github.com/espressif/esp-idf.git

# 3. Install toolchain for ESP32-S3
cd ~/esp/esp-idf
./install.sh esp32s3

# 4. Export environment variables
source ~/esp/esp-idf/export.sh
```

---

### 🍎 macOS (Intel & Apple Silicon M1/M2/M3)

Open Terminal and execute:

```bash
# 1. Install prerequisites via Homebrew (install Homebrew first if needed: https://brew.sh)
brew install cmake ninja dfu-util python3 libusb

# 2. Create ESP directory and clone ESP-IDF v5.1.2
mkdir -p ~/esp
cd ~/esp
git clone -b v5.1.2 --recursive https://github.com/espressif/esp-idf.git

# 3. Install toolchain for ESP32-S3
cd ~/esp/esp-idf
./install.sh esp32s3

# 4. Export environment variables
source ~/esp/esp-idf/export.sh
```

---

### 🪟 WINDOWS (PowerShell or Command Prompt)

#### Option A: Official ESP-IDF Windows Installer (Recommended)
1. Download the official **ESP-IDF Tools Installer**:
   `https://dl.espressif.com/dl/esp-idf/`
2. Run `esp-idf-tools-setup-offline-5.1.2.exe`.
3. Open **ESP-IDF 5.1 PowerShell** or **ESP-IDF 5.1 CMD** shortcut created on your desktop.

#### Option B: Manual PowerShell Installation

```powershell
# 1. Clone ESP-IDF repository
mkdir C:\esp
cd C:\esp
git clone -b v5.1.2 --recursive https://github.com/espressif/esp-idf.git

# 2. Install ESP-IDF toolchain for ESP32-S3
cd C:\esp\esp-idf
.\install.ps1 esp32s3

# 3. Export environment variables
.\export.ps1
```

---

## 🚀 Step 2: Build & Flash the Project

Navigate to this project folder:

### 🐧 LINUX & 🍎 macOS:

```bash
cd /home/izaan/Documents/rhy/esp32s3_rhythmSleep_espidf

# 1. Set chip target to ESP32-S3
idf.py set-target esp32s3

# 2. Build project (IDF Component Manager downloads espressif/usb_host_uac automatically)
idf.py build

# 3. Flash to board and open Serial Monitor
# Linux: /dev/ttyACM0 or /dev/ttyUSB0
# macOS: /dev/cu.usbmodem14101 or /dev/cu.usbserial-0001
idf.py -p /dev/ttyACM0 flash monitor
```

### 🪟 WINDOWS (PowerShell / CMD):

```powershell
cd C:\path\to\esp32s3_rhythmSleep_espidf

# 1. Set chip target to ESP32-S3
idf.py set-target esp32s3

# 2. Build project
idf.py build

# 3. Flash to board and open Serial Monitor (check COM port in Device Manager)
idf.py -p COM3 flash monitor
```

---

## 🎵 How USB-C DAC Audio Works in ESP-IDF:
Espressif's official **`usb_host_uac`** component performs USB Host OTG enumeration on **GPIO 20 (USB D+)** & **GPIO 19 (USB D-)**. When a USB-C DAC headphone adapter is plugged into the ESP32-S3 USB port, ESP-IDF automatically enumerates the soundcard and streams 100 Hz PCM audio frames!
