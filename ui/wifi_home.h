#pragma once

// Auto-generated from home.html
// Usage: server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){
//   r->send_P(200, "text/html", HOME_HTML);
// });

static const char HOME_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0">
<title>Home · Sharp Edge</title>
<style>
@import url('https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;500&family=DM+Sans:ital,wght@0,300;0,400;0,500;1,400&display=swap');

*, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

:root {
  --bg:        #0a0e14;
  --surface:   #111820;
  --surface2:  #0d1520;
  --border:    #1e2d3d;
  --border2:   #2a3d52;
  --accent:    #0ea5e9;
  --accent-h:  #38bdf8;
  --text:      #e2eaf2;
  --text-2:    #7bafc8;
  --text-3:    #3d5470;
  --text-4:    #2a3d52;
  --success:   #22c55e;
  --warning:   #f97316;
  --danger:    #ef4444;
  --font-sans: 'DM Sans', system-ui, sans-serif;
  --font-mono: 'IBM Plex Mono', monospace;
  --radius:    3px;
  --radius-lg: 6px;
}

html, body {
  min-height: 100%;
  background: var(--bg);
  color: var(--text);
  font-family: var(--font-sans);
  font-size: 14px;
  line-height: 1.5;
  -webkit-font-smoothing: antialiased;
}

body {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: flex-start;
  padding: 1.5rem 1rem 3rem;
  min-height: 100vh;
}

.card {
  width: 100%;
  max-width: 420px;
  background: var(--surface);
  border: 0.5px solid var(--border);
  border-radius: var(--radius-lg);
  overflow: hidden;
}

.hd {
  background: var(--surface2);
  border-bottom: 0.5px solid var(--border);
  padding: 1.25rem 1.5rem 1rem;
  display: flex;
  align-items: center;
  gap: 12px;
}
.hd-icon {
  display: flex;
  align-items: flex-end;
  justify-content: center;
  gap: 3px;
  padding-bottom: 3px;
  height: 32px;
}
.hd-icon span {
  width: 5px;
  background: var(--accent);
  border-radius: 1px;
}
.hd-info { flex: 1; }
.hd-title { font-size: 15px; font-weight: 500; color: var(--text); letter-spacing: 0.01em; }
.hd-sub {
  font-family: var(--font-mono);
  font-size: 10px;
  color: var(--text-3);
  margin-top: 2px;
}
.hd-dot {
  width: 8px; height: 8px;
  border-radius: 2px;
  background: var(--warning);
  flex-shrink: 0;
  transition: background 0.4s, box-shadow 0.4s;
}
.hd-dot.connected  { background: var(--success); box-shadow: 0 0 6px var(--success); }
.hd-dot.connecting { background: var(--accent);  box-shadow: 0 0 6px var(--accent); animation: pulse 1s infinite; }

@keyframes pulse { 0%,100%{opacity:1} 50%{opacity:0.4} }

.body { padding: 1.25rem 1.5rem; }

.sec {
  font-family: var(--font-mono);
  font-size: 10px;
  letter-spacing: 0.12em;
  text-transform: uppercase;
  color: var(--text-3);
  margin-bottom: 0.65rem;
}

.div { height: 0.5px; background: var(--border); margin: 1.1rem 0; }

.status-bar {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 10px 12px;
  background: var(--surface2);
  border: 0.5px solid var(--border);
  border-radius: var(--radius);
  margin-bottom: 1.25rem;
}
.status-bar.online  { border-color: rgba(34,197,94,0.3);  background: rgba(34,197,94,0.04); }
.status-bar.offline { border-color: rgba(239,68,68,0.3);  background: rgba(239,68,68,0.04); }
.status-led {
  width: 7px; height: 7px;
  border-radius: 2px;
  background: var(--text-4);
  flex-shrink: 0;
}
.status-bar.online  .status-led { background: var(--success); box-shadow: 0 0 5px var(--success); }
.status-bar.offline .status-led { background: var(--danger);  box-shadow: 0 0 5px var(--danger); }
.status-info { flex: 1; }
.status-label {
  font-size: 12px;
  font-weight: 500;
  color: var(--text);
}
.status-detail {
  font-family: var(--font-mono);
  font-size: 10px;
  color: var(--text-3);
  margin-top: 1px;
}

