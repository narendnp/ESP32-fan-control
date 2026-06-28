#pragma once

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>ESP32 Fan Dashboard</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Oxygen,Ubuntu,sans-serif;background:#0f1117;color:#e1e4e8;padding:20px;min-height:100vh}
h1{font-size:1.4rem;font-weight:600;margin-bottom:20px;color:#58a6ff}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:16px;max-width:900px;margin:0 auto}
.card{background:#1c1e26;border-radius:12px;padding:20px;border:1px solid #2d303a}
.card .label{font-size:.75rem;text-transform:uppercase;letter-spacing:.05em;color:#8b949e;margin-bottom:8px}
.card .value{font-size:2rem;font-weight:700;font-variant-numeric:tabular-nums}
.card .unit{font-size:1rem;font-weight:400;color:#8b949e;margin-left:4px}
.temp{color:#f0883e}
.humid{color:#58a6ff}
.pwm{color:#a371f7}
.rpm{color:#3fb950}
.bar{height:6px;background:#2d303a;border-radius:3px;margin-top:12px;overflow:hidden}
.bar-fill{height:100%;border-radius:3px;transition:width .3s ease}
.controls{display:flex;gap:12px;align-items:center;flex-wrap:wrap;margin-top:8px}
button{background:#2d303a;color:#e1e4e8;border:1px solid #444;border-radius:8px;padding:8px 16px;cursor:pointer;font-size:.85rem;font-weight:500;transition:all .2s}
button:hover{background:#3d404a;border-color:#666}
button.active{background:#238636;border-color:#2ea043;color:#fff}
button.active-auto{background:#1f6feb;border-color:#58a6ff;color:#fff}
button.active-manual{background:#da8a3e;border-color:#f0883e;color:#fff}
button.active-pid{background:#1f6feb;border-color:#58a6ff;color:#fff}
button.active-linear{background:#2ea043;border-color:#3fb950;color:#fff}
#autoModeToggle{display:none}
#autoModeToggle button{font-size:.8rem;padding:4px 12px}
input[type=range]{flex:1;min-width:120px;accent-color:#a371f7;height:6px;background:#2d303a;border-radius:3px;-webkit-appearance:none}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:18px;height:18px;border-radius:50%;background:#a371f7;cursor:pointer;border:2px solid #1c1e26}
input[type=range]::-moz-range-thumb{width:18px;height:18px;border-radius:50%;background:#a371f7;cursor:pointer;border:2px solid #1c1e26}
input[type=range]:disabled{opacity:.4}
input[type=range]:disabled::-webkit-slider-thumb{background:#555;cursor:default}
input[type=range]:disabled::-moz-range-thumb{background:#555;cursor:default}
.status{display:flex;gap:16px;font-size:.8rem;color:#8b949e;margin-top:16px;justify-content:center}
.status .dot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:6px}
.status .dot.on{background:#3fb950;box-shadow:0 0 6px #3fb95066}
.status .dot.off{background:#da3633}
.slider-value{font-size:.9rem;font-weight:600;color:#a371f7;min-width:36px;text-align:right}
.setpoint-color{color:#58a6ff}
.setpoint-bar{background:#58a6ff}
</style>
</head>
<body>
<h1>&#9881; Fan Control Dashboard</h1>
<div class="grid">

<div class="card">
<div class="label">Temperature</div>
<div class="value temp"><span id="temp">--</span><span class="unit">&deg;C</span></div>
<div class="bar"><div class="bar-fill temp" id="tempBar" style="width:0%"></div></div>
</div>

<div class="card">
<div class="label">Humidity</div>
<div class="value humid"><span id="humid">--</span><span class="unit">%</span></div>
<div class="bar"><div class="bar-fill humid" id="humidBar" style="width:0%"></div></div>
</div>

<div class="card">
<div class="label">Fan Speed</div>
<div class="value pwm"><span id="speed">--</span><span class="unit">%</span></div>
<div class="bar"><div class="bar-fill pwm" id="speedBar" style="width:0%"></div></div>
</div>

<div class="card">
<div class="label">RPM</div>
<div class="value rpm"><span id="rpm">--</span><span class="unit">RPM</span></div>
<div class="bar"><div class="bar-fill rpm" id="rpmBar" style="width:0%"></div></div>
</div>

<div class="card" style="grid-column:1/-1">
<div class="label">Target Temperature</div>
<div class="controls">
<button id="spDown">&minus;</button>
<span class="value setpoint-color"><span id="setpoint">24.0</span><span class="unit">&deg;C</span></span>
<button id="spUp">+</button>
</div>
<div class="bar"><div class="bar-fill setpoint-bar" id="spBar" style="width:50%"></div></div>
</div>

<div class="card" style="grid-column:1/-1">
<div class="label">Mode</div>
<div class="controls">
<button id="modeAuto" class="active-auto">AUTO</button>
    <button id="modeManual" class="active-manual">MANUAL</button>
  </div>
  <div class="controls" id="autoModeToggle">
    <button id="modePid" class="active-pid">PID</button>
    <button id="modeLinear" class="">Linear</button>
  </div>
</div>

<div class="card" style="grid-column:1/-1">
<div class="label">Manual Speed</div>
<div class="controls">
<input type="range" id="pwmSlider" min="0" max="100" value="0">
<span class="slider-value" id="sliderVal">0%</span>
</div>
</div>

</div>

<div class="status">
<span><span class="dot" id="wsDot"></span>Dashboard</span>
<span><span class="dot" id="mqttDot"></span>MQTT</span>
</div>

<script>
var currentMode = 0;
var sliderDragging = false;

function updateDash(){
  fetch('/api/status').then(function(r){return r.json()}).then(function(d){
    document.getElementById('wsDot').className='dot on';
    if(d.t!==undefined&&d.t>=0)document.getElementById('temp').textContent=d.t.toFixed(1);
    if(d.h!==undefined&&d.h>=0)document.getElementById('humid').textContent=d.h.toFixed(1);
    if(d.s!==undefined){document.getElementById('speed').textContent=d.s;document.getElementById('speedBar').style.width=d.s+'%'}
    if(d.r!==undefined){var rp=d.r;document.getElementById('rpm').textContent=rp<1000?rp:rp>=10000?(rp/1000).toFixed(1)+'k':(rp/1000).toFixed(2)+'k'}
    if(d.p!==undefined){document.getElementById('tempBar').style.width=Math.min(100,d.p/255*100)+'%';document.getElementById('rpmBar').style.width=Math.min(100,d.p/255*100)+'%'}
    if(d.h!==undefined&&d.h>=0)document.getElementById('humidBar').style.width=Math.min(100,d.h)+'%';
    if(d.m!==undefined){
      currentMode=d.m;
      var a=document.getElementById('modeAuto'),m=document.getElementById('modeManual');
      if(d.m){a.className='active active-auto';m.className=''}else{a.className='';m.className='active active-manual'}
      var tog=document.getElementById('autoModeToggle');
      if(d.m){
        tog.style.display='flex';
        var pidBtn=document.getElementById('modePid'),linBtn=document.getElementById('modeLinear');
        if(d.am!==undefined){
          if(d.am){linBtn.className='active active-linear';pidBtn.className=''}
          else{pidBtn.className='active active-pid';linBtn.className=''}
        }
      }else{tog.style.display='none'}
    }
    if(d.sp!==undefined){document.getElementById('setpoint').textContent=d.sp.toFixed(1);document.getElementById('spBar').style.width=((d.sp-20)/10*100)+'%'}
    if(d.mq!==undefined)document.getElementById('mqttDot').className=d.mq?'dot on':'dot off';
    if(!sliderDragging&&d.s!==undefined){document.getElementById('pwmSlider').value=d.s;document.getElementById('sliderVal').textContent=d.s+'%'}
  }).catch(function(){document.getElementById('wsDot').className='dot off'});
}
setInterval(updateDash,1000);
updateDash();

document.getElementById('spDown').onclick=function(){
  var sp=parseFloat(document.getElementById('setpoint').textContent);
  sp=Math.max(20,sp-0.5);
  fetch('/api/cmd',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({cmd:'setpoint',v:sp})});
};
document.getElementById('spUp').onclick=function(){
  var sp=parseFloat(document.getElementById('setpoint').textContent);
  sp=Math.min(30,sp+0.5);
  fetch('/api/cmd',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({cmd:'setpoint',v:sp})});
};

document.getElementById('modeAuto').onclick=function(){
  fetch('/api/cmd',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({cmd:'mode_auto'})});
};
document.getElementById('modeManual').onclick=function(){
  fetch('/api/cmd',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({cmd:'mode_manual'})});
};

document.getElementById('modePid').onclick=function(){
  fetch('/api/cmd',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({cmd:'automode',v:'pid'})});
};
document.getElementById('modeLinear').onclick=function(){
  fetch('/api/cmd',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({cmd:'automode',v:'linear'})});
};

document.getElementById('pwmSlider').oninput=function(){
  sliderDragging=true;
  var v=parseInt(this.value);
  document.getElementById('sliderVal').textContent=v+'%';
};
document.getElementById('pwmSlider').onchange=function(){
  var v=parseInt(this.value);
  fetch('/api/cmd',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({cmd:'pwm',v:v})});
  setTimeout(function(){sliderDragging=false},200);
};
</script>
</body>
</html>
)rawliteral";
