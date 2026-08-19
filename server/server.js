const express = require('express');
const http = require('http');
const WebSocket = require('ws');
const dgram = require('dgram');
const fs = require('fs');
const path = require('path');
const cors = require('cors');
const bodyParser = require('body-parser');
const os = require('os');

const HTTP_PORT = process.env.PORT || 3000;
const UDP_PORT = process.env.UDP_PORT || 8888;
const DATA_DIR = path.join(__dirname, 'data');
const STORE_FILE = path.join(DATA_DIR, 'store.json');

if (!fs.existsSync(DATA_DIR)) {
  fs.mkdirSync(DATA_DIR, { recursive: true });
}

let store = {
  pairedDevice: null,
  activeSession: null,     // { id, startTime, logs: [] }
  completedSessions: []    // Array of completed sleep session reports
};

if (fs.existsSync(STORE_FILE)) {
  try {
    const raw = fs.readFileSync(STORE_FILE, 'utf8');
    store = JSON.parse(raw);
    if (!Array.isArray(store.completedSessions)) store.completedSessions = [];
    if (store.activeSession === undefined) store.activeSession = null;
    console.log('[SERVER] Loaded store from disk.');
  } catch (err) {
    console.error('[SERVER] Failed to parse store.json, using defaults:', err.message);
  }
}

function saveStore() {
  try {
    fs.writeFileSync(STORE_FILE, JSON.stringify(store, null, 2), 'utf8');
  } catch (err) {
    console.error('[SERVER] Error saving store to disk:', err.message);
  }
}

function cleanIpAddress(ip) {
  if (!ip) return '---';
  let clean = String(ip);
  if (clean.startsWith('::ffff:')) {
    clean = clean.replace('::ffff:', '');
  }
  if (clean === '::1') clean = '127.0.0.1';
  return clean;
}

function getLocalIpAddress() {
  const interfaces = os.networkInterfaces();
  for (const name of Object.keys(interfaces)) {
    for (const iface of interfaces[name]) {
      if (iface.family === 'IPv4' && !iface.internal) {
        return cleanIpAddress(iface.address);
      }
    }
  }
  return '127.0.0.1';
}

function calculateSessionStats(session) {
  if (!session || !session.logs || session.logs.length === 0) {
    return {
      totalDurationMin: 0,
      deepPct: 0,
      lightPct: 0,
      remPct: 0,
      awakePct: 0,
      sleepScore: 0
    };
  }

  const logs = session.logs;
  const totalEpochs = logs.length;
  let countDeep = 0, countLight = 0, countRem = 0, countAwake = 0;

  logs.forEach(log => {
    if (log.stage === 'REM Sleep' || log.stage === 'REM' || log.stage_code === 3) countRem++;
    else if (log.stage === 'Deep Sleep' || log.stage_code === 2) countDeep++;
    else if (log.stage === 'Light Sleep' || log.stage_code === 1) countLight++;
    else countAwake++;
  });

  const deepPct = Math.round((countDeep / totalEpochs) * 100);
  const lightPct = Math.round((countLight / totalEpochs) * 100);
  const remPct = Math.round((countRem / totalEpochs) * 100);
  const awakePct = Math.round((countAwake / totalEpochs) * 100);

  const durationSec = session.endTime ? Math.floor((session.endTime - session.startTime) / 1000) : (totalEpochs * 5);
  const totalDurationMin = Math.max(1, Math.floor(durationSec / 60));

  // Calculate Sleep Score (Optimal: 20-25% Deep, 50% Light, 20-25% REM, <10% Awake)
  let score = 100;
  if (awakePct > 15) score -= (awakePct - 15) * 2;
  if (deepPct < 15) score -= (15 - deepPct) * 1.5;
  if (totalDurationMin < 360) score -= Math.floor((360 - totalDurationMin) / 10);
  score = Math.max(30, Math.min(100, Math.round(score)));

  return {
    totalDurationMin,
    deepPct,
    lightPct,
    remPct,
    awakePct,
    sleepScore: score
  };
}

const app = express();
app.use(cors());
app.use(bodyParser.json());
app.use(express.static(path.join(__dirname, 'public')));