.nav-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 8px;
  margin-bottom: 1.25rem;
}

.nav-card {
  display: flex;
  flex-direction: column;
  gap: 10px;
  padding: 14px 13px;
  background: var(--surface2);
  border: 0.5px solid var(--border);
  border-radius: var(--radius);
  cursor: pointer;
  text-decoration: none;
  transition: border-color 0.15s, background 0.15s, transform 0.1s;
  position: relative;
  overflow: hidden;
}
.nav-card::before {
  content: '';
  position: absolute;
  top: 0; left: 0; right: 0;
  height: 1.5px;
  background: transparent;
  transition: background 0.15s;
}
.nav-card:hover { border-color: var(--accent); background: rgba(14,165,233,0.05); }
.nav-card:hover::before { background: var(--accent); }
.nav-card:active { transform: scale(0.98); }

.nav-card.wide {
  grid-column: 1 / -1;
  flex-direction: row;
  align-items: center;
}

.nav-icon { font-size: 20px; line-height: 1; }
.nav-label { font-size: 13px; font-weight: 500; color: var(--text); letter-spacing: 0.01em; }
.nav-desc {
  font-family: var(--font-mono);
  font-size: 10px;
  color: var(--text-3);
  margin-top: 2px;
}
.nav-arrow {
  margin-left: auto;
  font-size: 12px;
  color: var(--text-4);
  transition: color 0.15s, transform 0.15s;
}
.nav-card:hover .nav-arrow { color: var(--accent); transform: translateX(2px); }

.nav-badge {
  font-family: var(--font-mono);
  font-size: 9px;
  letter-spacing: 0.06em;
  padding: 2px 5px;
  border-radius: 2px;
  background: rgba(14,165,233,0.15);
  color: var(--accent);
  margin-top: auto;
  align-self: flex-start;
}
.nav-badge.warn { background: rgba(249,115,22,0.15); color: var(--warning); }

.info-table { display: flex; flex-direction: column; gap: 6px; }
.info-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 7px 10px;
  background: var(--surface2);
  border: 0.5px solid var(--border);
  border-radius: var(--radius);
}
.info-key {
  font-family: var(--font-mono);
  font-size: 10px;
  color: var(--text-3);
  letter-spacing: 0.06em;
  text-transform: uppercase;
}
.info-val {
  font-family: var(--font-mono);
  font-size: 11px;
  color: var(--text-2);
}
.info-val.ok  { color: var(--success); }
.info-val.err { color: var(--danger); }

.ft {
  padding: 0.6rem 1.5rem 1.1rem;
  display: flex;
  align-items: center;
  justify-content: space-between;
}
.ft-chip {
  font-family: var(--font-mono);
  font-size: 10px;
  color: var(--text-4);
  letter-spacing: 0.04em;
}
.ft-uptime {
  font-family: var(--font-mono);
  font-size: 10px;
  color: var(--text-3);
}
</style>
</head>
<body>

