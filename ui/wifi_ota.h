#pragma once

const char OTA_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0">
<title>OTA Update · Sharp Edge</title>
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
.hd-icon span { width: 5px; background: var(--accent); border-radius: 1px; }
.hd-info { flex: 1; }
.hd-title { font-size: 15px; font-weight: 500; color: var(--text); letter-spacing: 0.01em; }
.hd-sub { font-family: var(--font-mono); font-size: 10px; color: var(--text-3); margin-top: 2px; }
.hd-dot { width: 8px; height: 8px; border-radius: 2px; flex-shrink: 0; }
.hd-dot.warn { background: var(--warning); box-shadow: 0 0 6px var(--warning); }

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

.info-table { display: flex; flex-direction: column; gap: 6px; margin-bottom: 1.25rem; }
.info-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 7px 10px;
  background: var(--surface2);
  border: 0.5px solid var(--border);
  border-radius: var(--radius);
}
.info-key { font-family: var(--font-mono); font-size: 10px; color: var(--text-3); letter-spacing: 0.06em; text-transform: uppercase; }
.info-val { font-family: var(--font-mono); font-size: 11px; color: var(--text-2); }
.info-val.ok { color: var(--success); }

.dropzone {
  border: 1px dashed var(--border2);
  border-radius: var(--radius-lg);
  padding: 2rem 1rem;
  text-align: center;
  cursor: pointer;
  transition: border-color 0.15s, background 0.15s;
  background: var(--surface2);
  margin-bottom: 1rem;
  position: relative;
}
.dropzone:hover, .dropzone.drag { border-color: var(--accent); background: rgba(14,165,233,0.05); }
.dropzone input[type=file] { position: absolute; inset: 0; opacity: 0; cursor: pointer; width: 100%; height: 100%; }
.dz-icon { font-size: 28px; margin-bottom: 8px; color: var(--text-3); }
.dz-label { font-size: 13px; color: var(--text-2); font-weight: 500; }
.dz-hint { font-family: var(--font-mono); font-size: 10px; color: var(--text-3); margin-top: 4px; }

.file-info {
  display: none;
  align-items: center;
  gap: 10px;
  padding: 10px 12px;
  background: var(--surface2);
  border: 0.5px solid var(--border);
  border-radius: var(--radius);
  margin-bottom: 1rem;
}
.file-info.show { display: flex; }
.fi-icon { font-size: 18px; color: var(--accent); }
.fi-name { font-family: var(--font-mono); font-size: 11px; color: var(--text); }
.fi-size { font-family: var(--font-mono); font-size: 10px; color: var(--text-3); margin-top: 2px; }
.fi-remove { margin-left: auto; font-size: 16px; color: var(--text-3); cursor: pointer; line-height: 1; padding: 2px 4px; }
.fi-remove:hover { color: var(--danger); }

.btn-flash {
  width: 100%;
  padding: 11px;
  border-radius: var(--radius);
  background: var(--accent);
  border: none;
  color: #fff;
  font-family: var(--font-sans);
  font-size: 13px;
  font-weight: 500;
  cursor: pointer;
  letter-spacing: 0.03em;
  transition: background 0.15s, opacity 0.15s;
}
.btn-flash:hover:not(:disabled) { background: var(--accent-h); }
.btn-flash:disabled { opacity: 0.4; cursor: not-allowed; }

.progress-wrap { display: none; margin-top: 1rem; }
.progress-wrap.show { display: block; }
.progress-bar-bg { height: 4px; background: var(--border); border-radius: 2px; overflow: hidden; margin-bottom: 6px; }
.progress-bar-fill { height: 100%; width: 0%; background: var(--accent); border-radius: 2px; transition: width 0.2s ease; }
.progress-label { display: flex; justify-content: space-between; font-family: var(--font-mono); font-size: 10px; color: var(--text-3); }

.log-box {
  margin-top: 1rem;
  background: var(--surface2);
  border: 0.5px solid var(--border);
  border-radius: var(--radius);
  padding: 10px 12px;
  font-family: var(--font-mono);
  font-size: 10px;
  color: var(--text-3);
  line-height: 1.8;
  max-height: 120px;
  overflow-y: auto;
  display: none;
}
.log-box.show { display: block; }
.log-line { color: var(--text-3); }
.log-line.ok   { color: var(--success); }
.log-line.err  { color: var(--danger); }
.log-line.info { color: var(--accent); }