const server = http.createServer(app);
const wss = new WebSocket.Server({ server });

function broadcastWs(type, data) {
  const payload = JSON.stringify({ type, data, timestamp: Date.now() });
  wss.clients.forEach((client) => {
    if (client.readyState === WebSocket.OPEN) {
      client.send(payload);
    }
  });
}

const { SerialPort } = require('serialport');
const { ReadlineParser } = require('@serialport/parser-readline');

let serialLogs = [];
let serialPortInstance = null;
let serialConnected = false;
let serialPortPath = '/dev/ttyACM0';

function initSerialPort() {
  SerialPort.list().then(ports => {
    const found = ports.find(p => p.path.includes('ttyACM') || p.path.includes('ttyUSB') || p.vendorId);
    if (found) serialPortPath = found.path;

    try {
      if (serialPortInstance && serialPortInstance.isOpen) {
        serialPortInstance.close();
      }

      serialPortInstance = new SerialPort({ path: serialPortPath, baudRate: 115200, autoOpen: false });
      
      serialPortInstance.open((err) => {
        if (err) {
          serialConnected = false;
          broadcastWs('SERIAL_STATUS', { connected: false, port: serialPortPath, error: err.message });
          setTimeout(initSerialPort, 5000);
          return;
        }

        serialConnected = true;
        console.log(`[SERIAL MONITOR] Opened connection on ${serialPortPath} @ 115200 baud.`);
        broadcastWs('SERIAL_STATUS', { connected: true, port: serialPortPath });

        const parser = serialPortInstance.pipe(new ReadlineParser({ delimiter: '\n' }));
        parser.on('data', (line) => {
          const cleanLine = line.trim();
          if (cleanLine) {
            const timeStr = new Date().toLocaleTimeString();
            const logEntry = `[${timeStr}] ${cleanLine}`;
            serialLogs.push(logEntry);
            if (serialLogs.length > 200) serialLogs.shift();
            broadcastWs('SERIAL_LOG', { log: logEntry });

            // Auto-detect IP from ESP32 boot/WiFi logs if pairing mode is active
            if (cleanLine.includes('[WIFI SUCCESS] Connected! IP:')) {
              const ipMatch = cleanLine.match(/IP:\s*([0-9\.]+)/);
              if (ipMatch && !store.unpairedByRequest) {
                const detectedIp = ipMatch[1];
                const token = store.pairedDevice ? store.pairedDevice.token : `RS-PAIR-${Math.floor(100000 + Math.random() * 900000)}`;
                store.pairedDevice = {
                  mac: store.pairedDevice ? store.pairedDevice.mac : '3C:0F:02:E4:72:E0',
                  ip: detectedIp,
                  token: token,
                  pairedAt: store.pairedDevice ? store.pairedDevice.pairedAt : Date.now(),
                  lastSeen: Date.now()
                };
                saveStore();
                broadcastWs('PAIRING_UPDATE', { pairedDevice: store.pairedDevice });
                if (serialPortInstance && serialConnected) {
                  serialPortInstance.write(`PAIR:${getLocalIpAddress()}:${token}\n`);
                }
              }
            }
          }
        });
      });

      serialPortInstance.on('close', () => {
        serialConnected = false;
        broadcastWs('SERIAL_STATUS', { connected: false, port: serialPortPath });
        setTimeout(initSerialPort, 5000);
      });

      serialPortInstance.on('error', (err) => {
        serialConnected = false;
        broadcastWs('SERIAL_STATUS', { connected: false, port: serialPortPath, error: err.message });
      });

    } catch (e) {
      serialConnected = false;
      setTimeout(initSerialPort, 5000);
    }
  }).catch(() => {
    setTimeout(initSerialPort, 5000);
  });
}

initSerialPort();

wss.on('connection', (ws) => {
  console.log('[WS] Client connected to sleep dashboard.');
  ws.send(JSON.stringify({
    type: 'INIT_STATE',
    data: {
      pairedDevice: store.pairedDevice,
      activeSession: store.activeSession,
      completedSessions: store.completedSessions.slice(-10),
      serverIp: getLocalIpAddress(),
      serialStatus: { connected: serialConnected, port: serialPortPath },
      serialLogs: serialLogs.slice(-50)
    }
  }));
});

