# RhythmSleep v3 - ESP32-S3 Neural Network EEG Sleep System

RhythmSleep v3 is a real-time EEG sleep monitoring, smart alarm, and on-device machine learning system built for the ESP32-S3 microcontroller. It runs an on-device backpropagation neural network, digital IIR bandpass signal filtering, motion artifact rejection, dual TFT/OLED displays, SoftAP Wi-Fi provisioning, UDP pairing, and connects to a dedicated telemetry web server running on an Arduino UNO Q.

---

## System Overview

The system consists of two primary hardware components:
1. **ESP32-S3 Wearable Monitor**: Captures analog EEG signals, processes frequency bands via FFT, runs local sleep stage classification (Wake, Light Sleep, Deep Sleep, REM), drives on-board displays, and controls haptic smart alarms.
2. **Arduino UNO Q Telemetry Server**: Hosts the Node.js web dashboard and persistent storage hub, receiving live telemetry streams over Wi-Fi and USB serial, managing device pairing, and rendering session reports and hypnograms.

---

## Hardware Architecture and Pinout Map

- **Wearable Board**: ESP32-S3-N16R8 (16MB Flash, 8MB PSRAM)
- **Telemetry Server Board**: Arduino UNO Q (running the Node.js server stack)
- **Serial Interface**: `/dev/ttyACM0` (115200 baud monitor / 921600 baud upload)

| Peripheral / Sensor | Pin Identifier | ESP32-S3 GPIO | Function |
| :--- | :--- | :--- | :--- |
| Analog EEG Input | `ANALOG_EEG_PIN` | GPIO 1 | ADC Analog Input (`ADC1_CH0`) |
| Vibration Haptic Motor | `PIN_VIBRATION` | GPIO 21 | Smart Alarm Haptic Feedback |
| I2C SDA (RTC/OLED) | `SDA_PIN` | GPIO 8 | PCF8563 RTC and SSD1306 OLED Data |
| I2C SCL (RTC/OLED) | `SCL_PIN` | GPIO 9 | PCF8563 RTC and SSD1306 OLED Clock |
| Button 1 (Menu) | `BTN_MENU_PIN` | GPIO 4 | Cycle Navigation Menus |
| Button 2 (Up) | `BTN_UP_PIN` | GPIO 5 | Increment Values |
| Button 3 (Down) | `BTN_DOWN_PIN` | GPIO 6 | Decrement Values |
| Button 4 (Select/OK) | `BTN_SELECT_PIN` | GPIO 7 | Select / Toggle Tracking |
| TFT SPI MOSI / SCLK | `TFT_MOSI / SCLK` | GPIO 11 / 12 | ST7789 2.8-inch SPI Display |
| TFT CS / DC / RST | `TFT_CS / DC / RST` | GPIO 38 / 39 / 40 | Display Control |
| TFT Backlight | `TFT_BLK` | GPIO 48 | Backlight PWM Control |
| SD Card Chip Select | `SD_CS_PIN` | GPIO 10 | SPI Storage Interface |

---

## On-Device Display Interface

The ESP32-S3 firmware provides a 5-screen interface accessible via the physical buttons:

1. **Menu 0 - Time and Date**:
   - Displays real-time clock synchronized via hardware PCF8563 RTC and calibrated to GMT+5:30.
2. **Menu 1 - Real-Time EEG**:
   - Shows dominant frequency (Hz), neural sleep classification (Wake, Light Sleep, Deep Sleep, REM Sleep), model confidence percentage, and tracking state.
   - Pressing Select toggles sleep tracking mode.
3. **Menu 2 - Neural Network Stats**:
   - Displays training progress, backpropagation passes, and non-volatile storage sync status.
   - Pressing Select triggers an on-device backpropagation calibration step.
4. **Menu 3 - Smart Alarm**:
   - Allows configuration of wake windows and alarm thresholds.
5. **Menu 4 - Wi-Fi and System Status**:
   - Shows network status, IP address, server token, and factory reset controls.

---

## Signal Processing and Neural Network Pipeline

- **Motion Artifact Rejection**: Monitors ADC rail limits (< 30 or > 4065 counts) to detect and discard motion corrupted epochs.
- **Digital Bandpass Filtering**: 0.5 Hz to 45.0 Hz Butterworth filter to remove DC drift and mains noise.
- **Temporal Context Voting**: Majority voting across consecutive 5-second epochs to smooth transient stage transitions.
- **On-Device Backpropagation**: Trains 1,140 model parameters locally on the ESP32 with persistent storage in flash memory upon session completion.

---

## Sleep Qualification Standards

Sessions are evaluated against standard sleep criteria before permanent recording:
1. **Sleep Ratio**: At least 90% of valid epochs must correspond to recognized sleep stages (Light, Deep, or REM).
2. **Minimum Duration**: A minimum of 60 minutes of cumulative sleep must be recorded.
3. Sessions falling below these thresholds are flagged and discarded to maintain data integrity.

---

## Telemetry Web Server (Arduino UNO Q)

The web dashboard and backend are configured to run directly on the **Arduino UNO Q** board, acting as the local telemetry collector and user interface host.

### Starting the Server on Arduino UNO Q

```bash
cd server
npm install
npm start
```

Access the dashboard by navigating to `http://localhost:3000` or `http://<arduino-uno-q-ip>:3000` on any device connected to the same local network.

### Dashboard Capabilities

- **Real-Time Neural State Panel**: Live sleep stage indicator, dominant frequency tracking, confidence ratings, and spectral power bars for Delta, Theta, Alpha, Beta, and Gamma bands.
- **Serial Monitor**: Direct WebSocket stream of hardware logs from `/dev/ttyACM0` at 115200 baud with log filtering and interactive serial command sending.
- **Hypnogram Timeline**: Interactive sleep stage progression chart with computed sleep efficiency metrics.

---

## Firmware Compilation and Upload

To compile and flash the ESP32-S3 firmware using `arduino-cli`:

```bash
# 1. Compile firmware binary
arduino-cli compile --fqbn esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=huge_app,PSRAM=opi esp32s3_rhythmSleep_arduinoide/

# 2. Upload to ESP32-S3
arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=huge_app,PSRAM=opi esp32s3_rhythmSleep_arduinoide/
```
