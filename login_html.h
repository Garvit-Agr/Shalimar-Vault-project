const char* login_html PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>Vault Control — Login</title>
<link href="https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Barlow:wght@300;400;600&display=swap" rel="stylesheet">
<style>
  :root {
    --vault-green: #00ff88; --vault-green-dim: #00cc6a;
    --vault-red: #ff3344; --bg: #0a0c0f; --surface: #111418;
    --surface2: #1a1e24; --border: #2a2f38; --text: #e8edf5;
    --text-muted: #5a6272; --mono: 'Share Tech Mono', monospace; --sans: 'Barlow', sans-serif;
  }
  * { margin: 0; padding: 0; box-sizing: border-box; }
  body { background: var(--bg); color: var(--text); font-family: var(--sans); min-height: 100vh; display: flex; align-items: center; justify-content: center; overflow: hidden; }
  body::before { content: ''; position: fixed; inset: 0; background-image: linear-gradient(var(--border) 1px, transparent 1px), linear-gradient(90deg, var(--border) 1px, transparent 1px); background-size: 40px 40px; opacity: 0.3; pointer-events: none; }
  .corner { position: fixed; width: 60px; height: 60px; border-color: var(--vault-green); border-style: solid; opacity: 0.4; }
  .corner.tl { top: 20px; left: 20px; border-width: 2px 0 0 2px; } .corner.tr { top: 20px; right: 20px; border-width: 2px 2px 0 0; }
  .corner.bl { bottom: 20px; left: 20px; border-width: 0 0 2px 2px; } .corner.br { bottom: 20px; right: 20px; border-width: 0 2px 2px 0; }
  .login-wrap { position: relative; width: 100%; max-width: 420px; padding: 20px; animation: fadeUp 0.6s ease both; }
  @keyframes fadeUp { from { opacity: 0; transform: translateY(24px); } to { opacity: 1; transform: translateY(0); } }
  .status-bar { display: flex; align-items: center; gap: 8px; margin-bottom: 32px; font-family: var(--mono); font-size: 11px; color: var(--text-muted); letter-spacing: 0.08em; }
  .status-dot { width: 7px; height: 7px; border-radius: 50%; background: var(--vault-green); animation: pulse 2s ease-in-out infinite; }
  @keyframes pulse { 0%, 100% { opacity: 1; box-shadow: 0 0 0 0 rgba(0,255,136,0.4); } 50% { opacity: 0.7; box-shadow: 0 0 0 6px rgba(0,255,136,0); } }
  .card { background: var(--surface); border: 1px solid var(--border); border-radius: 4px; padding: 40px 36px 36px; position: relative; }
  .card::before { content: ''; position: absolute; top: 0; left: 0; right: 0; height: 2px; background: linear-gradient(90deg, transparent, var(--vault-green), transparent); }
  .vault-icon { width: 52px; height: 52px; border: 1.5px solid var(--vault-green); border-radius: 4px; display: flex; align-items: center; justify-content: center; margin-bottom: 24px; }
  .vault-icon svg { width: 26px; height: 26px; stroke: var(--vault-green); fill: none; stroke-width: 1.5; }
  h1 { font-family: var(--mono); font-size: 20px; font-weight: 400; letter-spacing: 0.04em; color: var(--text); margin-bottom: 4px; }
  .subtitle { font-size: 13px; color: var(--text-muted); margin-bottom: 36px; font-weight: 300; letter-spacing: 0.02em; }
  .field { margin-bottom: 20px; } label { display: block; font-family: var(--mono); font-size: 10px; letter-spacing: 0.12em; color: var(--text-muted); margin-bottom: 8px; text-transform: uppercase; }
  input { width: 100%; background: var(--surface2); border: 1px solid var(--border); border-radius: 3px; padding: 12px 16px; color: var(--text); font-family: var(--mono); font-size: 14px; outline: none; transition: border-color 0.2s, box-shadow 0.2s; }
  input:focus { border-color: var(--vault-green); box-shadow: 0 0 0 3px rgba(0,255,136,0.08); } input::placeholder { color: var(--text-muted); }
  .btn { width: 100%; margin-top: 28px; padding: 14px; background: var(--vault-green); color: #0a0c0f; border: none; border-radius: 3px; font-family: var(--mono); font-size: 13px; letter-spacing: 0.1em; font-weight: 400; cursor: pointer; transition: background 0.2s, transform 0.1s; text-transform: uppercase; }
  .btn:hover { background: var(--vault-green-dim); } .btn:active { transform: scale(0.99); } .btn.loading { opacity: 0.6; pointer-events: none; }
  .error-msg { display: none; margin-top: 16px; padding: 10px 14px; background: rgba(255,51,68,0.08); border: 1px solid rgba(255,51,68,0.3); border-radius: 3px; font-family: var(--mono); font-size: 12px; color: var(--vault-red); letter-spacing: 0.04em; }
  .error-msg.show { display: block; animation: shake 0.3s ease; }
  @keyframes shake { 0%,100% { transform: translateX(0); } 25% { transform: translateX(-6px); } 75% { transform: translateX(6px); } }
  .footer-note { margin-top: 20px; text-align: center; font-size: 11px; color: var(--text-muted); font-family: var(--mono); letter-spacing: 0.06em; }

  /* --- ADDED: MOBILE RESPONSIVE DESIGN --- */
  @media (max-width: 480px) {
    .card { padding: 30px 20px 24px; }
    .corner { width: 40px; height: 40px; }
    .corner.tl { top: 10px; left: 10px; }
    .corner.tr { top: 10px; right: 10px; }
    .corner.bl { bottom: 10px; left: 10px; }
    .corner.br { bottom: 10px; right: 10px; }
    h1 { font-size: 18px; }
  }
</style>
</head>
<body>

<div class="corner tl"></div><div class="corner tr"></div><div class="corner bl"></div><div class="corner br"></div>
<div class="login-wrap">
  <div class="status-bar"><div class="status-dot"></div><span>VAULT SYSTEM ONLINE</span><span style="margin-left:auto;" id="clock">--:--:--</span></div>
  <div class="card">
    <div class="vault-icon"><svg viewBox="0 0 24 24"><rect x="3" y="4" width="18" height="16" rx="2"/><circle cx="12" cy="12" r="3"/><line x1="12" y1="9" x2="12" y2="7"/><line x1="12" y1="15" x2="12" y2="17"/><line x1="9" y1="12" x2="7" y2="12"/><line x1="15" y1="12" x2="17" y2="12"/></svg></div>
    <h1>VAULT CONTROL</h1><p class="subtitle">Manager authentication required</p>
    <div class="field"><label>Username</label><input type="text" id="username" placeholder="Enter username" autocomplete="off" /></div>
    <div class="field"><label>Password</label><input type="password" id="password" placeholder="Enter password" /></div>
    <button class="btn" id="loginBtn" onclick="doLogin()">AUTHENTICATE</button>
    <div class="error-msg" id="errorMsg">ACCESS DENIED — Invalid credentials</div>
  </div>
  <p class="footer-note">IIIT-H VAULT SECURITY SYSTEM v1.0</p>
</div>

<script>
  function tick() { document.getElementById('clock').textContent = new Date().toTimeString().slice(0,8); }
  tick(); setInterval(tick, 1000);

  function doLogin() {
    const u = document.getElementById('username').value.trim();
    const p = document.getElementById('password').value.trim();
    const btn = document.getElementById('loginBtn');
    const err = document.getElementById('errorMsg');

    err.classList.remove('show'); btn.textContent = 'VERIFYING...'; btn.classList.add('loading');
    setTimeout(() => {
      fetch('/auth', { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body: 'user=' + encodeURIComponent(u) + '&pass=' + encodeURIComponent(p) })
      .then(r => r.json()).then(data => { if (data.ok) { window.location.href = '/manager?v=' + new Date().getTime(); } else { showError(btn); } }).catch(() => showError(btn));
    }, 800);
  }
  function showError(btn) { document.getElementById('errorMsg').classList.add('show'); btn.textContent = 'AUTHENTICATE'; btn.classList.remove('loading'); }
  document.addEventListener('keydown', e => { if (e.key === 'Enter') doLogin(); });
</script>
</body>
</html>
)rawliteral";