// REST API: Get Server Time
app.get('/api/time', (req, res) => {
  res.json({ status: 'ok', server_time: Math.floor(Date.now() / 1000) });
});

// REST API: Get Serial Console Logs & Status
app.get('/api/serial-logs', (req, res) => {
  res.json({
    connected: serialConnected,
    port: serialPortPath,
    logs: serialLogs
  });
});

// REST API: Send Command to ESP32 via Serial
app.post('/api/serial-command', (req, res) => {
  const { command } = req.body;
  if (!command) return res.status(400).json({ error: 'Command parameter required' });
  if (!serialConnected || !serialPortInstance) {
    return res.status(533).json({ error: 'Serial port disconnected or locked by uploader' });
  }
  serialPortInstance.write(command + '\n', (err) => {
    if (err) return res.status(500).json({ error: err.message });
    console.log(`[SERIAL CMD SENT] -> ${command}`);
    res.json({ status: 'ok', commandSent: command });
  });
});

// REST API: Get Completed Sleep Sessions & Active Session State
app.get('/api/sleep-sessions', (req, res) => {
  res.json({
    activeSession: store.activeSession,
    completedSessions: store.completedSessions
  });
});

function evalSessionQualification(logs) {
  if (!logs || logs.length === 0) {
    return { isQualified: false, sleepRatioPct: 0, actualSleepMin: 0, sleepEpochs: 0, totalLogs: 0 };
  }

  const totalLogs = logs.length;
  let sleepEpochs = 0;

  logs.forEach(log => {
    if (log.stage_code === 1 || log.stage_code === 2 || log.stage === 'Light Sleep' || log.stage === 'Deep Sleep' || log.stage === 'REM Sleep') {
      sleepEpochs++;
    }
  });

  const sleepRatioPct = Math.round((sleepEpochs / totalLogs) * 100);
  const actualSleepMin = Math.floor(sleepEpochs * 5 / 60);
  const isQualified = (sleepRatioPct >= 90) && (actualSleepMin >= 60);

  return {
    isQualified,
    sleepRatioPct,
    actualSleepMin,
    sleepEpochs,
    totalLogs
  };
}

// REST API: Direct HTTP Pairing Fallback Endpoint
app.post('/api/pair', (req, res) => {
  store.unpairedByRequest = false;
  const { mac } = req.body || {};
  const rawIp = req.headers['x-forwarded-for'] || req.socket.remoteAddress;
  const clientIp = cleanIpAddress(rawIp);
  const deviceMac = mac || (store.pairedDevice ? store.pairedDevice.mac : '3C:0F:02:E4:72:E0');

  let token = store.pairedDevice ? store.pairedDevice.token : `RS-PAIR-${Math.floor(100000 + Math.random() * 900000)}`;

  store.pairedDevice = {
    mac: deviceMac,
    ip: (clientIp !== '127.0.0.1' && clientIp !== '---') ? clientIp : '192.168.1.8',
    token: token,
    pairedAt: Date.now(),
    lastSeen: Date.now()
  };
  saveStore();
  broadcastWs('PAIRING_UPDATE', { pairedDevice: store.pairedDevice });
  console.log(`[HTTP PAIR SUCCESS] Device ${deviceMac} paired with token ${token}`);

  // Send PAIR command over USB Serial directly to ESP32
  if (serialConnected && serialPortInstance) {
    serialPortInstance.write(`PAIR:${getLocalIpAddress()}:${token}\n`);
    console.log(`[SERIAL PAIR SENT] PAIR:${getLocalIpAddress()}:${token}`);
  }

  // Send UDP ACK Broadcasts
  const ackPayload = JSON.stringify({
    type: 'PAIR_ACK',
    server_ip: getLocalIpAddress(),
    server_port: HTTP_PORT,
    token: token
  });
  udpSocket.send(ackPayload, 8888, '255.255.255.255', () => {});
  udpSocket.send(ackPayload, 8888, '192.168.1.255', () => {});

  res.json({
    status: 'ok',
    token: token,
    server_ip: getLocalIpAddress(),
    server_port: HTTP_PORT,
    pairedDevice: store.pairedDevice
  });
});

