# 🌙 RhythmSleep v3 - ESP32-S3 Neural Network EEG Sleep System

RhythmSleep v3 is a complete, real-time EEG sleep monitoring, smart alarm, and on-device machine learning system built for the **ESP32-S3**. It features an on-device Backpropagation Neural Network ($\eta=0.005$), digital IIR bandpass signal filtering, motion artifact rejection, dual TFT/OLED menu display, SoftAP Wi-Fi provisioning, persistent UDP pairing, and a full Node.js web dashboard with a live hardware serial terminal.

---

## 📐 Hardware Architecture & Pinout Map

**Board**: ESP32-S3-N16R8 (16MB Flash, 8MB PSRAM)  
**Serial Port**: `/dev/ttyACM0` (115200 baud Serial Monitor / 921600 baud Flash)

| Peripherals / Sensor | Pin Name | ESP32-S3 GPIO | Notes |
| :--- | :--- | :--- | :--- |
| 🧠 **Analog EEG Input** | `ANALOG_EEG_PIN` | **GPIO 1** | ADC Analog Input (`ADC1_CH0`) |
| 📳 **Vibration Haptic Motor** | `PIN_VIBRATION` | **GPIO 21** | Smart Alarm Haptic Motor |
| 🔴 **I2C SDA (RTC/OLED)** | `SDA_PIN` | **GPIO 8** | PCF8563 RTC & SSD1306 OLED Data |
| 🔵 **I2C SCL (RTC/OLED)** | `SCL_PIN` | **GPIO 9** | PCF8563 RTC & SSD1306 OLED Clock |
| 🔘 **Button 1 (Menu)** | `BTN_MENU_PIN` | **GPIO 4** | Cycle Display Menus |
| 🔘 **Button 2 (Up)** | `BTN_UP_PIN` | **GPIO 5** | Increment Edit Values |
| 🔘 **Button 3 (Down)** | `BTN_DOWN_PIN` | **GPIO 6** | Decrement Edit Values |
| 🔘 **Button 4 (Select/OK)** | `BTN_SELECT_PIN`| **GPIO 7** | OK / Select / Toggle Sleep Tracking |
| 🖥️ **TFT SPI MOSI / SCLK** | `TFT_MOSI / SCLK`| **GPIO 11 / 12**| ST7789 2.8" SPI Color Display |
| 🖥️ **TFT CS / DC / RST** | `TFT_CS / DC / RST`| **GPIO 38 / 39 / 40**| Display Control Pins |
| 💡 **TFT Backlight** | `TFT_BLK` | **GPIO 48** | Backlight Control |
| 📁 **SD Card Chip Select** | `SD_CS_PIN` | **GPIO 10** | SD Card Module SPI |

---

## 📱 Hardware 5-Menu Display Flow

1. **Menu 0 `[1/5] TIME & DATE`**:
   - Real-time clock display synchronized via hardware PCF8563 RTC.
2. **Menu 1 `[2/5] EEG REAL-TIME`**:
   - Real-time dominant frequency (Hz), classified neural sleep stage (`WAKE`, `Light Sleep`, `Deep Sleep`, `REM Sleep`), AI confidence %, and current operation mode (`IDLE` vs `PRE-SLEEP AWAIT` vs `SLEEPING`).
   - Press **OK (SELECT)** to manually toggle Sleep Tracking Mode.
3. **Menu 2 `[3/5] NEURAL AI STATS`**:
   - % AI Learned, % Optimized, NVS Backprop passes count, and NVS weight sync state.
   - Press **OK (SELECT)** to force an immediate manual on-device Backpropagation recalibration pass.
4. **Menu 3 `[4/5] SMART ALARM`**:
   - Configure start time, end time, and enable/disable smart alarm window.
5. **Menu 4 `[5/5] WIFI & RESET`**:
   - Displays Wi-Fi connection status, Local IP, Server Pairing Token, and Factory Reset button.
   - Press **OK (SELECT)** to trigger full factory reset (clears Wi-Fi/NVS memory and reboots to SoftAP).

---

## 🔬 Signal Processing & On-Device Neural Network

- **Motion Artifact Rejection**: ADC rail clipping check ($< 30$ or $> 4065$ ADC units) discards motion artifact epochs (`[SIGNAL REJECTED]`).
- **Digital Bandpass Filtering**: 0.5 Hz – 45.0 Hz digital Butterworth filter removes DC offset and 50/60 Hz mains hum.
- **30-Second Temporal Context Filter**: 6 contiguous 5s epoch majority voting prevents stage flickering.
- **On-Device Backpropagation Learning**: Recalibrates 1,140 weights ($\eta=0.005$) directly on ESP32 RAM and saves updated weights persistently to `nn_weights` NVS namespace upon sleep session completion.

---

## ⏳ 90% Sleep Stages Qualification Rule

A session is **only logged into the official sleep history report** if:
1. **≥90% Actual Sleep States**: At least 90% of valid non-artifact epochs must be classified as actual sleep stages (`Light Sleep`, `Deep Sleep`, `REM Sleep`).
2. **≥60 Minutes (1 Hour) Actual Sleep**: At least 60 minutes of cumulative actual sleep must be recorded.
3. *Short sessions or sessions with >10% awake time are automatically discarded from official reports (`"Session Discarded: Only 45% sleep waves detected (≥90% required)"`).*

---

## 💻 Web Dashboard & Real-Time Serial Monitor

### 1. Launch the Node.js Server
```bash
# Enter server directory and start
cd server
npm install
npm start
```

Open `http://localhost:3000` (or `http://<server-ip>:3000` from any smartphone/tablet on the same Wi-Fi network).

### 2. Dashboard Features
- **🧠 Real-Time Neural Network State Card**: Live sleep stage, AI confidence %, dominant frequency, classified EEG band, and animated spectral power bars (Delta, Theta, Alpha, Beta, Gamma).
- **💻 Live ESP32 Hardware Serial Monitor**: Real-time log streaming over WebSockets from `/dev/ttyACM0` @ 115200 baud with search filtering, clear, copy, and serial command execution.
- **📊 Sleep Hypnogram & Stage Analysis**: Interactive sleep timeline chart, sleep quality score (0–100), and stage ratio breakdown.

---

## 🛠️ Compilation & Flashing Instructions

To compile and upload firmware via `arduino-cli`:

```bash
# 1. Compile sketch for ESP32-S3 16MB Flash
~/.local/bin/arduino-cli compile --fqbn esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi esp32s3_rhythmSleep_arduinoide/esp32s3_rhythmSleep_arduinoide.ino

# 2. Upload binary to ESP32-S3 on /dev/ttyACM0
~/.local/bin/arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi esp32s3_rhythmSleep_arduinoide/esp32s3_rhythmSleep_arduinoide.ino
```