<div class="card">

  <div class="hd">
    <div class="hd-icon">
      <span style="height:6px"></span>
      <span style="height:10px"></span>
      <span style="height:16px"></span>
      <span style="height:22px"></span>
    </div>
    <div class="hd-info">
      <div class="hd-title">Sharp Edge</div>
      <div class="hd-sub" id="hd-sub">WIFIMANAGER &middot; v2.0.0</div>
    </div>
    <div class="hd-dot" id="status-dot"></div>
  </div>

  <div class="body">

    <div class="sec">Status</div>
    <div class="status-bar" id="status-bar">
      <div class="status-led"></div>
      <div class="status-info">
        <div class="status-label" id="status-label">Not connected</div>
        <div class="status-detail" id="status-detail">No WiFi configured</div>
      </div>
    </div>

    <div class="sec">Navigation</div>
    <div class="nav-grid">

      <a class="nav-card" href="/scan">
        <div class="nav-icon">&#128225;</div>
        <div>
          <div class="nav-label">WiFi Setup</div>
          <div class="nav-desc">Scan &amp; connect</div>
        </div>
        <div class="nav-badge" id="badge-networks">SCAN</div>
      </a>

      <a class="nav-card" href="/config">
        <div class="nav-icon">&#9881;</div>
        <div>
          <div class="nav-label">Config</div>
          <div class="nav-desc">Device settings</div>
        </div>
        <div class="nav-badge warn">PARAM</div>
      </a>

      <a class="nav-card" href="/info">
        <div class="nav-icon">&#8505;</div>
        <div>
          <div class="nav-label">System Info</div>
          <div class="nav-desc">Chip, memory, uptime</div>
        </div>
        <div class="nav-badge">SYS</div>
      </a>

      <a class="nav-card" href="/ota">
        <div class="nav-icon">&#11014;</div>
        <div>
          <div class="nav-label">OTA Update</div>
          <div class="nav-desc">Flash firmware</div>
        </div>
        <div class="nav-badge warn">OTA</div>
      </a>

      <a class="nav-card wide" href="/reset" id="btn-reset">
        <div class="nav-icon">&#8634;</div>
        <div>
          <div class="nav-label">Reset Settings</div>
          <div class="nav-desc">Erase saved WiFi &amp; config &mdash; device will reboot into AP mode</div>
        </div>
        <div class="nav-arrow">&#8250;</div>
      </a>

    </div>

    <div class="div"></div>

    <div class="sec">Device</div>
    <div class="info-table">
      <div class="info-row">
        <span class="info-key">Hostname</span>
        <span class="info-val" id="info-host">%HOSTNAME%</span>
      </div>
      <div class="info-row">
        <span class="info-key">IP Address</span>
        <span class="info-val" id="info-ip">%IP%</span>
      </div>
      <div class="info-row">
        <span class="info-key">SSID</span>
        <span class="info-val" id="info-ssid">%SSID%</span>
      </div>
      <div class="info-row">
        <span class="info-key">Mode</span>
        <span class="info-val" id="info-mode">%MODE%</span>
      </div>
    </div>

  </div>

  <div class="ft">
    <span class="ft-chip">WIFIMANAGER PORTAL</span>
    <span class="ft-uptime" id="ft-uptime">up 0s</span>
  </div>

</div>

<script>
(function(){
  var connected = (%CONNECTED%);
  var dot = document.getElementById('status-dot');
  var bar = document.getElementById('status-bar');
  var cnt = (%SCANCOUNT%);

  if(connected){
    dot.className = 'hd-dot connected';
    bar.className = 'status-bar online';
    document.getElementById('status-label').textContent  = document.getElementById('info-ssid').textContent || 'Connected';
    document.getElementById('status-detail').textContent = document.getElementById('info-ip').textContent   || 'Online';
    document.getElementById('info-ip').className = 'info-val ok';
    document.getElementById('hd-sub').textContent = 'WIFIMANAGER \u00B7 ' + document.getElementById('info-ip').textContent;
  } else {
    dot.className = 'hd-dot';
    bar.className = 'status-bar offline';
    document.getElementById('status-label').textContent  = 'Not connected';
    document.getElementById('status-detail').textContent = 'Running in AP mode';
    document.getElementById('info-ip').className = 'info-val err';
  }

  if(cnt > 0){
    document.getElementById('badge-networks').textContent = cnt + ' net' + (cnt !== 1 ? 's' : '');
  }

  var t0 = Date.now();
  function fmt(ms){
    var s = Math.floor(ms/1000);
    if(s < 60) return 'up ' + s + 's';
    var m = Math.floor(s/60); s = s%60;
    if(m < 60) return 'up ' + m + 'm ' + s + 's';
    var h = Math.floor(m/60); m = m%60;
    return 'up ' + h + 'h ' + m + 'm';
  }
  setInterval(function(){
    document.getElementById('ft-uptime').textContent = fmt(Date.now() - t0);
  }, 1000);

  document.getElementById('btn-reset').addEventListener('click', function(e){
    if(!confirm('Reset all settings?\nDevice will reboot into AP mode.')) e.preventDefault();
  });
})();
</script>
</body>
</html>
)rawliteral";