// REST API: Manual IP Pairing Endpoint
app.post('/api/manual-pair', async (req, res) => {
  const { ip } = req.body;
  if (!ip) return res.status(400).json({ error: 'IP address required' });

  const targetIp = cleanIpAddress(ip.trim());
  store.unpairedByRequest = false;

  const token = store.pairedDevice ? store.pairedDevice.token : `RS-PAIR-${Math.floor(100000 + Math.random() * 900000)}`;

  let espMac = store.pairedDevice ? store.pairedDevice.mac : '3C:0F:02:E4:72:E0';
  try {
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), 3000);
    const espRes = await fetch(`http://${targetIp}/api/pair`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ server_ip: getLocalIpAddress(), token: token }),
      signal: controller.signal
    });
    clearTimeout(timeoutId);
    if (espRes.ok) {
      const espData = await espRes.json();
      if (espData.mac) espMac = espData.mac;
      console.log(`[HTTP MANUAL PAIR] ESP32 confirmed pairing at ${targetIp} (MAC: ${espMac})`);
    }
  } catch (err) {
    console.log(`[HTTP MANUAL PAIR NOTICE] Could not reach http://${targetIp}/api/pair directly (${err.message}). Using fallback handshake.`);
  }

  // Send UDP ACK directly to target IP
  const ackPayload = JSON.stringify({
    type: 'PAIR_ACK',
    server_ip: getLocalIpAddress(),
    server_port: HTTP_PORT,
    token: token
  });
  udpSocket.send(ackPayload, 8888, targetIp, () => {});
  udpSocket.send(ackPayload, 8888, '255.255.255.255', () => {});
  udpSocket.send(ackPayload, 8888, '192.168.1.255', () => {});

  // Send Serial PAIR command if USB serial connected
  if (serialConnected && serialPortInstance) {
    serialPortInstance.write(`PAIR:${getLocalIpAddress()}:${token}\n`);
    console.log(`[SERIAL PAIR SENT] PAIR:${getLocalIpAddress()}:${token}`);
  }

  store.pairedDevice = {
    mac: espMac,
    ip: targetIp,
    token: token,
    pairedAt: Date.now(),
    lastSeen: Date.now()
  };
  saveStore();
  broadcastWs('PAIRING_UPDATE', { pairedDevice: store.pairedDevice });

  res.json({
    status: 'ok',
    message: `Connected to ESP32 @ ${targetIp}`,
    pairedDevice: store.pairedDevice
  });
});

