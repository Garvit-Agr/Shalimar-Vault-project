const char* client_html PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>Vault Access</title>
<link href="https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Barlow:wght@300;400;600&display=swap" rel="stylesheet">
<style>
  :root { --v-green: #00ff88; --v-red: #ff3344; --v-amber: #ffaa00; --bg: #0a0c0f; --surf: #111418; --text: #e8edf5; --mono: 'Share Tech Mono', monospace; }
  * { margin: 0; padding: 0; box-sizing: border-box; }
  body { background: var(--bg); color: var(--text); font-family: 'Barlow', sans-serif; display: flex; align-items: center; justify-content: center; min-height: 100vh; padding: 20px; }
  .card { background: var(--surf); border: 1px solid #2a2f38; padding: 32px; border-radius: 4px; width: 100%; max-width: 400px; text-align: center; }
  .screen { display: none; } .screen.active { display: block; }
  h2 { font-family: var(--mono); margin-bottom: 20px; text-transform: uppercase; letter-spacing: 2px; }
  .btn { width: 100%; padding: 14px; border: none; border-radius: 3px; font-family: var(--mono); cursor: pointer; text-transform: uppercase; transition: 0.2s; margin-top: 15px; }
  .btn-amber { background: var(--v-amber); color: #000; }
  .btn-outline { background: transparent; border: 1px solid #2a2f38; color: #5a6272; }
  input { width: 100%; background: #1a1e24; border: 1px solid #2a2f38; padding: 12px; color: #fff; margin-bottom: 10px; font-family: var(--mono); outline: none; }
  .timer { font-size: 48px; color: var(--v-green); font-family: var(--mono); margin: 20px 0; }

  /* --- ADDED: MOBILE RESPONSIVE DESIGN --- */
  @media (max-width: 400px) {
    body { padding: 10px; }
    .card { padding: 24px 16px; }
    .timer { font-size: 40px; }
  }
</style>
</head>
<body>
<div class="card">
  <div class="screen active" id="sStatus">
    <h2 id="statusTitle">Vault Panel</h2>
    <div style="text-align:left; font-family:var(--mono); font-size:12px; color:#5a6272; margin-bottom:20px;">
        PPL INSIDE: <span id="vPpl" style="color:#fff">0</span><br>
        ALARM: <span id="vAlm" style="color:#fff">SECURE</span>
    </div>
    <button class="btn btn-amber" id="reqBtn" onclick="showP()">REQUEST ACCESS</button>
  </div>

  <div class="screen" id="sPass">
    <h2>Enter Code</h2>
    <input type="password" id="pIn" placeholder="PASSWORD" />
    <button class="btn btn-amber" onclick="subP()">SUBMIT</button>
    <button class="btn btn-outline" onclick="goB()">BACK</button>
  </div>

  <div class="screen" id="sWait">
    <h2>Awaiting...</h2>
    <p style="color:#5a6272">Manager review in progress.</p>
  </div>

  <div class="screen" id="sGrant">
    <h2 style="color:var(--v-green)">GRANTED</h2>
    <div class="timer" id="timer">00:30</div>
    <button class="btn" style="background:var(--v-red); color:#fff;" onclick="secureVault()">SECURE VAULT NOW</button>
  </div>

  <div class="screen" id="sEgress">
    <h2 style="color:var(--v-amber)">EXIT VAULT</h2>
    <p style="color:#5a6272; font-size:14px; margin-bottom: 10px;">Safe locked. Walk through the gate now.</p>
    <div class="timer" id="egressTimer" style="color:var(--v-amber);">10</div>
  </div>

  <div class="screen" id="sBreach">
    <h2 style="color:var(--v-red)">LOCKDOWN</h2>
    <p>Unauthorized access detected.</p>
  </div>
</div>

<script>
  let sActive = false;
  let isBreached = false;

  function showS(id) { 
    document.querySelectorAll('.screen').forEach(s=>s.classList.remove('active')); 
    document.getElementById(id).classList.add('active'); 
  }
  
  function showP() { showS('sPass'); }
  function goB() { showS('sStatus'); }

  function subP() {
    const p = document.getElementById('pIn').value;
    fetch('/request', { method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:'pass='+encodeURIComponent(p) })
    .then(r => {
        if(r.ok) showS('sWait');
        else alert("Request Blocked: Vault Busy or Unauthorized");
    });
  }

  function secureVault() {
    if(confirm("Initiate exit sequence? You will have 10 seconds to cross the threshold.")) {
      fetch('/end_session', { method: 'POST' }).then(r => {
        if(r.ok) {
          sActive = false;
          showS('sEgress');
        }
      });
    }
  }

  function poll() {
    fetch('/status').then(r=>r.json()).then(d=>{
      if(d.lockdown || d.gateBreach || d.vaultBreach) {
          if(!isBreached) { isBreached = true; showS('sBreach'); }
          return; 
      } 
      
      if(isBreached && !d.lockdown && !d.systemBreach) {
          isBreached = false; showS('sStatus'); 
      }

      if(d.isEgressing) {
          showS('sEgress');
          document.getElementById('egressTimer').textContent = d.egressSecs;
          if (sActive) sActive = false;
          return;
      } else if (document.getElementById('sEgress').classList.contains('active')) {
          showS('sStatus'); 
      }

      if(d.sessionActive) {
          if(!sActive) { sActive = true; showS('sGrant'); }
          document.getElementById('timer').textContent = "00:" + d.sessionSecs.toString().padStart(2,'0');
      } else {
          if(sActive) { sActive = false; showS('sStatus'); }
      }

      const isWaiting = document.getElementById('sWait').classList.contains('active');
      const isGranted = document.getElementById('sGrant').classList.contains('active');

      if(!d.requestPending && isWaiting) showS('sStatus');

      document.getElementById('vPpl').textContent = d.people;
      document.getElementById('vAlm').textContent = (d.systemBreach || d.lockdown) ? "BREACHED" : "SECURE";
      
      const btn = document.getElementById('reqBtn');
      if(d.sessionActive && !isGranted) { 
          btn.textContent = "VAULT IN USE"; 
          btn.disabled = true; btn.style.opacity = "0.5";
      }
      else if(d.lockdown) { 
          btn.textContent = "SYSTEM LOCKED"; 
          btn.disabled = true; btn.style.opacity = "0.5";
      }
      else if(d.requestPending && !isWaiting) { 
          btn.textContent = "PENDING..."; 
          btn.disabled = true; btn.style.opacity = "0.5";
      }
      else { 
          btn.textContent = "REQUEST ACCESS"; 
          btn.disabled = false; btn.style.opacity = "1";
      }
    }).catch(err => console.error("Poll error:", err));
  }

  setInterval(poll, 1000);
</script>
</body>
</html>
)rawliteral";