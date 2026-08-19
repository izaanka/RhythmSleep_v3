const fs = require('fs');
const path = require('path');

const STORE_FILE = path.join(__dirname, 'data', 'store.json');

function generateDemoSession(daysAgo, durationHours, scoreTarget) {
  const baseTime = Date.now() - (daysAgo * 24 * 60 * 60 * 1000) - (8 * 3600 * 1000);
  const totalEpochs = Math.floor((durationHours * 3600) / 30); // 30-sec epochs for hypnogram
  const epochIntervalSec = 30;
  const logs = [];

  let countDeep = 0, countLight = 0, countRem = 0, countAwake = 0;

  for (let i = 0; i < totalEpochs; i++) {
    const epochTime = baseTime + (i * epochIntervalSec * 1000);
    const progressPct = i / totalEpochs;

    let stage = 'Light Sleep';
    let stageCode = 1;
    let domFreq = 6.2;
    let delta = 2.5, theta = 6.1, alpha = 2.0, beta = 0.8, gamma = 0.2;
    let certainty = 88 + Math.floor(Math.random() * 8);

    // Realistic sleep architecture cycling:
    if (progressPct < 0.05 || progressPct > 0.96) {
      stage = 'Awake'; stageCode = 0; domFreq = 16.5;
      delta = 0.5; theta = 1.2; alpha = 4.5; beta = 8.2; gamma = 1.1;
      countAwake++;
    }
    else if ((progressPct >= 0.15 && progressPct <= 0.35) || (progressPct >= 0.50 && progressPct <= 0.62)) {
      stage = 'Deep Sleep'; stageCode = 2; domFreq = 1.8;
      delta = 12.4; theta = 1.5; alpha = 0.4; beta = 0.2; gamma = 0.0;
      countDeep++;
    }
    else if ((progressPct >= 0.38 && progressPct <= 0.48) || (progressPct >= 0.70 && progressPct <= 0.85)) {
      stage = 'REM Sleep'; stageCode = 3; domFreq = 7.5;
      delta = 1.1; theta = 7.8; alpha = 3.2; beta = 1.5; gamma = 0.3;
      countRem++;
    }
    else {
      stage = 'Light Sleep'; stageCode = 1; domFreq = 5.8;
      delta = 3.1; theta = 6.8; alpha = 2.1; beta = 0.7; gamma = 0.1;
      countLight++;
    }

    logs.push({
      id: epochTime,
      mac: "3C:0F:02:E4:72:E0",
      dominant_freq: parseFloat(domFreq.toFixed(2)),
      delta: parseFloat(delta.toFixed(1)),
      theta: parseFloat(theta.toFixed(1)),
      alpha: parseFloat(alpha.toFixed(1)),
      beta: parseFloat(beta.toFixed(1)),
      gamma: parseFloat(gamma.toFixed(1)),
      stage: stage,
      stage_code: stageCode,
      certainty: certainty,
      alarm_ringing: false,
      timestamp: Math.floor(epochTime / 1000)
    });
  }

  const totalMin = Math.floor(durationHours * 60);
  const deepPct = Math.round((countDeep / totalEpochs) * 100);
  const lightPct = Math.round((countLight / totalEpochs) * 100);
  const remPct = Math.round((countRem / totalEpochs) * 100);
  const awakePct = Math.round((countAwake / totalEpochs) * 100);

  return {
    id: `SESSION-DEMO-${daysAgo}DAY`,
    startTime: baseTime,
    endTime: baseTime + (durationHours * 3600 * 1000),
    status: 'COMPLETED',
    qualification: {
      isQualified: true,
      sleepRatioPct: 100 - awakePct,
      actualSleepMin: Math.floor(totalMin * ((100 - awakePct) / 100)),
      sleepEpochs: countDeep + countLight + countRem,
      totalLogs: totalEpochs
    },
    stats: {
      totalDurationMin: totalMin,
      deepPct: deepPct,
      lightPct: lightPct,
      remPct: remPct,
      awakePct: awakePct,
      sleepScore: scoreTarget
    },
    logs: logs
  };
}

let store = { pairedDevice: null, activeSession: null, completedSessions: [] };
if (fs.existsSync(STORE_FILE)) {
  try {
    store = JSON.parse(fs.readFileSync(STORE_FILE, 'utf8'));
  } catch (e) {}
}

if (!Array.isArray(store.completedSessions)) store.completedSessions = [];

// Keep valid demo sessions and add 3 fresh demo sessions
store.completedSessions = store.completedSessions.filter(s => s.stats && s.stats.totalDurationMin >= 60);

const demo1 = generateDemoSession(1, 7.5, 92); // Yesterday: 7.5 hrs, 92 Score
const demo2 = generateDemoSession(2, 8.0, 88); // 2 Days Ago: 8.0 hrs, 88 Score
const demo3 = generateDemoSession(3, 7.0, 85); // 3 Days Ago: 7.0 hrs, 85 Score

store.completedSessions.push(demo3, demo2, demo1);

fs.writeFileSync(STORE_FILE, JSON.stringify(store, null, 2), 'utf8');
console.log('Successfully generated 3 demo sleep session reports in store.json!');