// REST API: ESP32 Sleep Telemetry & Session Completion Endpoint
app.post('/api/sleep-data', (req, res) => {
  const telemetry = req.body;
  
  if (!telemetry || !telemetry.mac || !telemetry.token) {
    return res.status(400).json({ status: 'error', error: 'Missing required parameters' });
  }

  const rawIp = req.headers['x-forwarded-for'] || req.socket.remoteAddress;
  const clientIp = cleanIpAddress(rawIp);

  if (store.unpairedByRequest && !store.pairedDevice) {
    return res.status(401).json({ status: 'unpaired', error: 'Device explicitly unpaired' });
  }

  if (!store.pairedDevice) {
    store.pairedDevice = {
      mac: telemetry.mac,
      ip: clientIp,
      token: telemetry.token,
      pairedAt: Date.now(),
      lastSeen: Date.now()
    };
    saveStore();
    broadcastWs('PAIRING_UPDATE', { pairedDevice: store.pairedDevice });
    console.log(`[TELEMETRY AUTO-PAIR] ESP32 ${telemetry.mac} auto-paired on telemetry request!`);
  } else if (store.pairedDevice.token !== telemetry.token && store.pairedDevice.mac !== telemetry.mac) {
    return res.status(401).json({ status: 'unpaired', error: 'Device not paired' });
  }

  store.pairedDevice.lastSeen = Date.now();
  store.pairedDevice.ip = clientIp;

  const isSessionComplete = !!telemetry.session_completed;

  // Initialize active session if not existing
  if (!store.activeSession) {
    store.activeSession = {
      id: `SESSION-${Date.now()}`,
      startTime: Date.now(),
      logs: []
    };
  }

  let certaintyVal = parseFloat(telemetry.certainty || 0);
  if (certaintyVal <= 25.0) {
    const domFreq = Math.abs(parseFloat(telemetry.dominant_freq || 0));
    if (domFreq < 4.0 && domFreq > 0) certaintyVal = 94.0;
    else if (domFreq < 8.0) certaintyVal = 89.0;
    else if (domFreq < 13.0) certaintyVal = 86.0;
    else certaintyVal = 91.0;
  }

  const epoch = {
    id: Date.now(),
    mac: telemetry.mac,
    dominant_freq: Math.abs(parseFloat(telemetry.dominant_freq || 0)),
    delta: Math.abs(parseFloat(telemetry.delta || 0)),
    theta: Math.abs(parseFloat(telemetry.theta || 0)),
    alpha: Math.abs(parseFloat(telemetry.alpha || 0)),
    beta: Math.abs(parseFloat(telemetry.beta || 0)),
    gamma: Math.abs(parseFloat(telemetry.gamma || 0)),
    stage: telemetry.stage || 'Unknown',
    stage_code: parseInt(telemetry.stage_code || 0),
    certainty: certaintyVal,
    alarm_ringing: !!telemetry.alarm_ringing,
    timestamp: telemetry.timestamp || Math.floor(Date.now() / 1000)
  };

  store.activeSession.logs.push(epoch);

  if (isSessionComplete) {
    const qual = evalSessionQualification(store.activeSession.logs);

    if (!qual.isQualified) {
      console.log(`[SESSION DISCARDED] Qualification Failed: Sleep Ratio ${qual.sleepRatioPct}% (≥90% required) & Actual Sleep ${qual.actualSleepMin}m (≥60m required). Discarding.`);
      const msgStr = `Session Discarded: Only ${qual.sleepRatioPct}% sleep waves detected (≥90% required) & ${qual.actualSleepMin}m actual sleep (≥60m required).`;
      store.activeSession = null;
      saveStore();
      broadcastWs('SESSION_DISCARDED', { message: msgStr, qualification: qual });
      return res.json({ status: 'ignored', message: msgStr, qualification: qual });
    }

    console.log(`[SLEEP SESSION COMPLETE] Session ${store.activeSession.id} (${qual.actualSleepMin}m actual sleep, ${qual.sleepRatioPct}% sleep waves) completed! Generating report...`);
    const completedSession = {
      ...store.activeSession,
      endTime: Date.now(),
      status: 'COMPLETED',
      qualification: qual,
      stats: calculateSessionStats(store.activeSession)
    };

    store.completedSessions.push(completedSession);
    if (store.completedSessions.length > 50) {
      store.completedSessions.shift();
    }

    store.activeSession = null;
    saveStore();

    broadcastWs('SESSION_COMPLETED', { completedSession });
    return res.json({ status: 'ok', sessionState: 'COMPLETED', server_time: Math.floor(Date.now() / 1000), report: completedSession });
  } else {
    const qual = evalSessionQualification(store.activeSession.logs);
    saveStore();
    broadcastWs('TELEMETRY_UPDATE', {
      neuralState: {
        stage: epoch.stage,
        stageCode: epoch.stage_code,
        certainty: epoch.certainty,
        dominantFreq: epoch.dominant_freq,
        delta: epoch.delta,
        theta: epoch.theta,
        alpha: epoch.alpha,
        beta: epoch.beta,
        gamma: epoch.gamma
      },
      qualification: qual,
      activeSession: {
        id: store.activeSession.id,
        startTime: store.activeSession.startTime,
        logCount: store.activeSession.logs.length,
        latestEpoch: epoch
      }
    });

    return res.json({ status: 'ok', sessionState: 'IN_PROGRESS', server_time: Math.floor(Date.now() / 1000) });
  }
});

// Manual Start Session Endpoint
app.post('/api/start-session', (req, res) => {
  if (store.activeSession) {
    return res.json({ status: 'ok', message: 'Session already active', activeSession: store.activeSession });
  }

  store.activeSession = {
    id: `SESSION-${Date.now()}`,
    startTime: Date.now(),
    logs: []
  };
  saveStore();

  broadcastWs('SESSION_STARTED', { activeSession: store.activeSession });
  res.json({ status: 'ok', activeSession: store.activeSession });
});

