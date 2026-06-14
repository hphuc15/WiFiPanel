#pragma once

// Auto-generated from config.html
// Include in WiFiPanel_Page.c:
//   #include "wifi_config.h"

static const char CONFIG_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0">
<title>Config · Sharp Edge</title>
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

/* Header */
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
  align-items: center;
  justify-content: center;
  width: 32px;
  height: 32px;
}
.hd-icon svg { opacity: 0.85; }
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
.hd-dot.saved { background: var(--success); box-shadow: 0 0 6px var(--success); }
@keyframes pulse { 0%,100%{opacity:1} 50%{opacity:0.4} }

/* Body */
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

/* Field list */
#field-list {
  display: flex;
  flex-direction: column;
  gap: 6px;
  margin-bottom: 0.75rem;
}

.field-row {
  background: var(--surface2);
  border: 0.5px solid var(--border);
  border-radius: var(--radius);
  overflow: hidden;
  transition: border-color 0.15s;
}
.field-row:focus-within { border-color: var(--border2); }

.field-header {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 8px 10px;
  cursor: pointer;
  user-select: none;
}
.field-key-preview {
  flex: 1;
  font-family: var(--font-mono);
  font-size: 11px;
  color: var(--text-2);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.field-key-preview .key  { color: var(--accent); }
.field-key-preview .sep  { color: var(--text-4); margin: 0 4px; }
.field-key-preview .type-badge {
  font-size: 9px;
  letter-spacing: 0.06em;
  padding: 1px 5px;
  border-radius: 2px;
  background: rgba(14,165,233,0.12);
  color: var(--accent);
  text-transform: uppercase;
}
.field-key-preview .type-badge.str  { background: rgba(34,197,94,0.12);  color: var(--success); }
.field-key-preview .type-badge.int  { background: rgba(14,165,233,0.12); color: var(--accent); }
.field-key-preview .type-badge.flt  { background: rgba(249,115,22,0.12); color: var(--warning); }
.field-key-preview .type-badge.bool { background: rgba(168,85,247,0.12); color: #a855f7; }
.field-key-preview .type-badge.pass { background: rgba(239,68,68,0.12);  color: var(--danger); }
.field-key-preview .type-badge.sel  { background: rgba(251,191,36,0.12); color: #fbbf24; }
.field-key-preview .val-hint { color: var(--text-3); font-size: 10px; }

.field-actions { display: flex; align-items: center; gap: 4px; }
.icon-btn {
  background: transparent;
  border: none;
  color: var(--text-3);
  cursor: pointer;
  font-size: 13px;
  padding: 2px 4px;
  border-radius: 2px;
  line-height: 1;
  transition: color 0.12s, background 0.12s;
}
.icon-btn:hover { color: var(--text-2); background: rgba(255,255,255,0.04); }
.icon-btn.del:hover { color: var(--danger); }
.chevron {
  font-size: 11px;
  color: var(--text-4);
  transition: transform 0.15s;
  display: inline-block;
}
.field-row.open .chevron { transform: rotate(90deg); }

.field-body {
  display: none;
  padding: 8px 10px 10px;
  border-top: 0.5px solid var(--border);
  background: rgba(0,0,0,0.12);
}
.field-row.open .field-body { display: block; }

.mini-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 7px;
  margin-bottom: 7px;
}
.mini-grid.full { grid-template-columns: 1fr; }

.mini-field { display: flex; flex-direction: column; gap: 3px; }
.mini-label {
  font-family: var(--font-mono);
  font-size: 9px;
  letter-spacing: 0.08em;
  text-transform: uppercase;
  color: var(--text-3);
}
.mini-input {
  background: var(--surface);
  border: 0.5px solid var(--border);
  border-radius: 3px;
  padding: 6px 8px;
  font-family: var(--font-mono);
  font-size: 11px;
  color: var(--text);
  outline: none;
  width: 100%;
  transition: border-color 0.15s;
}
.mini-input:focus { border-color: var(--accent); }
.mini-input::placeholder { color: var(--text-4); }

.mini-select {
  background: var(--surface);
  border: 0.5px solid var(--border);
  border-radius: 3px;
  padding: 6px 8px;
  font-family: var(--font-mono);
  font-size: 11px;
  color: var(--text-2);
  outline: none;
  width: 100%;
  cursor: pointer;
  appearance: none;
  transition: border-color 0.15s;
}
.mini-select:focus { border-color: var(--accent); }

/* Add field button */
.btn-add {
  width: 100%;
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 7px;
  padding: 9px;
  background: transparent;
  border: 0.5px dashed var(--border2);
  border-radius: var(--radius);
  font-family: var(--font-mono);
  font-size: 11px;
  color: var(--text-3);
  letter-spacing: 0.06em;
  cursor: pointer;
  transition: border-color 0.15s, color 0.15s, background 0.15s;
}
.btn-add:hover {
  border-color: var(--accent);
  color: var(--accent);
  background: rgba(14,165,233,0.04);
}

/* Value input area */
.fields { display: flex; flex-direction: column; gap: 8px; margin-bottom: 1.1rem; }

.dyn-field { display: flex; flex-direction: column; gap: 4px; }
.dyn-label {
  font-size: 11px;
  font-weight: 500;
  color: var(--text-2);
  display: flex;
  align-items: center;
  gap: 6px;
}
.dyn-label .type-tag {
  font-family: var(--font-mono);
  font-size: 9px;
  letter-spacing: 0.06em;
  padding: 1px 5px;
  border-radius: 2px;
  text-transform: uppercase;
}
.dyn-label .hint { font-size: 10px; color: var(--text-4); font-weight: 400; margin-left: auto; font-family: var(--font-mono); }

input.cfg-input, textarea.cfg-input {
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
input.cfg-input:focus, textarea.cfg-input:focus { border-color: var(--accent); }
input.cfg-input::placeholder { color: var(--text-4); }
textarea.cfg-input { resize: vertical; min-height: 64px; font-family: var(--font-mono); font-size: 12px; }

.toggle-row {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 9px 12px;
  background: var(--surface2);
  border: 0.5px solid var(--border);
  border-radius: 4px;
}
.toggle-label { flex: 1; font-size: 13px; color: var(--text-3); }
.toggle-switch {
  position: relative;
  width: 34px;
  height: 18px;
  cursor: pointer;
  flex-shrink: 0;
}
.toggle-switch input { opacity: 0; width: 0; height: 0; }
.toggle-track {
  position: absolute;
  inset: 0;
  background: var(--border2);
  border-radius: 9px;
  transition: background 0.2s;
}
.toggle-switch input:checked + .toggle-track { background: var(--accent); }
.toggle-thumb {
  position: absolute;
  top: 2px; left: 2px;
  width: 14px; height: 14px;
  background: white;
  border-radius: 50%;
  transition: transform 0.2s;
}
.toggle-switch input:checked ~ .toggle-thumb { transform: translateX(16px); }

/* Submit */
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
.btn:hover  { background: var(--accent-h); }
.btn:active { opacity: 0.85; }

.btn-row { display: flex; gap: 8px; }
.btn.secondary {
  background: transparent;
  border: 0.5px solid var(--border);
  color: var(--text-3);
  flex: 0 0 auto;
  width: auto;
  padding: 11px 16px;
}
.btn.secondary:hover { border-color: var(--accent); color: var(--accent); background: rgba(14,165,233,0.05); }

/* Toast */
.toast {
  position: fixed;
  bottom: 1.5rem;
  left: 50%;
  transform: translateX(-50%) translateY(8px);
  background: var(--surface);
  border: 0.5px solid var(--border2);
  border-radius: var(--radius);
  padding: 8px 14px;
  font-family: var(--font-mono);
  font-size: 11px;
  color: var(--text-2);
  opacity: 0;
  pointer-events: none;
  transition: opacity 0.2s, transform 0.2s;
  z-index: 100;
  white-space: nowrap;
  box-shadow: 0 4px 20px rgba(0,0,0,0.4);
}
.toast.show { opacity: 1; transform: translateX(-50%) translateY(0); }
.toast.ok  { border-color: rgba(34,197,94,0.4); color: var(--success); }
.toast.err { border-color: rgba(239,68,68,0.4); color: var(--danger); }

/* Empty state */
.empty-state {
  text-align: center;
  padding: 1.5rem 1rem;
  font-family: var(--font-mono);
  font-size: 11px;
  color: var(--text-4);
  line-height: 1.8;
  display: none;
}

/* Tab strip */
.tab-strip {
  display: flex;
  gap: 0;
  border: 0.5px solid var(--border);
  border-radius: var(--radius);
  overflow: hidden;
  margin-bottom: 1.25rem;
}
.tab {
  flex: 1;
  padding: 7px 10px;
  background: var(--surface2);
  border: none;
  font-family: var(--font-mono);
  font-size: 10px;
  letter-spacing: 0.08em;
  text-transform: uppercase;
  color: var(--text-3);
  cursor: pointer;
  transition: background 0.15s, color 0.15s;
}
.tab + .tab { border-left: 0.5px solid var(--border); }
.tab.active { background: rgba(14,165,233,0.1); color: var(--accent); }

/* Success screen - giống /scan */
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
.status-icon.ok  { background: rgba(34,197,94,0.12);  color: var(--success); }
.status-icon.err { background: rgba(239,68,68,0.12);  color: var(--danger); }
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

/* Footer */
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
.ft-count {
  font-family: var(--font-mono);
  font-size: 10px;
  color: var(--text-3);
}
</style>
</head>
<body>

<div class="card">

  <!-- Header -->
  <div class="hd">
    <div class="hd-icon">
      <svg width="22" height="22" viewBox="0 0 22 22" fill="none">
        <rect x="3" y="3" width="6" height="6" rx="1" stroke="#0ea5e9" stroke-width="1.2"/>
        <rect x="13" y="3" width="6" height="6" rx="1" stroke="#0ea5e9" stroke-width="1.2" opacity=".5"/>
        <rect x="3" y="13" width="6" height="6" rx="1" stroke="#0ea5e9" stroke-width="1.2" opacity=".5"/>
        <rect x="13" y="13" width="6" height="6" rx="1" stroke="#0ea5e9" stroke-width="1.2" opacity=".35"/>
      </svg>
    </div>
    <div class="hd-info">
      <div class="hd-title">Config</div>
      <div class="hd-sub" id="hd-sub">WIFIPANEL · PARAM</div>
    </div>
    <div class="hd-dot" id="status-dot"></div>
  </div>

  <!-- Main form body -->
  <div id="screen-form">
    <div class="body">

      <!-- Tab switch: Define / Fill -->
      <div class="tab-strip">
        <button class="tab active" id="tab-define" onclick="switchTab('define')">Define Fields</button>
        <button class="tab"        id="tab-fill"   onclick="switchTab('fill')">Fill Values</button>
      </div>

      <!-- TAB: DEFINE -->
      <div id="pane-define">
        <div class="sec">Parameter schema</div>

        <div id="field-list"></div>
        <div class="empty-state" id="empty-state">
          No fields defined yet.<br>Press + to add a parameter.
        </div>

        <button class="btn-add" onclick="addField()">
          <span style="font-size:15px;line-height:1">+</span>
          ADD PARAMETER
        </button>
      </div>

      <!-- TAB: FILL -->
      <div id="pane-fill" style="display:none">
        <div class="sec">Parameter values</div>
        <div class="fields" id="value-form"></div>
        <div class="empty-state" id="fill-empty" style="display:block">
          No fields defined.<br>Switch to <em>Define Fields</em> to add parameters.
        </div>

        <div class="btn-row">
          <button class="btn secondary" onclick="doReset()">Reset</button>
          <button class="btn" onclick="doSave()">Save &amp; Apply</button>
        </div>
      </div>

    </div>

    <!-- Footer -->
    <div class="ft">
      <span class="ft-chip">WIFIPANEL PORTAL</span>
      <span class="ft-count" id="ft-count">0 params</span>
    </div>
  </div>

  <!-- Success / Error screen (giống /scan) -->
  <div id="screen-status">
    <div class="status-icon ok" id="st-icon">&#10003;</div>
    <div class="status-title"   id="st-title">Saved!</div>
    <div class="status-msg"     id="st-msg">Configuration applied</div>
    <button class="btn-ghost" onclick="showForm()">&#8592; Back</button>
  </div>

</div>

<div class="toast" id="toast"></div>

<script>
var TYPE_META = {
  str:  { label:'String',   badge:'STR',  badgeClass:'str',  placeholder:'text value' },
  int:  { label:'Integer',  badge:'INT',  badgeClass:'int',  placeholder:'0' },
  flt:  { label:'Float',    badge:'FLT',  badgeClass:'flt',  placeholder:'0.0' },
  bool: { label:'Boolean',  badge:'BOOL', badgeClass:'bool', placeholder:'' },
  pass: { label:'Password', badge:'PASS', badgeClass:'pass', placeholder:'••••••••' },
  sel:  { label:'Select',   badge:'SEL',  badgeClass:'sel',  placeholder:'opt1,opt2' },
};

var schema = [];
var values = {};
var uid = 0;

(function(){
  if(window.wifiparam_schema && Array.isArray(window.wifiparam_schema)){
    schema = window.wifiparam_schema.map(function(f){
      return Object.assign({ _id: uid++ }, f);
    });
  }
  if(window.wifiparam_values && typeof window.wifiparam_values === 'object'){
    values = Object.assign({}, window.wifiparam_values);
  } else {
    schema.forEach(function(f){ if(f.default !== undefined) values[f.key] = f.default; });
  }
  renderDefine();
})();

function showForm(){
  document.getElementById('screen-form').style.display   = '';
  document.getElementById('screen-status').style.display = 'none';
  document.getElementById('status-dot').className = 'hd-dot';
}

function showStatus(ok, title, msg){
  document.getElementById('screen-form').style.display   = 'none';
  document.getElementById('screen-status').style.display = 'flex';
  document.getElementById('st-icon').className  = 'status-icon ' + (ok ? 'ok' : 'err');
  document.getElementById('st-icon').innerHTML  = ok ? '&#10003;' : '&#10005;';
  document.getElementById('st-title').textContent = title;
  document.getElementById('st-msg').textContent   = msg;
  document.getElementById('status-dot').className = ok ? 'hd-dot saved' : 'hd-dot';
}

function switchTab(t){
  document.getElementById('tab-define').classList.toggle('active', t==='define');
  document.getElementById('tab-fill').classList.toggle('active',   t==='fill');
  document.getElementById('pane-define').style.display = t==='define' ? '' : 'none';
  document.getElementById('pane-fill').style.display   = t==='fill'   ? '' : 'none';
  if(t==='fill') renderFill();
}

function addField(tpl){
  var f = tpl || { _id: uid++, key:'', type:'str', label:'', default:'', desc:'' };
  schema.push(f);
  renderDefine();
  var rows = document.querySelectorAll('.field-row');
  var last = rows[rows.length-1];
  if(last){ last.classList.add('open'); last.querySelector('.mini-input').focus(); }
  updateCount();
}

function removeField(id){
  schema = schema.filter(function(f){ return f._id !== id; });
  renderDefine();
  updateCount();
}

function renderDefine(){
  var list  = document.getElementById('field-list');
  var empty = document.getElementById('empty-state');
  if(schema.length === 0){
    list.innerHTML = '';
    empty.style.display = 'block';
    updateCount();
    return;
  }
  empty.style.display = 'none';
  list.innerHTML = schema.map(function(f){ return buildFieldRow(f); }).join('');

  schema.forEach(function(f){
    var row = document.getElementById('fr-' + f._id);
    if(!row) return;

    row.querySelector('.field-header').addEventListener('click', function(e){
      if(e.target.closest('.icon-btn')) return;
      row.classList.toggle('open');
    });

    ['key','type','default'].forEach(function(attr){
      var inp = row.querySelector('[data-attr='+attr+']');
      if(inp) inp.addEventListener('input', function(){
        f[attr] = inp.value;
        updatePreview(row, f);
      });
    });
    ['label','desc'].forEach(function(attr){
      var inp = row.querySelector('[data-attr='+attr+']');
      if(inp) inp.addEventListener('input', function(){ f[attr] = inp.value; });
    });

    row.querySelector('.del').addEventListener('click', function(e){
      e.stopPropagation();
      removeField(f._id);
    });
  });
  updateCount();
}

function buildFieldRow(f){
  var tm    = TYPE_META[f.type] || TYPE_META.str;
  var badge = '<span class="type-badge '+tm.badgeClass+'">'+tm.badge+'</span>';
  var valHint = f.default ? ' <span class="val-hint">= '+esc(f.default)+'</span>' : '';

  return '<div class="field-row" id="fr-'+f._id+'">'
    + '<div class="field-header">'
    +   '<span class="chevron">&#8250;</span>'
    +   '<span class="key-preview field-key-preview">'
    +     '<span class="key">'+esc(f.key||'...')+'</span>'
    +     '<span class="sep">&#183;</span>'
    +     badge + valHint
    +   '</span>'
    +   '<div class="field-actions">'
    +     '<button class="icon-btn del" title="Remove">&#10005;</button>'
    +   '</div>'
    + '</div>'
    + '<div class="field-body">'
    +   '<div class="mini-grid">'
    +     '<div class="mini-field"><span class="mini-label">Key</span>'
    +       '<input class="mini-input" data-attr="key" value="'+esc(f.key)+'" placeholder="param_key" spellcheck="false" autocomplete="off"/>'
    +     '</div>'
    +     '<div class="mini-field"><span class="mini-label">Type</span>'
    +       '<select class="mini-select" data-attr="type">'
    +         Object.keys(TYPE_META).map(function(t){
                return '<option value="'+t+'"'+(f.type===t?' selected':'')+'>'+TYPE_META[t].label+'</option>';
              }).join('')
    +       '</select>'
    +     '</div>'
    +     '<div class="mini-field"><span class="mini-label">Label</span>'
    +       '<input class="mini-input" data-attr="label" value="'+esc(f.label)+'" placeholder="Human label" autocomplete="off"/>'
    +     '</div>'
    +     '<div class="mini-field"><span class="mini-label">Default</span>'
    +       '<input class="mini-input" data-attr="default" value="'+esc(f.default)+'" placeholder="'+esc(tm.placeholder)+'" autocomplete="off"/>'
    +     '</div>'
    +   '</div>'
    +   '<div class="mini-grid full">'
    +     '<div class="mini-field"><span class="mini-label">Description</span>'
    +       '<input class="mini-input" data-attr="desc" value="'+esc(f.desc||'')+'" placeholder="Short description (optional)" autocomplete="off"/>'
    +     '</div>'
    +   '</div>'
    + '</div>'
  + '</div>';
}

function updatePreview(row, f){
  var tm = TYPE_META[f.type] || TYPE_META.str;
  var preview = row.querySelector('.key-preview');
  var valHint = f.default ? ' <span class="val-hint">= '+esc(f.default)+'</span>' : '';
  preview.innerHTML = '<span class="key">'+esc(f.key||'...')+'</span>'
    + '<span class="sep">&#183;</span>'
    + '<span class="type-badge '+tm.badgeClass+'">'+tm.badge+'</span>'
    + valHint;
}

function renderFill(){
  var form  = document.getElementById('value-form');
  var empty = document.getElementById('fill-empty');
  if(schema.length === 0){
    form.innerHTML = '';
    empty.style.display = 'block';
    return;
  }
  empty.style.display = 'none';

  form.innerHTML = schema.map(function(f){
    var tm     = TYPE_META[f.type] || TYPE_META.str;
    var curVal = (values[f.key] !== undefined) ? values[f.key] : (f.default || '');
    var labelStr = f.label || f.key;
    var typeTag  = '<span class="type-tag '+tm.badgeClass+'" style="background:'+typeBg(f.type)+';color:'+typeColor(f.type)+'">'+tm.badge+'</span>';
    var hintStr  = f.desc ? '<span class="hint">'+esc(f.desc)+'</span>' : '';

    var input = '';
    if(f.type === 'bool'){
      var checked = (curVal === '1' || curVal === 'true') ? 'checked' : '';
      input = '<div class="toggle-row">'
        + '<span class="toggle-label">'+(f.desc||'Enable / Disable')+'</span>'
        + '<label class="toggle-switch">'
        +   '<input type="checkbox" id="v-'+esc(f.key)+'" '+checked+'>'
        +   '<div class="toggle-track"></div>'
        +   '<div class="toggle-thumb"></div>'
        + '</label>'
        + '</div>';
    } else if(f.type === 'pass'){
      input = '<input class="cfg-input" type="password" id="v-'+esc(f.key)+'" value="'+esc(curVal)+'" placeholder="'+esc(tm.placeholder)+'" autocomplete="new-password">';
    } else if(f.type === 'sel'){
      var opts = (f.desc || '').split(',');
      input = '<select class="cfg-input" id="v-'+esc(f.key)+'">'
        + opts.map(function(o){
            o = o.trim();
            return '<option value="'+esc(o)+'"'+(curVal===o?' selected':'')+'>'+esc(o)+'</option>';
          }).join('')
        + '</select>';
    } else {
      input = '<input class="cfg-input" type="text" id="v-'+esc(f.key)+'" value="'+esc(curVal)+'" placeholder="'+esc(f.default||tm.placeholder)+'" autocomplete="off" inputmode="'+(f.type==='int'||f.type==='flt'?'decimal':'text')+'">';
    }

    return '<div class="dyn-field">'
      + '<div class="dyn-label">'+esc(labelStr)+' '+typeTag+hintStr+'</div>'
      + input
      + '</div>';
  }).join('');
}

function typeBg(t){
  return {str:'rgba(34,197,94,0.12)',int:'rgba(14,165,233,0.12)',flt:'rgba(249,115,22,0.12)',bool:'rgba(168,85,247,0.12)',pass:'rgba(239,68,68,0.12)',sel:'rgba(251,191,36,0.12)'}[t]||'rgba(14,165,233,0.12)';
}
function typeColor(t){
  return {str:'#22c55e',int:'#0ea5e9',flt:'#f97316',bool:'#a855f7',pass:'#ef4444',sel:'#fbbf24'}[t]||'#0ea5e9';
}

function doSave(){
  schema.forEach(function(f){
    var el = document.getElementById('v-'+f.key);
    if(!el) return;
    if(f.type === 'bool') values[f.key] = el.checked ? '1' : '0';
    else values[f.key] = el.value;
  });

  var errors = [];
  schema.forEach(function(f){
    var v = values[f.key] || '';
    if(f.type === 'int' && v !== '' && !/^-?\d+$/.test(v.trim()))
      errors.push('"'+(f.label||f.key)+'" must be an integer');
    if(f.type === 'flt' && v !== '' && isNaN(parseFloat(v.trim())))
      errors.push('"'+(f.label||f.key)+'" must be a number');
  });
  if(errors.length){ showToast('&#10005; ' + errors[0], 'err'); return; }

  var parts = [];
  schema.forEach(function(f){
    parts.push(encodeURIComponent(f.key) + '=' + encodeURIComponent(values[f.key] !== undefined ? values[f.key] : ''));
  });

  showToast('Saving...', '');

  fetch('/configsave', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: parts.join('&')
  })
  .then(function(r){
    if(!r.ok) throw new Error(r.status);
    showStatus(true, 'Saved!', schema.length + ' param' + (schema.length !== 1 ? 's' : '') + ' applied');
  })
  .catch(function(){
    showStatus(false, 'Save failed', 'Could not reach device');
  });
}

function doReset(){
  schema.forEach(function(f){ values[f.key] = f.default || ''; });
  renderFill();
  showToast('Reset to defaults', '');
}

function esc(s){
  if(s === undefined || s === null) return '';
  return String(s)
    .replace(/&/g,'&amp;')
    .replace(/</g,'&lt;')
    .replace(/>/g,'&gt;')
    .replace(/"/g,'&quot;')
    .replace(/'/g,'&#39;');
}

function updateCount(){
  var n = schema.length;
  document.getElementById('ft-count').textContent = n + ' param' + (n !== 1 ? 's' : '');
}

function showToast(msg, cls){
  var t = document.getElementById('toast');
  t.textContent = msg;
  t.className   = 'toast' + (cls ? ' '+cls : '');
  t.classList.add('show');
  clearTimeout(t._tid);
  t._tid = setTimeout(function(){ t.classList.remove('show'); }, 2200);
}
</script>
</body>
</html>
)rawliteral";