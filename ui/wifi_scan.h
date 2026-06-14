#pragma once

// PART 1: trước chỗ inject data
static const char WP_PAGE_PART1[] = R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0">
<title>WiFi Setup · Sharp Edge</title>
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
  transition: opacity 0.3s;
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
.hd-dot.connected   { background: var(--success); box-shadow: 0 0 6px var(--success); }
.hd-dot.connecting  { background: var(--accent);  box-shadow: 0 0 6px var(--accent); animation: pulse 1s infinite; }

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

#net-list { display: flex; flex-direction: column; gap: 5px; margin-bottom: 1.25rem; }

.net {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 9px 11px;
  background: var(--surface2);
  border: 0.5px solid var(--border);
  border-radius: var(--radius);
  cursor: pointer;
  transition: border-color 0.15s, background 0.15s;
}
.net:hover  { border-color: var(--accent); }
.net.sel    { border-color: var(--accent); background: rgba(14,165,233,0.06); }
.net-name   { flex: 1; font-size: 13px; font-weight: 500; color: var(--text); }
.net-meta   { display: flex; align-items: center; gap: 7px; }
.lock       { font-size: 11px; color: var(--text-3); }
.bars       { display: flex; align-items: flex-end; gap: 2px; height: 13px; }
.bars span  { width: 3px; background: var(--accent); border-radius: 1px; }

.net-scan-placeholder {
  text-align: center;
  padding: 1.25rem;
  font-family: var(--font-mono);
  font-size: 11px;
  color: var(--text-3);
}

.div { height: 0.5px; background: var(--border); margin: 1.1rem 0; }

.fields { display: flex; flex-direction: column; gap: 10px; margin-bottom: 1.1rem; }
.field  { display: flex; flex-direction: column; gap: 4px; }

label {
  font-size: 11px;
  font-weight: 500;
  color: var(--text-2);
  letter-spacing: 0.03em;
}

input[type=text], input[type=password] {
  background: var(--surface2);
  border: 0.5px solid var(--border);
  border-radius: 4px;
  padding: 9px 12px;
  font-family: var(--font-sans);
  font-size: 13px;
  color: var(--text);
  outline: none;
  width: 100%;
  transition: border-color 0.15s;
}
input[type=text]:focus, input[type=password]:focus { border-color: var(--accent); }
input[type=text]::placeholder, input[type=password]::placeholder { color: var(--text-4); }

.check-row {
  display: flex; align-items: center; gap: 8px;
  font-size: 12px; color: var(--text-3);
  cursor: pointer;
  user-select: none;
}
.check-row input[type=checkbox] {
  width: 14px; height: 14px;
  accent-color: var(--accent);
  cursor: pointer;
}

.btn {
  width: 100%;
  padding: 11px;
  background: var(--accent);
  border: none;
  border-radius: 4px;
  font-family: var(--font-sans);
  font-size: 13px;
  font-weight: 500;
  color: #fff;
  cursor: pointer;
  letter-spacing: 0.02em;
  transition: background 0.15s, opacity 0.15s;
  margin-top: 0.25rem;
}
.btn:hover   { background: var(--accent-h); }
.btn:active  { opacity: 0.85; }
.btn:disabled { background: var(--border2); color: var(--text-3); cursor: default; }

#screen-status {
  display: none;
  flex-direction: column;
  align-items: center;
  text-align: center;
  padding: 2rem 1.5rem;
  gap: 12px;
}
.status-icon {
  width: 52px; height: 52px;
  border-radius: 12px;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 24px;
  font-weight: 500;
}
.status-icon.ok   { background: rgba(34,197,94,0.12);  color: var(--success); }
.status-icon.err  { background: rgba(239,68,68,0.12);  color: var(--danger); }
.status-icon.spin { background: rgba(14,165,233,0.12); color: var(--accent); animation: spin 1s linear infinite; }

@keyframes spin { to { transform: rotate(360deg); } }

.status-title { font-size: 15px; font-weight: 500; }
.status-msg   { font-size: 12px; color: var(--text-3); font-family: var(--font-mono); }

.btn-ghost {
  background: transparent;
  border: 0.5px solid var(--border);
  border-radius: 4px;
  padding: 8px 20px;
  font-family: var(--font-sans);
  font-size: 12px;
  color: var(--text-3);
  cursor: pointer;
  margin-top: 4px;
  transition: color 0.15s, border-color 0.15s;
}
.btn-ghost:hover { color: var(--accent); border-color: var(--accent); }

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
.btn-rescan {
  font-family: var(--font-mono);
  font-size: 10px;
  color: var(--text-3);
  background: transparent;
  border: 0.5px solid var(--border);
  border-radius: 3px;
  padding: 3px 8px;
  cursor: pointer;
  letter-spacing: 0.04em;
  transition: color 0.15s, border-color 0.15s;
}
.btn-rescan:hover { color: var(--accent); border-color: var(--accent); }
</style>
</head>
<body>
<div class="card">
  <div class="hd">
    <div class="hd-icon" id="sig-bars">
      <span style="height:6px"  id="b1"></span>
      <span style="height:10px" id="b2"></span>
      <span style="height:16px" id="b3"></span>
      <span style="height:22px" id="b4"></span>
    </div>
    <div class="hd-info">
      <div class="hd-title">WiFi Setup</div>
      <div class="hd-sub" id="hd-ip">WIFIPANEL &middot; PORTAL</div>
    </div>
    <div class="hd-dot" id="status-dot"></div>
  </div>
  <div id="screen-form">
    <div class="body">
      <div class="sec">Available networks</div>
      <div id="net-list">
        <div class="net-scan-placeholder" id="scan-placeholder">↻ Scanning...</div>
      </div>
      <div class="div"></div>
      <div class="sec">Connection details</div>
      <div class="fields">
        <div class="field">
          <label for="s">SSID</label>
          <input type="text" id="s" name="s" value="" placeholder="WiFi name" autocomplete="off" />
        </div>
        <div class="field">
          <label for="p">Password</label>
          <input type="password" id="p" name="p" value="" placeholder="••••••••" autocomplete="off" />
          <label class="check-row" style="margin-top:4px">
            <input type="checkbox" id="show-pw" onchange="togglePw(this)"> Show password
          </label>
        </div>
      </div>
      <button class="btn" id="btn-connect" onclick="doConnect()">Connect</button>
    </div>
    <div class="ft">
      <span class="ft-chip">WIFIPANEL PORTAL</span>
      <button class="btn-rescan" onclick="doRescan()">&#x21BB; Rescan</button>
    </div>
  </div>
  <div id="screen-status">
    <div class="status-icon spin" id="st-icon">&#9675;</div>
    <div class="status-title"    id="st-title">Connecting...</div>
    <div class="status-msg"      id="st-msg">Please wait</div>
    <button class="btn-ghost"    id="st-back" onclick="showForm()" style="display:none">&#8592; Back</button>
  </div>