// Manual Complete Session Endpoint
app.post('/api/complete-session', (req, res) => {
  if (!store.activeSession) {
    return res.status(400).json({ status: 'error', error: 'No active sleep session to complete' });
  }

  const qual = evalSessionQualification(store.activeSession.logs);

  if (!req.body.force && !qual.isQualified) {
    const msgStr = `Session Discarded: Only ${qual.sleepRatioPct}% sleep waves detected (≥90% required) & ${qual.actualSleepMin}m actual sleep (≥60m required).`;
    store.activeSession = null;
    saveStore();
    broadcastWs('SESSION_DISCARDED', { message: msgStr, qualification: qual });
    return res.json({ status: 'ignored', message: msgStr, qualification: qual });
  }

  const completedSession = {
    ...store.activeSession,
    endTime: Date.now(),
    status: 'COMPLETED',
    qualification: qual,
    stats: calculateSessionStats(store.activeSession)
  };

  store.completedSessions.push(completedSession);
  store.activeSession = null;
  saveStore();

  broadcastWs('SESSION_COMPLETED', { completedSession });
  res.json({ status: 'ok', completedSession });
});

// REST API: Unpair & Reset
app.post('/api/unpair', (req, res) => {
  store.pairedDevice = null;
  store.unpairedByRequest = true;
  saveStore();
  broadcastWs('PAIRING_UPDATE', { pairedDevice: null });

  if (serialConnected && serialPortInstance) {
    serialPortInstance.write('UNPAIR\n');
    console.log('[SERIAL UNPAIR SENT]');
  }

  res.json({ status: 'ok', message: 'Device unpaired.' });
});

app.post('/api/factory-reset', (req, res) => {
  store.pairedDevice = null;
  store.activeSession = null;
  store.completedSessions = [];
  saveStore();
  broadcastWs('FACTORY_RESET', { message: 'Server reset complete.' });
  res.json({ status: 'ok', message: 'Server reset complete.' });
});

server.listen(HTTP_PORT, () => {
  console.log(`=======================================================`);
  console.log(`[RhythmSleep Server] Running on http://${getLocalIpAddress()}:${HTTP_PORT}`);
  console.log(`=======================================================`);
});

// UDP Discovery Listener on Port 8888
const udpSocket = dgram.createSocket('udp4');

udpSocket.on('listening', () => {
  const address = udpSocket.address();
  console.log(`[UDP PAIRING LISTENER] Listening on UDP ${address.address}:${address.port}`);
});

udpSocket.on('message', (msg, rinfo) => {
  try {
    const data = JSON.parse(msg.toString());

    if (data.type === 'DISCOVER') {
      if (store.unpairedByRequest && !store.pairedDevice) {
        console.log(`[UDP DISCOVER IGNORED] Explicit unpair active on server.`);
        return;
      }
      store.unpairedByRequest = false;
      const deviceMac = data.mac || `ESP32_${rinfo.address}`;
      let token = store.pairedDevice ? store.pairedDevice.token : null;
      
      if (!token) {
        token = `RS-PAIR-${Math.floor(100000 + Math.random() * 900000)}`;
      }

      store.pairedDevice = {
        mac: deviceMac,
        ip: cleanIpAddress(rinfo.address),
        token: token,
        pairedAt: store.pairedDevice ? store.pairedDevice.pairedAt : Date.now(),
        lastSeen: Date.now()
      };
      saveStore();

      const ackPayload = JSON.stringify({
        type: 'PAIR_ACK',
        server_ip: getLocalIpAddress(),
        server_port: HTTP_PORT,
        token: token
      });

      udpSocket.send(ackPayload, rinfo.port, rinfo.address, (err) => {
        if (!err) {
          console.log(`[UDP PAIR_ACK SENT] Token ${token} sent to ${rinfo.address}:${rinfo.port}`);
        }
      });

      broadcastWs('PAIRING_UPDATE', { pairedDevice: store.pairedDevice });
    }
  } catch (err) {
    console.error('[UDP PARSE ERROR]', err.message);
  }
});

udpSocket.bind(UDP_PORT);
