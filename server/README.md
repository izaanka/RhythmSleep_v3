# 🌙 RhythmSleep AI - Sleep Telemetry & Dashboard Server

An NPM-based Node.js server for receiving, persisting, and displaying sleep telemetry, live hardware serial logs, and completed sleep session hypnograms from the RhythmSleep ESP32 system.

---

## 🚀 Quick Start - How to Turn On the Server

### 1. Turn On the Server
Open your terminal / command prompt and run:

```bash
# Navigate to the server folder
cd server

# Install dependencies (only needed first time)
npm install

# Start the server
npm start
```

### 2. Accessing the Dashboard on Any Device
Once started, the terminal will display your server's local IP address:

- **From the local computer**: Open `http://localhost:3000`
- **From any device on your Wi-Fi (Phone / Tablet / PC)**: Open `http://<your-server-ip>:3000` (e.g. `http://192.168.1.7:3000`)

---

## 🌙 Key Web Dashboard Features

1. **🧠 Real-Time Neural Network State Card**:
   - Live sleep stage (`WAKE`, `Light Sleep`, `Deep Sleep`, `REM Sleep`), AI confidence %, peak frequency (Hz), classified EEG band, and animated spectral power bars (Delta, Theta, Alpha, Beta, Gamma).

2. **💻 Live ESP32 Hardware Serial Terminal**:
   - Real-time log streaming over WebSockets from `/dev/ttyACM0` @ 115200 baud with keyword search filtering, clear, copy, and serial command execution (`POST /api/serial-command`).

3. **⏳ 90% Sleep Stages Qualification Filter**:
   - Requires $\ge 90\%$ of valid non-artifact epochs to correspond to actual sleep states (`Light Sleep`, `Deep Sleep`, `REM Sleep`) and $\ge 60$ minutes of actual sleep duration before logging to official report history.

4. **📊 Sleep Hypnogram & Stage Analysis**:
   - Interactive Chart.js hypnogram timeline, sleep quality score (0–100), and stage breakdown.

---

## 🛠 Ports & Configuration
- **HTTP / WebSocket Port**: `3000` (Web Dashboard, WS Stream & Data REST API)
- **UDP Port**: `8888` (ESP32 Discovery & Automatic Pairing)
- **Serial Port**: `/dev/ttyACM0` (115200 baud)
- **Data File**: `server/data/store.json` (Stores persistent pairing tokens & completed sleep session logs)