</div>
<script>
)rawhtml";

// PART 2: sau chỗ inject — phần còn lại của JS + đóng tag
static const char WP_PAGE_PART2[] = R"rawhtml(
var NETWORKS=(function(){
  if(window.wifiscandata&&Array.isArray(window.wifiscandata))return window.wifiscandata;
  return [];
})();
function rssiToBars(r){if(r>-55)return 4;if(r>-65)return 3;if(r>-75)return 2;return 1;}
function renderBars(n){var h=[5,9,13,17],s='';for(var i=0;i<4;i++)s+='<span style="height:'+h[i]+'px;opacity:'+(i<n?'1':'0.18')+'"></span>';return s;}
function escSSID(s){if(!s)return'';return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;').replace(/'/g,'&#39;');}
function buildNetList(){
  var list=document.getElementById('net-list');
  if(!NETWORKS||NETWORKS.length===0){list.innerHTML='<div class="net-scan-placeholder">No networks found</div>';return;}
  var sorted=NETWORKS.slice().sort(function(a,b){return b.rssi-a.rssi;});
  var html='';
  sorted.forEach(function(n,idx){
    var bars=rssiToBars(n.rssi);
    var lock=n.enc?'<span class="lock" title="Secured">&#128274;</span>':'';
    var safe=escSSID(n.ssid);
    html+='<div class="net'+(idx===0?' sel':'')+'" onclick="selectNet(this,\''+safe.replace(/'/g,"\\'")+'\')">'
      +'<span class="net-name">'+safe+'</span>'
      +'<div class="net-meta">'+lock+'<div class="bars">'+renderBars(bars)+'</div></div>'
      +'</div>';
  });
  list.innerHTML=html;
  if(sorted.length>0)document.getElementById('s').value=sorted[0].ssid;
}
function selectNet(el,ssid){document.querySelectorAll('.net').forEach(function(n){n.classList.remove('sel');});el.classList.add('sel');document.getElementById('s').value=ssid;document.getElementById('p').value='';document.getElementById('p').focus();}
function togglePw(cb){document.getElementById('p').type=cb.checked?'text':'password';}
function doRescan(){var b=document.querySelector('.btn-rescan');b.textContent='Scanning...';setTimeout(function(){window.location.href='/';},600);}
function showForm(){document.getElementById('screen-form').style.display='';document.getElementById('screen-status').style.display='none';setDot('idle');}
function setDot(s){var d=document.getElementById('status-dot');d.className='hd-dot'+(s==='ok'?' connected':s==='connecting'?' connecting':'');}
function doConnect(){
  var ssid=document.getElementById('s').value.trim();
  var pass=document.getElementById('p').value;
  if(!ssid){document.getElementById('s').focus();document.getElementById('s').style.borderColor='var(--danger)';return;}
  document.getElementById('screen-form').style.display='none';
  var ss=document.getElementById('screen-status');ss.style.display='flex';
  document.getElementById('st-icon').textContent='◌';document.getElementById('st-icon').className='status-icon spin';
  document.getElementById('st-title').textContent='Connecting...';
  document.getElementById('st-msg').textContent=ssid;
  document.getElementById('st-back').style.display='none';
  setDot('connecting');
  var fd=new FormData();fd.append('ssid',ssid);fd.append('password',pass);
  fetch('/',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'ssid='+encodeURIComponent(ssid)+'&password='+encodeURIComponent(pass)})
    .then(function(){
      document.getElementById('st-icon').textContent='✓';document.getElementById('st-icon').className='status-icon ok';
      document.getElementById('st-title').textContent='Submitted!';
      document.getElementById('st-msg').textContent='Device is connecting to '+ssid;
      setDot('ok');
    }).catch(function(){
      document.getElementById('st-icon').textContent='✕';document.getElementById('st-icon').className='status-icon err';
      document.getElementById('st-title').textContent='Error';
      document.getElementById('st-msg').textContent='Could not reach device';
      document.getElementById('st-back').style.display='inline-block';
      setDot('idle');
    });
}
buildNetList();setDot('idle');
document.getElementById('s').addEventListener('input',function(){this.style.borderColor='';});
</script>
</body>
</html>
)rawhtml";