.result-box { display: none; margin-top: 1rem; padding: 12px; border-radius: var(--radius); text-align: center; font-size: 13px; font-weight: 500; }
.result-box.ok  { display: block; background: rgba(34,197,94,0.08);  border: 0.5px solid rgba(34,197,94,0.3);  color: var(--success); }
.result-box.err { display: block; background: rgba(239,68,68,0.08);  border: 0.5px solid rgba(239,68,68,0.3);  color: var(--danger); }

.ft { padding: 0.6rem 1.5rem 1.1rem; display: flex; align-items: center; justify-content: space-between; }
.ft-chip { font-family: var(--font-mono); font-size: 10px; color: var(--text-4); letter-spacing: 0.04em; }
.back-link { font-family: var(--font-mono); font-size: 10px; color: var(--text-3); text-decoration: none; letter-spacing: 0.04em; }
.back-link:hover { color: var(--accent); }
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
      <div class="hd-title">OTA Update</div>
      <div class="hd-sub">SHARP EDGE &middot; FIRMWARE FLASH</div>
    </div>
    <div class="hd-dot warn"></div>
  </div>

  <div class="body">

    <div class="sec">Current firmware</div>
    <div class="info-table">
      <div class="info-row">
        <span class="info-key">Version</span>
        <span class="info-val ok" id="cur-ver">%FW_VERSION%</span>
      </div>
      <div class="info-row">
        <span class="info-key">Build</span>
        <span class="info-val">%FW_BUILD%</span>
      </div>
      <div class="info-row">
        <span class="info-key">Partition</span>
        <span class="info-val">%FW_PARTITION%</span>
      </div>
      <div class="info-row">
        <span class="info-key">Flash free</span>
        <span class="info-val">%FW_FLASHFREE%</span>
      </div>
    </div>

    <div class="div"></div>

    <div class="sec">Upload firmware (.bin)</div>

    <div class="dropzone" id="dropzone">
      <input type="file" id="file-input" accept=".bin">
      <div class="dz-icon">&#11014;</div>
      <div class="dz-label">Drop .bin file here</div>
      <div class="dz-hint">or click to browse</div>
    </div>

    <div class="file-info" id="file-info">
      <span class="fi-icon">&#128230;</span>
      <div>
        <div class="fi-name" id="fi-name">firmware.bin</div>
        <div class="fi-size" id="fi-size">0 KB</div>
      </div>
      <span class="fi-remove" id="fi-remove" title="Remove">&#10005;</span>
    </div>

    <button class="btn-flash" id="btn-flash" disabled>Flash firmware</button>

    <div class="progress-wrap" id="progress-wrap">
      <div class="progress-bar-bg">
        <div class="progress-bar-fill" id="progress-fill"></div>
      </div>
      <div class="progress-label">
        <span id="progress-status">Uploading...</span>
        <span id="progress-pct">0%</span>
      </div>
    </div>

    <div class="log-box" id="log-box"></div>
    <div class="result-box" id="result-box"></div>

  </div>

  <div class="ft">
    <span class="ft-chip">WIFIMANAGER PORTAL</span>
    <a href="/" class="back-link">&#8592; back</a>
  </div>

</div>

