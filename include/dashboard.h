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
.grid{display:grid;grid-template-columns:1fr 1fr 1fr 1fr;gap:16px;max-width:900px;margin:0 auto}
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
.mode-btn{font-size:1rem;padding:10px 24px}
input[type=range]{flex:1;min-width:120px;accent-color:#a371f7;height:6px;background:#2d303a;border-radius:3px;-webkit-appearance:none}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:18px;height:18px;border-radius:50%;background:#a371f7;cursor:pointer;border:2px solid #1c1e26}
input[type=range]::-moz-range-thumb{width:18px;height:18px;border-radius:50%;background:#a371f7;cursor:pointer;border:2px solid #1c1e26}
input[type=range]:disabled{opacity:.4}
input[type=range]:disabled::-webkit-slider-thumb{background:#555;cursor:default}
input[type=range]:disabled::-moz-range-thumb{background:#555;cursor:default}
.sp-slider{accent-color:#58a6ff}
.sp-slider::-webkit-slider-thumb{background:#58a6ff}
.sp-slider::-moz-range-thumb{background:#58a6ff}
.greyed{opacity:.4;pointer-events:none}
.chart-canvas{display:block;width:100%;height:90px;margin-top:8px}
.chart-legend{display:flex;gap:20px;margin:4px 0 8px 0;font-size:.75rem;color:#8b949e;justify-content:center}
.legend-dot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:4px;vertical-align:middle}
.status{display:flex;gap:16px;font-size:.8rem;color:#8b949e;margin:16px auto 0 auto;justify-content:center;max-width:900px}
.status .dot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:6px}
.status .dot.on{background:#3fb950;box-shadow:0 0 6px #3fb95066}
.status .dot.off{background:#da3633}
.slider-value{font-size:.9rem;font-weight:600;color:#a371f7;min-width:36px;text-align:right}
.setpoint-color{color:#58a6ff}
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

<div class="card" style="grid-column:span 2">
<div class="label">Mode</div>
<div class="controls">
<button id="modeAuto" class="mode-btn active-auto">AUTO</button>
<button id="modeManual" class="mode-btn active-manual">MANUAL</button>
</div>
</div>

<div class="card" id="ctrlModeCard" style="grid-column:span 2">
<div class="label">Control Mode</div>
<div class="controls">
<button id="modePid" class="mode-btn active-pid">PID</button>
<button id="modeLinear" class="mode-btn">Linear</button>
</div>
</div>

<div class="card" id="spCard" style="grid-column:1/-1">
<div class="label">Target Temperature</div>
<div class="controls">
<button id="spDown">&minus;</button>
<input type="range" id="spSlider" class="sp-slider" min="20" max="30" step="0.5" value="24">
<span class="value setpoint-color"><span id="setpoint">24.0</span><span class="unit">&deg;C</span></span>
<button id="spUp">+</button>
</div>
</div>

<div class="card" id="manualCard" style="grid-column:1/-1">
<div class="label">Manual Speed</div>
<div class="controls">
<input type="range" id="pwmSlider" min="0" max="100" value="0">
<span class="slider-value" id="sliderVal">0%</span>
</div>
</div>

<div class="card" style="grid-column:1/-1">
<div class="label">History (last 60s)</div>
<canvas id="chartTemp" class="chart-canvas"></canvas>
<div class="chart-legend"><span><span class="legend-dot" style="background:#f0883e"></span>Temperature</span></div>
<canvas id="chartHumid" class="chart-canvas"></canvas>
<div class="chart-legend"><span><span class="legend-dot" style="background:#58a6ff"></span>Humidity</span></div>
<canvas id="chartTargetRpm" class="chart-canvas"></canvas>
<div class="chart-legend"><span><span class="legend-dot" style="background:#a371f7"></span>Target RPM</span></div>
<canvas id="chartActualRpm" class="chart-canvas"></canvas>
<div class="chart-legend"><span><span class="legend-dot" style="background:#3fb950"></span>Actual RPM</span></div>
</div>

<div class="status">
<span><span class="dot" id="wsDot"></span>Dashboard</span>
<span><span class="dot" id="mqttDot"></span>MQTT</span>
</div>

<script>
var currentMode = 0;
var currentAutoMode = 0;
var sliderDragging = false;
var histTemp = [];
var histHumid = [];
var histTargetRpm = [];
var histActualRpm = [];
var histTime = [];
const MAX_HIST = 60;

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
      if(d.m){a.className='mode-btn active active-auto';m.className='mode-btn'}else{a.className='mode-btn';m.className='mode-btn active active-manual'}
    }
    if(d.am!==undefined){
      currentAutoMode=d.am;
      var pidBtn=document.getElementById('modePid'),linBtn=document.getElementById('modeLinear');
      if(d.am){linBtn.className='mode-btn active active-linear';pidBtn.className='mode-btn'}else{pidBtn.className='mode-btn active active-pid';linBtn.className='mode-btn'}
    }
    if(d.sp!==undefined){
      document.getElementById('setpoint').textContent=d.sp.toFixed(1);
      document.getElementById('spSlider').value=d.sp;
    }
    if(d.mq!==undefined)document.getElementById('mqttDot').className=d.mq?'dot on':'dot off';
    if(!sliderDragging&&d.s!==undefined){document.getElementById('pwmSlider').value=d.s;document.getElementById('sliderVal').textContent=d.s+'%'}

    var cc=document.getElementById('ctrlModeCard');
    cc.className=currentMode?'card':'card greyed';
    var sc=document.getElementById('spCard');
    sc.className=(currentMode&&currentAutoMode===0)?'card':'card greyed';
    document.getElementById('pwmSlider').disabled=currentMode?true:false;

    if(d.t!==undefined&&d.t>=0){histTemp.push(d.t);histHumid.push(d.h)}else{histTemp.push(null);histHumid.push(null)}
    if(d.r!==undefined){histActualRpm.push(d.r);histTargetRpm.push(d.s*50)}else{histActualRpm.push(null);histTargetRpm.push(null)}
    while(histTemp.length>MAX_HIST){histTemp.shift();histHumid.shift();histTargetRpm.shift();histActualRpm.shift()}
    drawCharts();
  }).catch(function(){document.getElementById('wsDot').className='dot off'});
}
setInterval(updateDash,1000);
updateDash();

function drawChart(id, data, yMin, yMax, color, lw, dash){
  var c=document.getElementById(id);
  var rect=c.getBoundingClientRect();
  if(rect.width<1||rect.height<1)return;
  var dpr=window.devicePixelRatio||1;
  c.width=rect.width*dpr;c.height=rect.height*dpr;
  var ctx=c.getContext('2d');
  ctx.scale(dpr,dpr);
  var w=rect.width,h=rect.height,pt=10,pr=10,pb=22,pl=40;
  var pw=w-pl-pr,ph=h-pt-pb;
  ctx.clearRect(0,0,w,h);
  ctx.strokeStyle='#2d303a';ctx.lineWidth=1;
  for(var i=0;i<=4;i++){var y=pt+ph*i/4;ctx.beginPath();ctx.moveTo(pl,y);ctx.lineTo(w-pr,y);ctx.stroke()}
  ctx.fillStyle='#8b949e';ctx.font='10px monospace';ctx.textAlign='right';ctx.textBaseline='middle';
  for(var i=0;i<=4;i++){var val=yMax-(yMax-yMin)*i/4;ctx.fillText(val.toFixed(0),pl-5,pt+ph*i/4)}
  ctx.textAlign='center';ctx.textBaseline='top';
  for(var i=0;i<=4;i++){var sec=MAX_HIST*i/4;ctx.fillText('-'+(MAX_HIST-sec).toFixed(0)+'s',pl+pw*i/4,h-pb+6)}
  if(!data||data.length<2)return;
  ctx.strokeStyle=color;ctx.lineWidth=lw||2;ctx.setLineDash(dash||[]);
  ctx.beginPath();var started=false;
  for(var i=0;i<data.length;i++){
    var x=pl+(i/(MAX_HIST-1))*pw;
    if(data[i]===null||data[i]===undefined){started=false;continue}
    var y=pt+ph-((data[i]-yMin)/(yMax-yMin))*ph;
    if(!started){ctx.moveTo(x,y);started=true}else ctx.lineTo(x,y)
  }
  ctx.stroke();
  ctx.setLineDash([]);
}

function drawCharts(){
  drawChart('chartTemp',histTemp,0,50,'#f0883e',2);
  drawChart('chartHumid',histHumid,0,100,'#58a6ff',1.5);
  drawChart('chartTargetRpm',histTargetRpm,0,10000,'#a371f7',1.5,[4,3]);
  drawChart('chartActualRpm',histActualRpm,0,10000,'#3fb950',2);
}

document.getElementById('spDown').onclick=function(){
  var sp=parseFloat(document.getElementById('setpoint').textContent);
  sp=Math.max(20,sp-0.5);
  document.getElementById('spSlider').value=sp;
  fetch('/api/cmd',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({cmd:'setpoint',v:sp})});
};
document.getElementById('spUp').onclick=function(){
  var sp=parseFloat(document.getElementById('setpoint').textContent);
  sp=Math.min(30,sp+0.5);
  document.getElementById('spSlider').value=sp;
  fetch('/api/cmd',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({cmd:'setpoint',v:sp})});
};
document.getElementById('spSlider').onchange=function(){
  var v=parseFloat(this.value);
  fetch('/api/cmd',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({cmd:'setpoint',v:v})});
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
