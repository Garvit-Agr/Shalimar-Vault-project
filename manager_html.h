const char manager_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>Vault Control — Manager Dashboard</title>
<link href="https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Barlow:wght@300;400;600&display=swap" rel="stylesheet">
<style>
  :root {
    --vault-green: #00ff88; --vault-green-dim: #00cc6a;
    --vault-red: #ff3344; --vault-amber: #ffaa00;
    --bg: #0a0c0f; --surface: #111418; --surface2: #1a1e24;
    --border: #2a2f38; --text: #e8edf5; --text-muted: #5a6272;
    --mono: 'Share Tech Mono', monospace; --sans: 'Barlow', sans-serif;
  }
  * { margin: 0; padding: 0; box-sizing: border-box; }
  body { background: var(--bg); color: var(--text); font-family: var(--sans); min-height: 100vh; }
  body::before { content: ''; position: fixed; inset: 0; background-image: linear-gradient(var(--border) 1px, transparent 1px), linear-gradient(90deg, var(--border) 1px, transparent 1px); background-size: 40px 40px; opacity: 0.15; pointer-events: none; }
  nav { display: flex; align-items: center; justify-content: space-between; padding: 16px 28px; border-bottom: 1px solid var(--border); background: var(--surface); position: sticky; top: 0; z-index: 100; }
  .nav-left { display: flex; align-items: center; gap: 14px; }
  .nav-logo { font-family: var(--mono); font-size: 15px; letter-spacing: 0.08em; color: var(--vault-green); }
  .nav-sep { width: 1px; height: 20px; background: var(--border); }
  .nav-role { font-size: 12px; color: var(--text-muted); font-family: var(--mono); letter-spacing: 0.06em; }
  .nav-right { display: flex; align-items: center; gap: 20px; font-family: var(--mono); font-size: 12px; color: var(--text-muted); }
  .status-pill { display: flex; align-items: center; gap: 6px; padding: 5px 12px; border-radius: 20px; border: 1px solid; font-size: 11px; letter-spacing: 0.08em; }
  .status-pill.secure { border-color: rgba(0,255,136,0.3); color: var(--vault-green); background: rgba(0,255,136,0.05); }
  .status-pill.session { border-color: rgba(255,170,0,0.3); color: var(--vault-amber); background: rgba(255,170,0,0.05); }
  .dot { width: 6px; height: 6px; border-radius: 50%; background: currentColor; animation: blink 1.4s infinite; }
  @keyframes blink { 0%,100%{opacity:1} 50%{opacity:0.3} }
  .page { padding: 28px; max-width: 1100px; margin: 0 auto; }
  .section-title { font-family: var(--mono); font-size: 10px; letter-spacing: 0.16em; color: var(--text-muted); text-transform: uppercase; margin-bottom: 14px; display: flex; align-items: center; gap: 10px; }
  .section-title::after { content: ''; flex: 1; height: 1px; background: var(--border); }
  .stats-row { display: grid; grid-template-columns: repeat(4, 1fr); gap: 14px; margin-bottom: 24px; animation: fadeUp 0.5s ease both; }
  @keyframes fadeUp { from { opacity: 0; transform: translateY(16px); } to { opacity: 1; transform: translateY(0); } }
  .stat-card { background: var(--surface); border: 1px solid var(--border); border-radius: 4px; padding: 20px; position: relative; overflow: hidden; }
  .stat-card::before { content: ''; position: absolute; top: 0; left: 0; right: 0; height: 1.5px; }
  .stat-card.green::before { background: var(--vault-green); } .stat-card.red::before { background: var(--vault-red); } .stat-card.amber::before { background: var(--vault-amber); } .stat-card.blue::before { background: #4488ff; }
  .stat-label { font-family: var(--mono); font-size: 9px; letter-spacing: 0.14em; color: var(--text-muted); text-transform: uppercase; margin-bottom: 10px; }
  .stat-value { font-family: var(--mono); font-size: 30px; font-weight: 400; line-height: 1; margin-bottom: 6px; }
  .stat-value.green { color: var(--vault-green); } .stat-value.red { color: var(--vault-red); } .stat-value.amber { color: var(--vault-amber); } .stat-value.blue { color: #4488ff; }
  .stat-sub { font-size: 11px; color: var(--text-muted); font-weight: 300; }
  
  .panel { background: var(--surface); border: 1px solid var(--border); border-radius: 4px; padding: 24px; margin-bottom: 20px; }
  .map-wrap { display: flex; align-items: center; gap: 15px; margin: 30px 0 20px; }
  .map-label { font-family: var(--mono); font-size: 10px; letter-spacing: 0.1em; color: var(--text-muted); font-weight: bold; }
  .map-track { flex: 1; height: 2px; background: rgba(90,98,114,0.3); position: relative; }
  .map-track::before, .map-track::after { content: ''; position: absolute; width: 6px; height: 14px; background: var(--border); top: -6px; }
  .map-track::before { left: 0; } .map-track::after { right: 0; }
  .map-person { position: absolute; top: 50%; right: 0; transform: translate(50%, -50%); width: 14px; height: 14px; background: var(--vault-amber); border-radius: 50%; box-shadow: 0 0 12px var(--vault-amber); transition: right 0.5s linear, opacity 0.3s; opacity: 0; }
  .map-person.breach { background: var(--vault-red); box-shadow: 0 0 15px var(--vault-red); }
  .map-status { text-align: center; font-family: var(--mono); font-size: 11px; color: var(--text-muted); letter-spacing: 0.05em; }

  .main-grid { display: grid; grid-template-columns: 1fr 380px; gap: 20px; margin-bottom: 28px; animation: fadeUp 0.5s 0.1s ease both; }
  .panel-title { font-family: var(--mono); font-size: 11px; letter-spacing: 0.1em; color: var(--text-muted); text-transform: uppercase; margin-bottom: 20px; padding-bottom: 14px; border-bottom: 1px solid var(--border); }
  .sensor-row { display: flex; align-items: center; justify-content: space-between; padding: 14px 0; border-bottom: 1px solid rgba(42,47,56,0.6); }
  .sensor-row:last-child { border-bottom: none; }
  .sensor-name { font-family: var(--mono); font-size: 12px; color: var(--text-muted); letter-spacing: 0.06em; }
  .sensor-right { display: flex; align-items: center; gap: 14px; }
  .sensor-val { font-family: var(--mono); font-size: 18px; min-width: 80px; text-align: right; }
  .sensor-bar-wrap { width: 100px; height: 4px; background: var(--surface2); border-radius: 2px; overflow: hidden; }
  .sensor-bar { height: 100%; border-radius: 2px; transition: width 0.6s ease; }
  .bar-green { background: var(--vault-green); } .bar-red { background: var(--vault-red); } .bar-amber { background: var(--vault-amber); }
  .request-panel { display: flex; flex-direction: column; gap: 16px; }
  .request-card { background: var(--surface2); border: 1px solid var(--border); border-radius: 4px; padding: 20px; transition: border-color 0.3s; }
  .request-card.pending { border-color: rgba(255,170,0,0.5); animation: glow-amber 2s ease-in-out infinite; }
  @keyframes glow-amber { 0%,100% { box-shadow: 0 0 0 0 rgba(255,170,0,0); } 50% { box-shadow: 0 0 16px 2px rgba(255,170,0,0.1); } }
  .req-header { display: flex; align-items: center; justify-content: space-between; margin-bottom: 14px; }
  .req-title { font-family: var(--mono); font-size: 11px; letter-spacing: 0.1em; color: var(--text-muted); text-transform: uppercase; }
  .req-badge { font-family: var(--mono); font-size: 10px; padding: 3px 10px; border-radius: 20px; letter-spacing: 0.08em; }
  .badge-pending { background: rgba(255,170,0,0.12); color: var(--vault-amber); border: 1px solid rgba(255,170,0,0.3); }
  .badge-none { background: rgba(90,98,114,0.15); color: var(--text-muted); border: 1px solid var(--border); }
  .badge-granted { background: rgba(0,255,136,0.1); color: var(--vault-green); border: 1px solid rgba(0,255,136,0.3); }
  .req-info { font-size: 13px; color: var(--text-muted); font-weight: 300; line-height: 1.6; margin-bottom: 18px; }
  .req-info span { color: var(--text); font-weight: 400; }
  .req-actions { display: flex; gap: 10px; }
  .btn-approve { flex: 1; padding: 11px; background: var(--vault-green); color: #0a0c0f; border: none; border-radius: 3px; font-family: var(--mono); font-size: 11px; letter-spacing: 0.1em; cursor: pointer; transition: background 0.2s, transform 0.1s; text-transform: uppercase; }
  .btn-approve:hover { background: var(--vault-green-dim); } .btn-approve:disabled { opacity: 0.3; pointer-events: none; }
  .btn-deny { flex: 1; padding: 11px; background: transparent; color: var(--vault-red); border: 1px solid rgba(255,51,68,0.35); border-radius: 3px; font-family: var(--mono); font-size: 11px; letter-spacing: 0.1em; cursor: pointer; transition: background 0.2s; text-transform: uppercase; }
  .btn-deny:hover { background: rgba(255,51,68,0.08); } .btn-deny:disabled { opacity: 0.3; pointer-events: none; }
  
  .btn-override { padding: 11px; background: transparent; color: var(--vault-amber); border: 1px solid rgba(255,170,0,0.35); border-radius: 3px; font-family: var(--mono); font-size: 11px; letter-spacing: 0.1em; cursor: pointer; transition: background 0.2s; text-transform: uppercase; width: 100%; margin-bottom: 10px;}
  .btn-override:hover { background: rgba(255,170,0,0.08); }

  .session-box { background: var(--surface); border: 1px solid var(--border); border-radius: 4px; padding: 20px; animation: fadeUp 0.5s 0.2s ease both; }
  .timer-display { font-family: var(--mono); font-size: 42px; color: var(--vault-amber); text-align: center; margin: 16px 0; letter-spacing: 0.04em; }
  .timer-display.inactive { color: var(--text-muted); font-size: 28px; }
  .timer-bar-wrap { height: 3px; background: var(--surface2); border-radius: 2px; overflow: hidden; margin-top: 10px; }
  .timer-bar { height: 100%; background: var(--vault-amber); border-radius: 2px; transition: width 1s linear; }
  .log-list { background: var(--surface); border: 1px solid var(--border); border-radius: 4px; padding: 16px; max-height: 180px; overflow-y: auto; font-family: var(--mono); font-size: 11px; line-height: 1.8; }
  .log-entry { display: flex; gap: 12px; } .log-time { color: var(--text-muted); min-width: 70px; }
  .log-msg.ok { color: var(--vault-green); } .log-msg.err { color: var(--vault-red); } .log-msg.warn { color: var(--vault-amber); } .log-msg.info { color: var(--text-muted); }
  .modal-overlay { display: none; position: fixed; inset: 0; background: rgba(0,0,0,0.85); backdrop-filter: blur(4px); z-index: 1000; align-items: center; justify-content: center; }
  .modal-overlay.active { display: flex; animation: flashRed 1s infinite alternate; }
  @keyframes flashRed { 0% { background: rgba(0,0,0,0.85); } 100% { background: rgba(60,0,0,0.9); } }
  .modal-card { background: var(--surface); border: 2px solid var(--vault-red); border-radius: 6px; padding: 40px; text-align: center; max-width: 500px; width: 90%; box-shadow: 0 0 30px rgba(255,51,68,0.3); }
  .modal-card h1 { font-family: var(--mono); color: var(--vault-red); font-size: 32px; letter-spacing: 0.1em; margin-bottom: 10px; }
  .modal-card p { font-size: 16px; color: var(--text); line-height: 1.5; margin-bottom: 30px; }
  .btn-modal { padding: 16px 30px; background: var(--vault-red); color: white; border: none; border-radius: 4px; font-family: var(--mono); font-size: 16px; font-weight: bold; cursor: pointer; letter-spacing: 0.1em; text-transform: uppercase; }
  .btn-modal:hover { background: #ff1a2e; box-shadow: 0 0 15px rgba(255,51,68,0.5); }
</style>
</head>
<body>

<div class="modal-overlay" id="alarmModal">
  <div class="modal-card">
    <h1>CRITICAL ALARM</h1>
    <p id="alarmModalText">Unauthorized activity detected.</p>
    <button class="btn-modal" onclick="clearAlarm()">MUTE ALARM & RESET SYSTEM</button>
  </div>
</div>

<nav>
  <div class="nav-left"><span class="nav-logo">VAULT CTRL</span><div class="nav-sep"></div><span class="nav-role">MANAGER VIEW</span></div>
  <div class="nav-right"><div class="status-pill secure" id="systemStatus"><div class="dot"></div><span>SECURE</span></div><span id="clock">--:--:--</span></div>
</nav>

<div class="page">
  <div class="section-title">Live Status</div>
  <div class="stats-row">
    <div class="stat-card green"><div class="stat-label">People Inside</div><div class="stat-value green" id="statPeople">0</div><div class="stat-sub">Vault occupancy</div></div>
    <div class="stat-card amber"><div class="stat-label">Distance to Safe</div><div class="stat-value amber" id="statDist">-- cm</div><div class="stat-sub">Radar sweeping sensor</div></div>
    <div class="stat-card red"><div class="stat-label">Light Level (LDR)</div><div class="stat-value red" id="statLight">--</div><div class="stat-sub">Safe lid status</div></div>
    <div class="stat-card blue"><div class="stat-label">Entry Status</div><div class="stat-value blue" id="statGate">LOCKED</div><div class="stat-sub">Biometric access</div></div>
  </div>

  <div class="panel" style="margin-bottom: 24px;">
    <div class="panel-title" style="margin-bottom: 0; border-bottom: none;">Radar Sweep Tracker</div>
    <div class="map-wrap">
      <div class="map-label">ENTRY</div>
      <div class="map-track">
        <div class="map-person" id="mapPerson"></div>
      </div>
      <div class="map-label">SAFE</div>
    </div>
    <div class="map-status" id="mapStatusText">Room is clear.</div>
  </div>

  <div class="main-grid">
    <div style="display:flex;flex-direction:column;gap:20px;">
      <div class="panel">
        <div class="panel-title">Sensor Readings</div>
        <div class="sensor-row"><span class="sensor-name">RADAR SWEEP — Distance</span><div class="sensor-right"><div class="sensor-bar-wrap"><div class="sensor-bar bar-amber" id="barDist" style="width:0%"></div></div><span class="sensor-val amber" id="sensorDist">-- cm</span></div></div>
        <div class="sensor-row"><span class="sensor-name">LDR — Light Level</span><div class="sensor-right"><div class="sensor-bar-wrap"><div class="sensor-bar bar-red" id="barLight" style="width:0%"></div></div><span class="sensor-val red" id="sensorLight">--</span></div></div>
        <div class="sensor-row"><span class="sensor-name">IR — Person Count</span><div class="sensor-right"><div class="sensor-bar-wrap"><div class="sensor-bar bar-green" id="barPeople" style="width:0%"></div></div><span class="sensor-val green" id="sensorPeople">0</span></div></div>
        <div class="sensor-row"><span class="sensor-name">ENTRY ALARM</span><div class="sensor-right"><span class="sensor-val" id="sensorGate" style="color:var(--vault-red)">ARMED</span></div></div>
        <div class="sensor-row"><span class="sensor-name">VAULT ALARM</span><div class="sensor-right"><span class="sensor-val" id="sensorVault" style="color:var(--vault-red)">ARMED</span></div></div>
      </div>
      <div class="session-box">
        <div class="section-title" style="margin-bottom:10px">Access Session</div>
        <div class="timer-display inactive" id="timerDisplay">NO SESSION</div>
        <div class="timer-bar-wrap"><div class="timer-bar" id="timerBar" style="width:0%"></div></div>
      </div>
    </div>
    <div class="request-panel">
      <div class="request-card" id="requestCard">
        <div class="req-header"><span class="req-title">Access Request</span><span class="req-badge badge-none" id="reqBadge">NO REQUEST</span></div>
        <p class="req-info" id="reqInfo">Waiting for client to submit a request...</p>
        <div class="req-actions"><button class="btn-approve" id="btnApprove" onclick="approveAccess()" disabled>APPROVE</button><button class="btn-deny" id="btnDeny" onclick="denyAccess()" disabled>DENY</button></div>
      </div>
      <div class="panel">
        <div class="panel-title">Manual Controls</div>
        <div style="display:flex;flex-direction:column;gap:10px;">
          <button class="btn-override" onclick="overrideGate()">🔓 EMERGENCY ENTRY OVERRIDE</button>
          <button class="btn-deny" onclick="forceReset()" style="width:100%;border-color:rgba(90,98,114,0.4);color:var(--text-muted)">FORCE RESET SYSTEM</button>
        </div>
      </div>
    </div>
  </div>
  <div class="log-panel"><div class="section-title">Event Log</div><div class="log-list" id="logList"></div></div>
</div>

<script>
  function tick() { document.getElementById('clock').textContent = new Date().toTimeString().slice(0,8); }
  tick(); setInterval(tick, 1000);

  const SESSION_DURATION = 30; 
  let sessionRemaining = 0; let sessionActive = false; let sessionInterval = null;
  const MAX_ROOM_LENGTH = 100; 

  function startSession() { sessionActive = true; sessionInterval = setInterval(tickSession, 1000); updateUI(); }
  function tickSession() {
    if(sessionRemaining > 0) sessionRemaining--;
    const m = Math.floor(sessionRemaining/60).toString().padStart(2,'0'), s = (sessionRemaining%60).toString().padStart(2,'0');
    document.getElementById('timerDisplay').textContent = m + ':' + s; document.getElementById('timerDisplay').className = 'timer-display';
    document.getElementById('timerBar').style.width = (sessionRemaining/SESSION_DURATION*100) + '%';
    if (sessionRemaining <= 0) endSession();
  }
  function endSession() { clearInterval(sessionInterval); sessionActive = false; sessionRemaining = 0; document.getElementById('timerDisplay').textContent = 'NO SESSION'; document.getElementById('timerDisplay').className = 'timer-display inactive'; document.getElementById('timerBar').style.width = '0%'; updateUI(); }

  let pendingRequest = false;
  function setPendingRequest(from) {
    pendingRequest = true; document.getElementById('requestCard').className = 'request-card pending';
    document.getElementById('reqBadge').textContent = 'PENDING'; document.getElementById('reqBadge').className = 'req-badge badge-pending';
    document.getElementById('reqInfo').innerHTML = 'Request from <span>' + from + '</span>';
    document.getElementById('btnApprove').disabled = false; document.getElementById('btnDeny').disabled = false;
  }
  function approveAccess() { pendingRequest = false; resetRequest(); fetch('/approve').catch(()=>{}); startSession(); }
  function denyAccess() { pendingRequest = false; resetRequest(); fetch('/deny').catch(()=>{}); }
  function resetRequest() { document.getElementById('requestCard').className = 'request-card'; document.getElementById('reqBadge').textContent = 'NO REQUEST'; document.getElementById('reqBadge').className = 'req-badge badge-none'; document.getElementById('reqInfo').textContent = 'Waiting for client to submit a request...'; document.getElementById('btnApprove').disabled = true; document.getElementById('btnDeny').disabled = true; }
  
  function forceReset() { if(confirm('Reset system?')) { endSession(); resetRequest(); fetch('/reset').catch(()=>{}); } }
  function overrideGate() { if(confirm('Force entry LEDs open and bypass alarms for 5 seconds?')) fetch('/override_gate').catch(()=>{}); }

  let isModalOpen = false;
  function triggerAlarmModal(type) {
    if (isModalOpen) return; 
    isModalOpen = true;
    const overlay = document.getElementById('alarmModal'); const text = document.getElementById('alarmModalText');
    if (type === "GATE") { text.innerHTML = "<strong>TAILGATING DETECTED:</strong><br><br>An unauthorized person has crossed the entry line without a valid biometric scan."; } 
    else if (type === "VAULT") { text.innerHTML = "<strong>VAULT PROXIMITY ALERT:</strong><br><br>An unauthorized person is too close to the safe, or the safe box was forced open!"; }
    overlay.classList.add('active');
  }
  function clearAlarm() { fetch('/reset').catch(()=>{}); document.getElementById('alarmModal').classList.remove('active'); isModalOpen = false; addLog('Manager muted alarm and reset system.', 'ok'); }

  function addLog(msg, type) {
    const el = document.createElement('div'); el.className = 'log-entry';
    el.innerHTML = '<span class="log-time">' + new Date().toTimeString().slice(0,8) + '</span><span class="log-msg ' + type + '">' + msg + '</span>';
    const list = document.getElementById('logList'); list.insertBefore(el, list.firstChild);
    if(list.children.length>20) list.removeChild(list.lastChild);
  }

  function updateSensors(data) {
    if (!data.sessionActive && sessionActive) {
      endSession(); 
      addLog('Session ended remotely (early exit/timeout).', 'info');
    } else if (data.sessionActive && !sessionActive) {
        startSession();
    }

    // Sync timer strictly with ESP32 payload
    if (data.sessionActive) {
      sessionRemaining = data.sessionSecs;
    }

    document.getElementById('statDist').textContent = data.distance + ' cm'; document.getElementById('sensorDist').textContent = data.distance + ' cm';
    document.getElementById('statLight').textContent = data.light; document.getElementById('sensorLight').textContent = data.light;
    document.getElementById('statPeople').textContent = data.people; document.getElementById('sensorPeople').textContent = data.people;
    if (data.requestPending && !pendingRequest && !sessionActive) setPendingRequest('Authorized IP');

    const personDot = document.getElementById('mapPerson');
    const mapStatus = document.getElementById('mapStatusText');
    
    if (data.people > 0 && data.distance < 999) {
      personDot.style.opacity = 1;
      let pct = (data.distance / MAX_ROOM_LENGTH) * 100;
      if (pct > 100) pct = 100; 
      personDot.style.right = pct + '%';
      
      if (data.lockdown) {
        personDot.className = 'map-person breach';
        mapStatus.textContent = "INTRUDER TRACKED AT " + data.distance + " cm";
        mapStatus.style.color = "var(--vault-red)";
      } else {
        personDot.className = 'map-person';
        mapStatus.textContent = "Authorized occupant at " + data.distance + " cm from safe.";
        mapStatus.style.color = "var(--vault-amber)";
      }
    } else {
      personDot.style.opacity = 0;
      mapStatus.textContent = "Room is clear.";
      mapStatus.style.color = "var(--text-muted)";
    }

    if (data.gateBreach) { triggerAlarmModal("GATE"); document.getElementById('statGate').textContent = "BREACHED"; document.getElementById('statGate').className = "stat-value red"; document.getElementById('sensorGate').textContent = "BREACHED!"; } 
    else { document.getElementById('statGate').textContent = data.gateOpen ? 'OPEN' : 'LOCKED'; document.getElementById('statGate').className = "stat-value blue"; document.getElementById('sensorGate').textContent = "ARMED"; }
    
    if (data.vaultBreach) { triggerAlarmModal("VAULT"); document.getElementById('sensorVault').textContent = "BREACHED!"; } 
    else { document.getElementById('sensorVault').textContent = sessionActive ? "DISARMED" : "ARMED"; }

    // Modal auto-clear strictly tied to physical room occupancy
    if (isModalOpen && data.people === 0) {
      document.getElementById('alarmModal').classList.remove('active'); 
      isModalOpen = false;
      addLog('Room empty. Alarm cleared.', 'ok');
    }

    if (data.lockdown) {
      document.getElementById('btnApprove').textContent = "SYSTEM LOCKED";
      document.getElementById('btnApprove').style.background = "var(--border)";
      document.getElementById('btnApprove').style.color = "var(--text-muted)";
      document.getElementById('btnApprove').disabled = true; 
    } else if (pendingRequest) {
      document.getElementById('btnApprove').textContent = "APPROVE";
      document.getElementById('btnApprove').style.background = "var(--vault-green)";
      document.getElementById('btnApprove').style.color = "#0a0c0f";
      document.getElementById('btnApprove').disabled = false;
    }
  }

  function updateUI() {
    const pill = document.getElementById('systemStatus');
    if (sessionActive) { pill.className = 'status-pill session'; pill.innerHTML = '<div class="dot"></div><span>SESSION ACTIVE</span>'; } 
    else { pill.className = 'status-pill secure'; pill.innerHTML = '<div class="dot"></div><span>SECURE</span>'; }
  }
  function pollESP32() { fetch('/status').then(r => r.json()).then(data => updateSensors(data)).catch(() => {}); }
  setInterval(pollESP32, 2000);
</script>
</body>
</html>
)rawliteral";
