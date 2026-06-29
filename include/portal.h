#pragma once

const char PORTAL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>ESP32 Fan Controller</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Oxygen,Ubuntu,sans-serif;background:#0f1117;color:#e1e4e8;padding:20px;min-height:100vh;display:flex;justify-content:center}
.container{max-width:480px;width:100%;padding-top:20px}
h1{font-size:1.3rem;font-weight:600;margin-bottom:20px;color:#58a6ff;text-align:center}
.scanning{text-align:center;padding:24px;color:#8b949e;font-size:.9rem}
.spinner{display:inline-block;width:20px;height:20px;border:2px solid #2d303a;border-top-color:#58a6ff;border-radius:50%;animation:spin .8s linear infinite;margin-right:8px;vertical-align:middle}
@keyframes spin{to{transform:rotate(360deg)}}
.net-list{margin-bottom:16px}
.net-item{background:#1c1e26;border:1px solid #2d303a;border-radius:10px;padding:14px 16px;margin-bottom:8px;cursor:pointer;display:flex;align-items:center;justify-content:space-between;transition:border-color .2s}
.net-item:hover{border-color:#444}
.net-item.selected{border-color:#58a6ff;background:#1c2333}
.net-item .ssid{font-size:.95rem;font-weight:500;color:#e1e4e8}
.net-item .sec-icon{color:#8b949e;font-size:.8rem;margin-left:8px}
.signal-bars{display:inline-flex;align-items:flex-end;gap:1px;height:14px;margin-left:auto}
.signal-bars .bar{width:3px;border-radius:1px;background:#2d303a}
.signal-bars .bar.active{background:#58a6ff}
.bar-1{height:4px}.bar-2{height:7px}.bar-3{height:10px}.bar-4{height:14px}
.connect-form{background:#1c1e26;border:1px solid #2d303a;border-radius:12px;padding:20px;margin-top:16px}
.form-group{margin-bottom:16px}
.form-group label{display:block;font-size:.75rem;text-transform:uppercase;letter-spacing:.05em;color:#8b949e;margin-bottom:6px}
.form-group .selected-ssid{font-size:1rem;font-weight:600;color:#e1e4e8;word-break:break-all}
.form-group input[type=password]{width:100%;padding:10px 14px;border-radius:8px;border:1px solid #2d303a;background:#0f1117;color:#e1e4e8;font-size:.95rem;outline:none;transition:border-color .2s}
.form-group input[type=password]:focus{border-color:#58a6ff}
.btn{width:100%;padding:12px;border:none;border-radius:8px;font-size:1rem;font-weight:600;cursor:pointer;transition:all .2s;background:#238636;color:#fff}
.btn:hover{background:#2ea043}
.btn:disabled{opacity:.5;cursor:default}
.btn-secondary{background:#2d303a;color:#e1e4e8}
.btn-secondary:hover{background:#3d404a}
.status-msg{text-align:center;padding:12px;border-radius:8px;margin-bottom:16px;font-size:.9rem;display:none}
.status-msg.error{display:block;background#da363320;color:#f85149;border:1px solid #da363340}
.status-msg.success{display:block;background:#23863620;color:#3fb950;border:1px solid #23863640}
.status-msg.info{display:block;background:#1f6feb20;color:#58a6ff;border:1px solid #1f6feb40}
.connected-screen{text-align:center;padding:40px 20px}
.connected-screen .checkmark{font-size:3rem;color:#3fb950;margin-bottom:16px}
.connected-screen .ip{font-size:1.2rem;background:#0f1117;padding:8px 16px;border-radius:8px;display:inline-block;font-family:monospace;color:#58a6ff;margin:12px 0}
.connected-screen .hint{color:#8b949e;font-size:.85rem;margin-top:20px}
.connected-screen .dash-link{display:inline-block;margin-top:16px;padding:10px 24px;background:#1f6feb;color:#fff;border-radius:8px;text-decoration:none;font-weight:600}
.connected-screen .dash-link:hover{background:#58a6ff}
</style>
</head>
<body>
<div class="container">
<h1>Connect to a network</h1>
<div id="statusMsg" class="status-msg"></div>
<div id="scanning" class="scanning"><span class="spinner"></span>Scanning for networks...</div>
<div id="networks" class="net-list"></div>
<div id="connectForm" class="connect-form" style="display:none">
<div class="form-group">
<label>Selected Network</label>
<div class="selected-ssid" id="selectedSsid"></div>
</div>
<div class="form-group" id="passGroup">
<label>Password</label>
<input type="password" id="wifiPass" placeholder="Enter WiFi password" autocomplete="off">
</div>
<button id="connectBtn" class="btn" onclick="doConnect()">Connect</button>
</div>
</div>
<script>
var selectedSsid = '';
var selectedSecured = false;
var connecting = false;

function showStatus(msg, type) {
  var el = document.getElementById('statusMsg');
  el.textContent = msg;
  el.className = 'status-msg ' + type;
}

function rssiBars(rssi) {
  var count = 0;
  if (rssi >= -50) count = 4;
  else if (rssi >= -60) count = 3;
  else if (rssi >= -70) count = 2;
  else if (rssi >= -80) count = 1;
  var html = '<span class="signal-bars">';
  for (var i = 1; i <= 4; i++) {
    html += '<span class="bar bar-' + i + (i <= count ? ' active' : '') + '"></span>';
  }
  html += '</span>';
  return html;
}

function scanNetworks() {
  document.getElementById('scanning').style.display = 'block';
  document.getElementById('networks').innerHTML = '';
  fetch('/api/portal/scan', {method:'POST'}).then(function(r){return r.json()}).then(function(d){
    document.getElementById('scanning').style.display = 'none';
    if (d.networks && d.networks.length > 0) {
      var html = '';
      d.networks.sort(function(a,b){return b.rssi - a.rssi});
      for (var i = 0; i < d.networks.length; i++) {
        var n = d.networks[i];
        var lock = n.secured ? '<span class="sec-icon">&#x1F512;</span>' : '';
        html += '<div class="net-item" onclick="selectNetwork(\'' + i + '\')" data-idx="' + i + '">';
        html += '<div><div class="ssid">' + escapeHtml(n.ssid) + lock + '</div></div>';
        html += rssiBars(n.rssi);
        html += '</div>';
      }
      document.getElementById('networks').innerHTML = html;
    } else {
      document.getElementById('networks').innerHTML = '<div style="text-align:center;padding:24px;color:#8b949e">No networks found</div>';
    }
  }).catch(function(){
    document.getElementById('scanning').style.display = 'none';
    showStatus('Scan failed. Retrying...', 'error');
    setTimeout(scanNetworks, 3000);
  });
}

function escapeHtml(s) {
  if (!s) return '';
  return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}

function selectNetwork(idx) {
  var items = document.querySelectorAll('.net-item');
  for (var i = 0; i < items.length; i++) items[i].classList.remove('selected');
  items[idx].classList.add('selected');
  var nets = window._networks;
  if (!nets) return;
  var net = nets[idx];
  selectedSsid = net.ssid;
  selectedSecured = net.secured;
  document.getElementById('selectedSsid').textContent = selectedSsid;
  document.getElementById('connectForm').style.display = 'block';
  var pg = document.getElementById('passGroup');
  pg.style.display = selectedSecured ? 'block' : 'none';
  if (!selectedSecured) document.getElementById('wifiPass').value = '';
  document.getElementById('wifiPass').focus();
}

function doConnect() {
  if (connecting) return;
  if (!selectedSsid) { showStatus('Select a network first', 'error'); return; }
  var pass = selectedSecured ? document.getElementById('wifiPass').value : '';
  if (selectedSecured && pass.length === 0) { showStatus('Enter a password', 'error'); return; }
  connecting = true;
  var btn = document.getElementById('connectBtn');
  btn.disabled = true;
  btn.textContent = 'Connecting...';
  showStatus('Connecting to ' + selectedSsid + '...', 'info');
  fetch('/api/portal/connect', {
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify({ssid:selectedSsid, pass:pass})
  }).then(function(r){return r.json()}).then(function(d){
    pollConnection();
  }).catch(function(){
    showStatus('Connection request failed', 'error');
    btn.disabled = false;
    btn.textContent = 'Connect';
    connecting = false;
  });
}

function pollConnection() {
  var attempts = 0;
  var maxAttempts = 15;
  var interval = setInterval(function(){
    attempts++;
    fetch('/api/portal/status').then(function(r){return r.json()}).then(function(d){
      if (d.connected) {
        clearInterval(interval);
        showConnected(d.ssid, d.ip);
      } else if (attempts >= maxAttempts) {
        clearInterval(interval);
        showStatus('Connection failed. Check the password and try again.', 'error');
        var btn = document.getElementById('connectBtn');
        btn.disabled = false;
        btn.textContent = 'Connect';
        connecting = false;
      }
    });
  }, 1000);
}

function showConnected(ssid, ip) {
  document.getElementById('connectForm').style.display = 'none';
  document.getElementById('networks').innerHTML = '';
  document.getElementById('statusMsg').style.display = 'none';
  document.querySelector('h1').textContent = 'Connected';
  var container = document.querySelector('.container');
  var div = document.createElement('div');
  div.className = 'connected-screen';
  div.innerHTML = '<div class="checkmark">&#10004;</div>' +
    '<p style="font-size:1.1rem;margin-bottom:8px">Connected to <strong>' + escapeHtml(ssid) + '</strong></p>' +
    '<div class="ip">' + ip + '</div>' +
    '<p style="margin-top:16px;color:#8b949e">You can now switch your device to the same network.</p>' +
    '<a href="/" class="dash-link">Open Dashboard</a>' +
    '<p class="hint">After switching networks, visit <strong>' + ip + '</strong> in your browser.</p>';
  container.appendChild(div);
  showStatus('Redirecting to Google for captive portal check...', 'info');
  setTimeout(function(){ window.location.href = 'http://google.com'; }, 3000);
}

fetch('/api/portal/status').then(function(r){return r.json()}).then(function(d){
  if (d.connected) {
    document.getElementById('scanning').style.display = 'none';
    showConnected(d.ssid, d.ip);
    return;
  }
  scanNetworks();
}).catch(function(){
  scanNetworks();
});

fetch('/api/portal/scan', {method:'POST'}).then(function(r){return r.json()}).then(function(d){
  if (!d.networks) return;
  window._networks = d.networks;
});
</script>
</body>
</html>
)rawliteral";

const char STATUS_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>ESP32 Status</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Oxygen,Ubuntu,sans-serif;background:#0f1117;color:#e1e4e8;padding:20px;min-height:100vh}
.container{max-width:700px;margin:0 auto}
h1{font-size:1.4rem;font-weight:600;margin-bottom:20px;color:#58a6ff}
h2{font-size:1.1rem;font-weight:600;margin-bottom:12px;color:#e1e4e8}
.card{background:#1c1e26;border-radius:12px;padding:20px;border:1px solid #2d303a;margin-bottom:16px}
.card .label{font-size:.75rem;text-transform:uppercase;letter-spacing:.05em;color:#8b949e;margin-bottom:8px}
table{width:100%;border-collapse:collapse}
td{padding:6px 0;font-size:.9rem;border-bottom:1px solid #2d303a}
td:first-child{color:#8b949e;width:140px}
td:last-child{font-weight:500;word-break:break-all}
.info-grid{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:8px}
.info-item{background:#0f1117;border-radius:8px;padding:12px}
.info-item .lbl{font-size:.7rem;text-transform:uppercase;color:#8b949e;letter-spacing:.05em}
.info-item .val{font-size:1.1rem;font-weight:600;margin-top:4px}
.status-dot{display:inline-block;width:10px;height:10px;border-radius:50%;margin-right:8px;vertical-align:middle}
.status-dot.on{background:#3fb950;box-shadow:0 0 8px #3fb95066}
.status-dot.off{background:#da3633}
.status-dot.warn{background:#f0883e}
.row{display:flex;gap:16px;flex-wrap:wrap;margin-bottom:16px}
.api-ref{font-family:monospace;font-size:.85rem;background:#0f1117;border-radius:8px;padding:12px;margin-bottom:8px;overflow-x:auto}
.api-ref .method{display:inline-block;padding:2px 6px;border-radius:4px;font-size:.75rem;font-weight:600;margin-right:8px}
.method.get{background:#1f6feb20;color:#58a6ff}
.method.post{background:#23863620;color:#3fb950}
.method.sub{background:#f0883e20;color:#f0883e}
.api-ref .path{color:#e1e4e8}
.api-ref .desc{display:block;color:#8b949e;font-size:.8rem;margin-top:4px;padding-left:52px}
.form-group{margin-bottom:14px}
.form-group label{display:block;font-size:.75rem;text-transform:uppercase;letter-spacing:.05em;color:#8b949e;margin-bottom:6px}
.form-group input[type=text],.form-group input[type=password]{width:100%;padding:10px 14px;border-radius:8px;border:1px solid #2d303a;background:#0f1117;color:#e1e4e8;font-size:.95rem;outline:none;transition:border-color .2s}
.form-group input:focus{border-color:#58a6ff}
.btn{padding:10px 20px;border:none;border-radius:8px;font-size:.9rem;font-weight:600;cursor:pointer;transition:all .2s;margin-right:8px;margin-bottom:8px}
.btn-primary{background:#238636;color:#fff}
.btn-primary:hover{background:#2ea043}
.btn-danger{background:#da3633;color:#fff}
.btn-danger:hover{background:#f85149}
.btn-secondary{background:#2d303a;color:#e1e4e8}
.btn-secondary:hover{background:#3d404a}
.btn-restart{background:#da3633;color:#fff;padding:12px 24px;font-size:1rem}
.btn-restart:hover{background:#f85149}
.uptime{font-size:1.2rem;font-weight:700;color:#58a6ff;font-variant-numeric:tabular-nums}
.msg{text-align:center;padding:12px;border-radius:8px;margin-bottom:16px;font-size:.9rem;display:none}
.msg.error{display:block;background:#da363320;color:#f85149;border:1px solid #da363340;display:block}
.msg.success{display:block;background:#23863620;color:#3fb950;border:1px solid #23863640;display:block}
@media(max-width:500px){.info-grid{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="container">
<h1>&#9881; System Status</h1>

<div class="row">
<div class="card" style="flex:1;min-width:160px">
<div class="label">Uptime</div>
<div class="uptime" id="uptime">--</div>
<button class="btn btn-restart" id="restartBtn" onclick="doRestart()" style="margin-top:12px;width:100%">&#x21bb; Restart ESP32</button>
</div>
</div>

<div class="info-grid" id="statusCards">
<div class="info-item"><div class="lbl">WiFi</div><div class="val" id="wifiStatus"><span class="status-dot off"></span>Disconnected</div></div>
<div class="info-item"><div class="lbl">MQTT</div><div class="val" id="mqttStatus"><span class="status-dot off"></span>Disconnected</div></div>
<div class="info-item"><div class="lbl">DHT22</div><div class="val" id="dhtStatus"><span class="status-dot off"></span>--</div></div>
<div class="info-item"><div class="lbl">Fan Mode</div><div class="val" id="fanStatus">--</div></div>
</div>

<div class="card">
<div class="label">WiFi Details</div>
<table>
<tr><td>STA SSID</td><td id="staSsid">--</td></tr>
<tr><td>STA IP</td><td id="staIp">--</td></tr>
<tr><td>Signal</td><td id="staSignal">--</td></tr>
<tr><td>Saved SSID</td><td id="savedSsid">--</td></tr>
<tr><td>AP SSID</td><td>ESP32-Fan-A1NP</td></tr>
<tr><td>AP IP</td><td>192.168.4.1</td></tr>
<tr><td>Free Heap</td><td id="freeHeap">--</td></tr>
</table>
</div>

<div class="card">
<div class="label">Sensor &amp; Fan</div>
<table>
<tr><td>Temperature</td><td id="sensorTemp">--</td></tr>
<tr><td>Humidity</td><td id="sensorHumid">--</td></tr>
<tr><td>Fan Speed</td><td id="fanSpeed">--</td></tr>
<tr><td>RPM</td><td id="fanRpm">--</td></tr>
<tr><td>Setpoint</td><td id="fanSetpoint">--</td></tr>
<tr><td>PID Output</td><td id="pidOutput">--</td></tr>
</table>
</div>

<div class="card">
<div class="label">WiFi Configuration</div>
<div id="wifiConfigMsg" class="msg"></div>
<div class="form-group">
<label>SSID</label>
<input type="text" id="newSsid" placeholder="Enter WiFi SSID">
</div>
<div class="form-group">
<label>Password</label>
<input type="password" id="newPass" placeholder="Enter WiFi password" autocomplete="off">
</div>
<div>
<button class="btn btn-primary" onclick="saveWifi()">Save &amp; Reconnect</button>
<button class="btn btn-danger" onclick="forgetWifi()">Forget WiFi</button>
</div>
</div>

<div class="card">
<div class="label">MQTT Configuration</div>
<table>
<tr><td>Server</td><td>__MQTT_SERVER__:__MQTT_PORT__</td></tr>
<tr><td>User</td><td>__MQTT_USER__</td></tr>
<tr><td>Password</td><td>__MQTT_PASS__</td></tr>
</table>
<div style="margin-top:12px">
<div class="label">Topics</div>
<div class="api-ref" style="margin-bottom:4px"><span class="method post">PUB</span><span class="path">fan/telemetry</span><span class="desc">Sensor data published every 2 seconds</span></div>
<div class="api-ref" style="margin-bottom:4px"><span class="method post">PUB</span><span class="path">fan/status</span><span class="desc">Online/offline status (retained)</span></div>
<div class="api-ref"><span class="method sub">SUB</span><span class="path">fan/cmd</span><span class="desc">Incoming command messages</span></div>
</div>
</div>

<div class="card">
<div class="label">API Reference</div>
<div class="api-ref"><span class="method get">GET</span><span class="path">/</span><span class="desc">Dashboard HTML</span></div>
<div class="api-ref"><span class="method get">GET</span><span class="path">/api/status</span><span class="desc">JSON status (temp, humidity, fan, etc.)</span></div>
<div class="api-ref"><span class="method post">POST</span><span class="path">/api/cmd</span><span class="desc">Send commands (mode_auto, mode_manual, pwm, setpoint, automode)</span></div>
<div class="api-ref"><span class="method get">GET</span><span class="path">/portal</span><span class="desc">WiFi captive portal page</span></div>
<div class="api-ref"><span class="method get">GET</span><span class="path">/status?pass=...</span><span class="desc">This status page (requires admin password)</span></div>
<div class="api-ref"><span class="method post">POST</span><span class="path">/api/portal/scan</span><span class="desc">Scan WiFi networks</span></div>
<div class="api-ref"><span class="method post">POST</span><span class="path">/api/portal/connect</span><span class="desc">Connect to a WiFi network {ssid, pass}</span></div>
<div class="api-ref"><span class="method get">GET</span><span class="path">/api/portal/status</span><span class="desc">Portal connection status</span></div>
<div class="api-ref"><span class="method get">GET</span><span class="path">/api/wifi/status?pass=...</span><span class="desc">Full WiFi status with saved SSID</span></div>
<div class="api-ref"><span class="method post">POST</span><span class="path">/api/wifi/save</span><span class="desc">Save new WiFi credentials {ssid, pass, adminPass}</span></div>
<div class="api-ref"><span class="method post">POST</span><span class="path">/api/wifi/forget</span><span class="desc">Clear saved WiFi credentials {adminPass}</span></div>
<div class="api-ref"><span class="method post">POST</span><span class="path">/api/system/restart</span><span class="desc">Restart ESP32 {adminPass}</span></div>
</div>

</div>
<script>
var adminPass = new URLSearchParams(window.location.search).get('pass') || '';

function showWifiMsg(text, type) {
  var el = document.getElementById('wifiConfigMsg');
  el.textContent = text;
  el.className = 'msg ' + type;
}

function formatUptime(secs) {
  var d = Math.floor(secs / 86400);
  var h = Math.floor((secs % 86400) / 3600);
  var m = Math.floor((secs % 3600) / 60);
  var s = secs % 60;
  var parts = [];
  if (d > 0) parts.push(d + 'd');
  parts.push((h < 10 ? '0' : '') + h + 'h');
  parts.push((m < 10 ? '0' : '') + m + 'm');
  parts.push((s < 10 ? '0' : '') + s + 's');
  return parts.join(' ');
}

function updateStatus() {
  fetch('/api/status').then(function(r){return r.json()}).then(function(d){
    if (d.ut !== undefined) document.getElementById('uptime').textContent = formatUptime(d.ut);
    if (d.fh !== undefined) document.getElementById('freeHeap').textContent = (d.fh / 1024).toFixed(0) + ' KB';
    if (d.t !== undefined && d.t >= 0) {
      document.getElementById('sensorTemp').textContent = d.t.toFixed(1) + ' °C';
      document.getElementById('dhtStatus').innerHTML = '<span class="status-dot on"></span>' + d.t.toFixed(1) + ' °C / ' + (d.h !== undefined ? d.h.toFixed(1) : '--') + '%';
    } else {
      document.getElementById('sensorTemp').textContent = 'Error';
      document.getElementById('dhtStatus').innerHTML = '<span class="status-dot off"></span>Error';
    }
    if (d.h !== undefined && d.h >= 0) document.getElementById('sensorHumid').textContent = d.h.toFixed(1) + '%';
    if (d.s !== undefined) document.getElementById('fanSpeed').textContent = d.s + '%';
    if (d.r !== undefined) document.getElementById('fanRpm').textContent = d.r + ' RPM';
    if (d.sp !== undefined) document.getElementById('fanSetpoint').textContent = d.sp.toFixed(1) + ' °C';
    if (d.pid !== undefined) document.getElementById('pidOutput').textContent = d.pid;
    if (d.m !== undefined) {
      var mode = d.m ? 'AUTO' : 'MANUAL';
      var sub = '';
      if (d.m && d.am !== undefined) sub = d.am ? ' (Linear)' : ' (PID)';
      document.getElementById('fanStatus').innerHTML = (d.m ? '<span class="status-dot on"></span>' : '<span class="status-dot warn"></span>') + mode + sub;
    }
    if (d.mq !== undefined) {
      document.getElementById('mqttStatus').innerHTML = d.mq ? '<span class="status-dot on"></span>Connected' : '<span class="status-dot off"></span>Disconnected';
    }
  });
}

function updateWifiStatus() {
  if (!adminPass) return;
  fetch('/api/wifi/status?pass=' + encodeURIComponent(adminPass)).then(function(r){return r.json()}).then(function(d){
    if (d.connected) {
      document.getElementById('wifiStatus').innerHTML = '<span class="status-dot on"></span>Connected';
      document.getElementById('staSsid').textContent = d.ssid || '--';
      document.getElementById('staIp').textContent = d.ip || '--';
      document.getElementById('staSignal').textContent = (d.signal || '--') + ' dBm';
    } else {
      document.getElementById('wifiStatus').innerHTML = '<span class="status-dot off"></span>Disconnected';
      document.getElementById('staSsid').textContent = '--';
      document.getElementById('staIp').textContent = '--';
      document.getElementById('staSignal').textContent = '--';
    }
    document.getElementById('savedSsid').textContent = d.savedSsid || 'None';
  });
}

function doRestart() {
  if (!confirm('Are you sure you want to restart the ESP32?\n\nThe connection will be lost temporarily.')) return;
  var btn = document.getElementById('restartBtn');
  btn.disabled = true;
  btn.textContent = 'Restarting...';
  fetch('/api/system/restart', {
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify({adminPass: adminPass})
  });
}

function saveWifi() {
  var ssid = document.getElementById('newSsid').value.trim();
  var pass = document.getElementById('newPass').value;
  if (!ssid) { showWifiMsg('Enter an SSID', 'error'); return; }
  fetch('/api/wifi/save', {
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify({ssid:ssid, pass:pass, adminPass:adminPass})
  }).then(function(r){return r.json()}).then(function(d){
    if (d.ok) { showWifiMsg('Saved! Reconnecting...', 'success'); updateWifiStatus(); }
    else showWifiMsg(d.message || 'Failed', 'error');
  }).catch(function(){showWifiMsg('Request failed', 'error')});
}

function forgetWifi() {
  if (!confirm('Forget saved WiFi credentials?')) return;
  fetch('/api/wifi/forget', {
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify({adminPass:adminPass})
  }).then(function(r){return r.json()}).then(function(d){
    if (d.ok) { showWifiMsg('Credentials cleared', 'success'); updateWifiStatus(); }
    else showWifiMsg(d.message || 'Failed', 'error');
  }).catch(function(){showWifiMsg('Request failed', 'error')});
}

if (!adminPass) {
  document.body.innerHTML = '<div class="container" style="padding-top:40px;text-align:center"><h1>Access Denied</h1><p style="color:#8b949e;margin-top:12px">This page requires the admin password.</p><p style="color:#8b949e">Add <code>?pass=YOUR_PASSWORD</code> to the URL.</p></div>';
} else {
  updateStatus();
  updateWifiStatus();
  setInterval(updateStatus, 3000);
  setInterval(updateWifiStatus, 5000);
}
</script>
</body>
</html>
)rawliteral";
