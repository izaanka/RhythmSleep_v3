document.addEventListener('DOMContentLoaded', () => {
  let socket = null;
  let hypnogramChart = null;

  const stageMap = {
    0: 'Awake',
    1: 'Light Sleep',
    2: 'Deep Sleep',
    3: 'REM Sleep'
  };

  // DOM Elements
  const serverIpDisplay = document.getElementById('server-ip-display');
  const sessionStatusBanner = document.getElementById('session-status-banner');
  const bannerTitle = document.getElementById('banner-title');
  const bannerSub = document.getElementById('banner-sub');
  const btnForceComplete = document.getElementById('btn-force-complete');

  const sessionHistorySelect = document.getElementById('session-history-select');
  const metricScore = document.getElementById('metric-score');
  const metricDuration = document.getElementById('metric-duration');
  const metricDeep = document.getElementById('metric-deep');
  const metricRem = document.getElementById('metric-rem');

  const pairingStatusBadge = document.getElementById('pairing-status-badge');
  const deviceMacEl = document.getElementById('device-mac');
  const deviceIpEl = document.getElementById('device-ip');
  const deviceTokenEl = document.getElementById('device-token');
  const deviceLastSeenEl = document.getElementById('device-last-seen');

  const manualEspIp = document.getElementById('manual-esp-ip');
  const btnManualPair = document.getElementById('btn-manual-pair');
  const btnPair = document.getElementById('btn-pair');
  const btnUnpair = document.getElementById('btn-unpair');
  const btnFactoryReset = document.getElementById('btn-factory-reset');
  const actionMessage = document.getElementById('action-message');

  let completedSessionsStore = [];

  function initChart() {
    const ctx = document.getElementById('hypnogramChart').getContext('2d');
    hypnogramChart = new Chart(ctx, {
      type: 'line',
      data: {
        labels: [],
        datasets: [{
          label: 'Sleep Stage',
          data: [],
          borderColor: '#00f2fe',
          backgroundColor: 'rgba(0, 242, 254, 0.15)',
          borderWidth: 2,
          fill: true,
          stepped: true,
          pointRadius: 2,
          pointBackgroundColor: '#9d4edd'
        }]
      },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        scales: {
          x: {
            grid: { color: 'rgba(255, 255, 255, 0.05)' },
            ticks: { color: '#8a9bb8', font: { family: 'JetBrains Mono' } }
          },
          y: {
            min: 0,
            max: 3,
            ticks: {
              stepSize: 1,
              color: '#8a9bb8',
              callback: (val) => stageMap[val] || val
            },
            grid: { color: 'rgba(255, 255, 255, 0.05)' }
          }
        },
        plugins: {
          legend: { display: false }
        }
      }
    });
  }

  function renderCompletedSessionReport(session) {
    if (!session || !session.logs || session.logs.length === 0) {
      metricScore.textContent = '--';
      metricDuration.textContent = '--';
      metricDeep.textContent = '--%';
      metricRem.textContent = '--%';
      if (hypnogramChart) {
        hypnogramChart.data.labels = [];
        hypnogramChart.data.datasets[0].data = [];
        hypnogramChart.update();
      }
      return;
    }

    const stats = session.stats || {};
    metricScore.textContent = stats.sleepScore || 85;
    
    const min = stats.totalDurationMin || Math.max(1, Math.floor(session.logs.length * 5 / 60));
    const h = Math.floor(min / 60);
    const m = min % 60;
    metricDuration.textContent = h > 0 ? `${h}h ${m}m` : `${m}m`;

    metricDeep.textContent = `${stats.deepPct || 0}%`;
    metricRem.textContent = `${stats.remPct || 0}%`;

    // Render Full Hypnogram Chart
    if (hypnogramChart) {
      const labels = [];
      const stageCodes = [];

      session.logs.forEach(log => {
        const timeStr = new Date(log.timestamp * 1000).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
        labels.push(timeStr);
        let code = log.stage_code;
        if (log.stage === 'REM Sleep' || log.stage === 'REM' || code === 3) {
          code = 3;
        } else if (log.stage === 'Deep Sleep' || code === 2) {
          code = 2;
        } else if (log.stage === 'Light Sleep' || code === 1) {
          code = 1;
        } else {
          code = 0;
        }
        stageCodes.push(code);
      });

      hypnogramChart.data.labels = labels;
      hypnogramChart.data.datasets[0].data = stageCodes;
      hypnogramChart.update();
    }
  }

  function updateSessionStatusUI(activeSession, completedSessions) {
    completedSessionsStore = completedSessions || [];

    // Populate session history dropdown
    sessionHistorySelect.innerHTML = '';
    if (completedSessionsStore.length > 0) {
      completedSessionsStore.forEach((s, idx) => {
        const opt = document.createElement('option');
        opt.value = idx;
        const timeStr = new Date(s.endTime || s.startTime).toLocaleString();
        opt.textContent = `Session ${idx + 1} - ${timeStr} (${s.stats ? s.stats.totalDurationMin : 0}m)`;
        sessionHistorySelect.appendChild(opt);
      });
      sessionHistorySelect.value = completedSessionsStore.length - 1;
      renderCompletedSessionReport(completedSessionsStore[completedSessionsStore.length - 1]);
    } else {
      const opt = document.createElement('option');
      opt.textContent = 'No completed sessions yet';
      sessionHistorySelect.appendChild(opt);
      renderCompletedSessionReport(null);
    }

    let latestState = neuralState || lastNeuralState;
    if (neuralState) lastNeuralState = neuralState;

    // Active session status banner (if present in DOM)
    if (sessionStatusBanner) {
      if (activeSession) {
        sessionStatusBanner.className = 'session-banner banner-in-progress';
        const isConfirmedSleep = latestState && (
          latestState.stage === 'Light Sleep' || 
          latestState.stage === 'Deep Sleep' || 
          latestState.stage === 'REM Sleep' ||
          (latestState.stageCode >= 1 && latestState.stageCode <= 3)
        );

        if (isConfirmedSleep) {
          if (bannerTitle) bannerTitle.textContent = `${latestState.stage} in Progress...`;
          if (bannerSub) bannerSub.textContent = `Confirmed sleeping state (${latestState.stage}, ${(latestState.certainty || 90).toFixed(0)}% confidence). Recording sleep telemetry.`;
        } else {
          if (bannerTitle) bannerTitle.textContent = 'Pre-Sleep Monitoring (Awake)';
          if (bannerSub) bannerSub.textContent = 'Tracking session active. Monitoring brainwaves — "Sleep in Progress" will activate automatically as soon as sleep is detected.';
        }
        if (btnForceComplete) btnForceComplete.classList.remove('hidden');
        if (btnStartSession) btnStartSession.classList.add('hidden');
      } else {
        sessionStatusBanner.className = 'session-banner banner-completed';
        if (bannerTitle) bannerTitle.textContent = 'Device Ready / Sleep Idle';
        if (bannerSub) bannerSub.textContent = 'ESP32 is connected in IDLE mode. Start sleep tracking on your ESP32 (press OK on Menu 1) or click below to begin session.';
        if (btnForceComplete) btnForceComplete.classList.add('hidden');
        if (btnStartSession) btnStartSession.classList.remove('hidden');
      }
    }
  }

  let actualMac = '--:--:--:--:--:--';
  let isMacRevealed = false;

  function updateMacDisplay() {
    if (isMacRevealed) {
      deviceMacEl.textContent = actualMac;
      deviceMacEl.title = 'Click to hide MAC Address';
    } else {
      deviceMacEl.textContent = actualMac !== '--:--:--:--:--:--' ? '•••••••••••• (Click to reveal)' : '--:--:--:--:--:--';
      deviceMacEl.title = 'Click to show MAC Address';
    }
  }

  if (deviceMacEl) {
    deviceMacEl.style.cursor = 'pointer';
    deviceMacEl.addEventListener('click', () => {
      isMacRevealed = !isMacRevealed;
      updateMacDisplay();
    });
  }

  function updatePairingUI(pairedDevice) {
    if (pairedDevice && pairedDevice.mac) {
      pairingStatusBadge.textContent = 'PAIRED';
      pairingStatusBadge.className = 'badge badge-connected';
      actualMac = pairedDevice.mac;
      updateMacDisplay();
      const rawIp = pairedDevice.ip || '---';
      deviceIpEl.textContent = rawIp.replace(/^::ffff:/, '');
      deviceTokenEl.textContent = pairedDevice.token || '---';
      
      if (pairedDevice.lastSeen) {
        const secAgo = Math.max(0, Math.floor((Date.now() - pairedDevice.lastSeen) / 1000));
        deviceLastSeenEl.textContent = `${secAgo} sec ago`;
      } else {
        deviceLastSeenEl.textContent = 'Just now';
      }
      if (btnPair) btnPair.classList.add('hidden');
      if (btnUnpair) btnUnpair.classList.remove('hidden');
    } else {
      pairingStatusBadge.textContent = 'UNPAIRED';
      pairingStatusBadge.className = 'badge badge-disconnected';
      actualMac = '--:--:--:--:--:--';
      updateMacDisplay();
      deviceIpEl.textContent = '---.---.---.---';
      deviceTokenEl.textContent = 'NONE';
      deviceLastSeenEl.textContent = 'Never';
      if (btnPair) btnPair.classList.remove('hidden');
      if (btnUnpair) btnUnpair.classList.add('hidden');
    }
  }

  function showMessage(msg, isError = false) {
    actionMessage.textContent = msg;
    actionMessage.className = `action-message ${isError ? 'btn-danger' : ''}`;
    actionMessage.classList.remove('hidden');
    setTimeout(() => {
      actionMessage.classList.add('hidden');
    }, 4000);
  }

  // Terminal DOM Elements
  const serialStatusBadge = document.getElementById('serial-status-badge');
  const serialFilterInput = document.getElementById('serial-filter-input');
  const serialAutoscroll = document.getElementById('serial-autoscroll');
  const btnClearTerminal = document.getElementById('btn-clear-terminal');
  const btnCopyTerminal = document.getElementById('btn-copy-terminal');
  const serialTerminalWindow = document.getElementById('serial-terminal-window');
  const serialCmdInput = document.getElementById('serial-cmd-input');
  const btnSendSerialCmd = document.getElementById('btn-send-serial-cmd');

  function updateSerialStatusUI(status) {
    if (status && status.connected) {
      serialStatusBadge.textContent = `CONNECTED (${status.port || '/dev/ttyACM0'})`;
      serialStatusBadge.className = 'badge badge-connected';
    } else {
      serialStatusBadge.textContent = `RECONNECTING (${status ? status.port : '/dev/ttyACM0'})...`;
      serialStatusBadge.className = 'badge badge-disconnected';
    }
  }

  function appendSerialLog(logStr) {
    if (!serialTerminalWindow) return;
    const filterText = serialFilterInput ? serialFilterInput.value.toLowerCase().trim() : '';

    if (filterText && !logStr.toLowerCase().includes(filterText)) {
      return;
    }

    const lineDiv = document.createElement('div');
    lineDiv.className = 'terminal-line';
    if (logStr.includes('[ALARM]') || logStr.includes('RINGING')) {
      lineDiv.classList.add('error-line');
    } else if (logStr.includes('[NN') || logStr.includes('[SIGNAL')) {
      lineDiv.classList.add('system-line');
    }
    lineDiv.textContent = logStr;

    serialTerminalWindow.appendChild(lineDiv);

    while (serialTerminalWindow.children.length > 300) {
      serialTerminalWindow.removeChild(serialTerminalWindow.firstChild);
    }

    if (serialAutoscroll && serialAutoscroll.checked) {
      serialTerminalWindow.scrollTop = serialTerminalWindow.scrollHeight;
    }
  }

  function updateNeuralStateUI(neuralState, activeSession, qualification) {
    if (!neuralState) return;

    const liveDominantFreq = document.getElementById('live-dominant-freq');
    const liveEegBand = document.getElementById('live-eeg-band');
    const liveStageIcon = document.getElementById('live-stage-icon');
    const liveStageName = document.getElementById('live-stage-name');
    const liveStageConf = document.getElementById('live-stage-conf');
    const liveSessionTimer = document.getElementById('live-session-timer');

    const domFreq = Math.abs(neuralState.dominantFreq || 0);
    if (liveDominantFreq) liveDominantFreq.textContent = `${domFreq.toFixed(2)} Hz`;

    let bandName = 'Awake / High Beta';
    if (domFreq < 4.0) bandName = 'Delta (Deep Sleep)';
    else if (domFreq < 8.0) bandName = 'Theta (Light Sleep)';
    else if (domFreq < 13.0) bandName = 'Alpha (Relaxed)';
    else if (domFreq < 30.0) bandName = 'Beta (Active)';
    else bandName = 'Gamma (High Cognitive)';
    if (liveEegBand) liveEegBand.textContent = bandName;

    const stage = neuralState.stage || 'Awake';
    if (liveStageName) liveStageName.textContent = stage;
    if (liveStageConf) liveStageConf.textContent = `${(neuralState.certainty || 0).toFixed(0)}% Confidence`;

    // 90% Sleep Ratio & 60m Duration Qualification Counter
    if (qualification && activeSession) {
      const ratio = qualification.sleepRatioPct || 0;
      const actualMin = qualification.actualSleepMin || 0;
      const isQualified = qualification.isQualified;

      if (liveSessionTimer) {
        if (isQualified) {
          liveSessionTimer.textContent = `Sleep Ratio: ${ratio}% ✅ | Actual Sleep: ${actualMin}m ✅ (Log Qualified!)`;
          liveSessionTimer.style.color = 'var(--accent-green)';
        } else {
          const ratioStatus = ratio >= 90 ? '✅' : '(Need ≥90%)';
          const durStatus = actualMin >= 60 ? '✅' : '(Need ≥60m)';
          liveSessionTimer.textContent = `Sleep Ratio: ${ratio}% ${ratioStatus} | Actual Sleep: ${actualMin}m ${durStatus}`;
          liveSessionTimer.style.color = 'var(--accent-warning)';
        }
      }
    } else {
      if (liveSessionTimer) {
        liveSessionTimer.textContent = 'Sleep Duration Filter: Need ≥90% Sleep Waves & 60m Sleep Time';
        liveSessionTimer.style.color = 'var(--text-muted)';
      }
    }

    // Update spectral frequency bars (normalized non-negative values)
    const delta = Math.abs(neuralState.delta || 0);
    const theta = Math.abs(neuralState.theta || 0);
    const alpha = Math.abs(neuralState.alpha || 0);
    const beta  = Math.abs(neuralState.beta || 0);
    const gamma = Math.abs(neuralState.gamma || 0);

    const maxVal = Math.max(1.0, delta, theta, alpha, beta, gamma);

    const updateBar = (band, val) => {
      const bar = document.getElementById(`bar-${band}`);
      const txt = document.getElementById(`val-${band}`);
      if (bar) bar.style.width = `${Math.min(100, Math.max(5, (val / maxVal) * 100))}%`;
      if (txt) txt.textContent = val.toFixed(1);
    };

    updateBar('delta', delta);
    updateBar('theta', theta);
    updateBar('alpha', alpha);
    updateBar('beta', beta);
    updateBar('gamma', gamma);
  }

  function connectWebSocket() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${protocol}//${window.location.host}`;
    
    socket = new WebSocket(wsUrl);

    socket.onopen = () => {
      console.log('[WS] Connected to RhythmSleep server.');
    };

    socket.onmessage = (event) => {
      try {
        const msg = JSON.parse(event.data);
        if (msg.type === 'INIT_STATE') {
          serverIpDisplay.textContent = msg.data.serverIp || window.location.hostname;
          updatePairingUI(msg.data.pairedDevice);
          updateSessionStatusUI(msg.data.activeSession, msg.data.completedSessions);
          updateSerialStatusUI(msg.data.serialStatus);
          if (msg.data.serialLogs) {
            msg.data.serialLogs.forEach(l => appendSerialLog(l));
          }
        } else if (msg.type === 'SERIAL_LOG') {
          appendSerialLog(msg.data.log);
        } else if (msg.type === 'SERIAL_STATUS') {
          updateSerialStatusUI(msg.data);
        } else if (msg.type === 'TELEMETRY_UPDATE') {
          if (msg.data.neuralState) {
            updateNeuralStateUI(msg.data.neuralState, msg.data.activeSession, msg.data.qualification);
            updateSessionStatusUI(msg.data.activeSession, completedSessionsStore, msg.data.neuralState);
          }
        } else if (msg.type === 'SESSION_COMPLETED') {
          showMessage('Qualified Sleep Session Complete! Report logged.');
          fetchSessions();
        } else if (msg.type === 'SESSION_DISCARDED') {
          showMessage(msg.data.message || 'Session discarded (Did not meet 90% sleep waves or 60m duration).', true);
          fetchSessions();
        } else if (msg.type === 'PAIRING_UPDATE') {
          updatePairingUI(msg.data.pairedDevice);
        } else if (msg.type === 'FACTORY_RESET') {
          updatePairingUI(null);
          updateSessionStatusUI(null, []);
          showMessage('Factory reset complete. Server memory wiped.');
        }
      } catch (err) {
        console.error('[WS PARSE ERROR]', err);
      }
    };

    socket.onclose = () => {
      setTimeout(connectWebSocket, 3000);
    };
  }

  async function fetchSessions() {
    try {
      const res = await fetch('/api/sleep-sessions');
      const data = await res.json();
      updateSessionStatusUI(data.activeSession, data.completedSessions);
    } catch (err) {
      console.error('[FETCH SESSIONS ERROR]', err);
    }
  }

  // Listener for History Select Dropdown
  sessionHistorySelect.addEventListener('change', (e) => {
    const idx = parseInt(e.target.value);
    if (!isNaN(idx) && completedSessionsStore[idx]) {
      renderCompletedSessionReport(completedSessionsStore[idx]);
    }
  });

  // Listener for Force Complete Session Button
  btnForceComplete.addEventListener('click', async () => {
    try {
      const res = await fetch('/api/complete-session', { method: 'POST' });
      const data = await res.json();
      if (data.status === 'ok') {
        showMessage('Session marked as completed. Report generated!');
        fetchSessions();
      } else {
        showMessage(data.error || 'Failed to complete session.', true);
      }
    } catch (err) {
      showMessage('Error completing session.', true);
    }
  });

  // Pair, Unpair & Factory Reset Buttons
  if (btnPair) {
    btnPair.addEventListener('click', async () => {
      try {
        const res = await fetch('/api/pair', { method: 'POST' });
        const data = await res.json();
        if (data.status === 'ok') {
          if (data.pairedDevice) {
            updatePairingUI(data.pairedDevice);
            showMessage('Device paired successfully!');
          } else {
            showMessage('Pairing mode enabled. Listening for ESP32...');
          }
        }
      } catch (err) { showMessage('Network error.', true); }
    });
  }

  if (btnManualPair) {
    btnManualPair.addEventListener('click', async () => {
      const ip = manualEspIp ? manualEspIp.value.trim() : '';
      if (!ip) {
        showMessage('Please enter ESP32 IP address.', true);
        return;
      }
      try {
        showMessage(`Connecting to ESP32 @ ${ip}...`);
        const res = await fetch('/api/manual-pair', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ ip })
        });
        const data = await res.json();
        if (data.status === 'ok') {
          if (data.pairedDevice) {
            updatePairingUI(data.pairedDevice);
            showMessage(`Connected & Paired with ESP32 @ ${ip}!`);
          }
        } else {
          showMessage(data.error || 'Failed to connect.', true);
        }
      } catch (err) {
        showMessage('Network error during manual pairing.', true);
      }
    });
  }

  if (manualEspIp) {
    manualEspIp.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') {
        if (btnManualPair) btnManualPair.click();
      }
    });
  }

  if (btnUnpair) {
    btnUnpair.addEventListener('click', async () => {
      try {
        const res = await fetch('/api/unpair', { method: 'POST' });
        const data = await res.json();
        if (data.status === 'ok') {
          updatePairingUI(null);
          showMessage('Device unpaired.');
        }
      } catch (err) { showMessage('Network error.', true); }
    });
  }

  // Terminal Event Listeners
  if (btnClearTerminal) {
    btnClearTerminal.addEventListener('click', () => {
      if (serialTerminalWindow) serialTerminalWindow.innerHTML = '';
      showMessage('Terminal cleared.');
    });
  }

  if (btnCopyTerminal) {
    btnCopyTerminal.addEventListener('click', () => {
      if (!serialTerminalWindow) return;
      const text = serialTerminalWindow.innerText;
      navigator.clipboard.writeText(text).then(() => {
        showMessage('Serial log lines copied to clipboard!');
      }).catch(err => {
        showMessage('Failed to copy to clipboard.', true);
      });
    });
  }

  async function sendSerialCommand() {
    if (!serialCmdInput) return;
    const cmd = serialCmdInput.value.trim();
    if (!cmd) return;

    try {
      const res = await fetch('/api/serial-command', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ command: cmd })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        showMessage(`Command sent -> ${cmd}`);
        appendSerialLog(`[CLIENT SENT] -> ${cmd}`);
        serialCmdInput.value = '';
      } else {
        showMessage(data.error || 'Failed to send serial command', true);
      }
    } catch (err) {
      showMessage('Error sending command over serial.', true);
    }
  }

  if (btnSendSerialCmd) {
    btnSendSerialCmd.addEventListener('click', sendSerialCommand);
  }

  if (serialCmdInput) {
    serialCmdInput.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') {
        sendSerialCommand();
      }
    });
  }

  if (btnFactoryReset) {
    btnFactoryReset.addEventListener('click', async () => {
      try {
        const res = await fetch('/api/factory-reset', { method: 'POST' });
        const data = await res.json();
        if (data.status === 'ok') {
          updatePairingUI(null);
          showMessage('Factory reset executed.');
          fetchSessions();
        }
      } catch (err) { showMessage('Network error.', true); }
    });
  }

  initChart();
  connectWebSocket();
  fetchSessions();
});