<script>
(function(){
  var fileInput      = document.getElementById('file-input');
  var dropzone       = document.getElementById('dropzone');
  var fileInfo       = document.getElementById('file-info');
  var fiName         = document.getElementById('fi-name');
  var fiSize         = document.getElementById('fi-size');
  var fiRemove       = document.getElementById('fi-remove');
  var btnFlash       = document.getElementById('btn-flash');
  var progressWrap   = document.getElementById('progress-wrap');
  var progressFill   = document.getElementById('progress-fill');
  var progressStatus = document.getElementById('progress-status');
  var progressPct    = document.getElementById('progress-pct');
  var logBox         = document.getElementById('log-box');
  var resultBox      = document.getElementById('result-box');
  var selectedFile   = null;

  function formatSize(bytes) {
    if (bytes < 1024)        return bytes + ' B';
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
    return (bytes / (1024 * 1024)).toFixed(2) + ' MB';
  }

  function log(msg, cls) {
    var line = document.createElement('div');
    line.className = 'log-line' + (cls ? ' ' + cls : '');
    line.textContent = '> ' + msg;
    logBox.appendChild(line);
    logBox.scrollTop = logBox.scrollHeight;
  }

  function setFile(file) {
    if (!file || !file.name.endsWith('.bin')) {
      alert('Please select a valid .bin firmware file.');
      return;
    }
    selectedFile = file;
    fiName.textContent = file.name;
    fiSize.textContent = formatSize(file.size);
    dropzone.style.display = 'none';
    fileInfo.classList.add('show');
    btnFlash.disabled = false;
    resultBox.className = 'result-box';
    resultBox.textContent = '';
    logBox.innerHTML = '';
    logBox.classList.remove('show');
    progressWrap.classList.remove('show');
    progressFill.style.width = '0%';
  }

  fileInput.addEventListener('change', function() {
    if (this.files[0]) setFile(this.files[0]);
  });

  dropzone.addEventListener('dragover',  function(e) { e.preventDefault(); this.classList.add('drag'); });
  dropzone.addEventListener('dragleave', function()  { this.classList.remove('drag'); });
  dropzone.addEventListener('drop', function(e) {
    e.preventDefault();
    this.classList.remove('drag');
    if (e.dataTransfer.files[0]) setFile(e.dataTransfer.files[0]);
  });

  fiRemove.addEventListener('click', function() {
    selectedFile = null;
    fileInput.value = '';
    fileInfo.classList.remove('show');
    dropzone.style.display = '';
    btnFlash.disabled = true;
  });

  btnFlash.addEventListener('click', function() {
    if (!selectedFile) return;
    if (!confirm('Flash "' + selectedFile.name + '"?\nDevice will reboot after flashing.')) return;

    btnFlash.disabled = true;
    fiRemove.style.pointerEvents = 'none';
    progressWrap.classList.add('show');
    logBox.innerHTML = '';
    logBox.classList.add('show');
    resultBox.className = 'result-box';

    log('Initiating OTA upload...', 'info');
    log('File: ' + selectedFile.name + ' (' + formatSize(selectedFile.size) + ')');

    var xhr = new XMLHttpRequest();
    xhr.open('POST', '/ota', true);

    xhr.upload.onprogress = function(e) {
      if (e.lengthComputable) {
        var pct = Math.round(e.loaded / e.total * 100);
        progressFill.style.width = pct + '%';
        progressPct.textContent  = pct + '%';
        progressStatus.textContent = pct < 100 ? 'Uploading...' : 'Writing flash...';
        if (pct === 100) log('Upload done -- writing to flash partition...', 'info');
      }
    };

    xhr.onload = function() {
      progressFill.style.width = '100%';
      if (xhr.status === 200) {
        log('Flash verified OK.', 'ok');
        log('Rebooting device...', 'ok');
        resultBox.className   = 'result-box ok';
        progressStatus.textContent = 'Done';
        var countdown = 10;
        resultBox.textContent = 'Flash successful -- rebooting (' + countdown + 's)';
        var iv = setInterval(function() {
          countdown--;
          resultBox.textContent = 'Flash successful -- rebooting (' + countdown + 's)';
          if (countdown <= 0) { clearInterval(iv); window.location.href = '/'; }
        }, 1000);
      } else {
        log('Server returned ' + xhr.status + ': ' + xhr.responseText, 'err');
        resultBox.className   = 'result-box err';
        resultBox.textContent = 'Flash failed -- ' + (xhr.responseText || 'unknown error');
        btnFlash.disabled = false;
        fiRemove.style.pointerEvents = '';
        progressStatus.textContent = 'Failed';
      }
    };

    xhr.onerror = function() {
      log('Connection lost -- device may have rebooted.', 'err');
      resultBox.className   = 'result-box err';
      resultBox.textContent = 'Connection lost -- try reconnecting to device';
      progressStatus.textContent = 'Error';
      btnFlash.disabled = false;
      fiRemove.style.pointerEvents = '';
    };

    var formData = new FormData();
    formData.append('firmware', selectedFile, selectedFile.name);
    xhr.send(formData);
  });
})();
</script>
</body>
</html>
)rawliteral";