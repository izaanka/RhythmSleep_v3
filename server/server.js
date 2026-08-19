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

function getLocalIpAddress() {
  const interfaces = os.networkInterfaces();
  for (const name of Object.keys(interfaces)) {
    for (const iface of interfaces[name]) {
      if (iface.family === 'IPv4' && !iface.internal) {
        return iface.address;
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
    if (log.stage_code === 2) countDeep++;
    else if (log.stage_code === 1) countLight++;
    else if (log.stage_code === 2 || log.stage === 'REM Sleep') countRem++;
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

wss.on('connection', (ws) => {
  console.log('[WS] Client connected to sleep dashboard.');
  ws.send(JSON.stringify({
    type: 'INIT_STATE',
    data: {
      pairedDevice: store.pairedDevice,
      activeSession: store.activeSession,
      completedSessions: store.completedSessions.slice(-10),
      serverIp: getLocalIpAddress()
    }
  }));
});

// REST API: Get Server Time
app.get('/api/time', (req, res) => {
  res.json({ status: 'ok', server_time: Math.floor(Date.now() / 1000) });
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

// REST API: ESP32 Sleep Telemetry & Session Completion Endpoint
app.post('/api/sleep-data', (req, res) => {
  const telemetry = req.body;
  
  if (!telemetry || !telemetry.mac || !telemetry.token) {
    return res.status(400).json({ status: 'error', error: 'Missing required parameters' });
  }

  if (!store.pairedDevice || store.pairedDevice.token !== telemetry.token) {
    return res.status(401).json({ status: 'unpaired', error: 'Device not paired' });
  }

  const clientIp = req.headers['x-forwarded-for'] || req.socket.remoteAddress;
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
  saveStore();
  broadcastWs('PAIRING_UPDATE', { pairedDevice: null });
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
      const deviceMac = data.mac || `ESP32_${rinfo.address}`;
      let token = store.pairedDevice ? store.pairedDevice.token : null;
      
      if (!token) {
        token = `RS-PAIR-${Math.floor(100000 + Math.random() * 900000)}`;
      }

      store.pairedDevice = {
        mac: deviceMac,
        ip: rinfo.address,
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
