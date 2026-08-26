#pragma once
// dashboard.h — Dashboard web embutido (PROGMEM) — T-013.
// Direção estética: instrumento de bancada industrial, tema claro, tipografia
// Syne + IBM Plex Mono/Sans, viewport única (sem rolagem), gauges/gráficos de
// PV e MV, hints em todos os controles e polling assíncrono (fetch a cada 500 ms).

#include <Arduino.h>

static const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Controle Térmico</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Syne:wght@600;700;800&family=IBM+Plex+Mono:wght@400;500&family=IBM+Plex+Sans:wght@400;500;600&display=swap" rel="stylesheet">
<style>
:root{
  --bg:#F5F0E8; --ink:#1B1B1B; --muted:#6B6B66; --card:#FCFAF5;
  --line:#E4DCCF; --amber:#E5600C; --teal:#0F7D6B; --amber-soft:#FBE3D2;
  --teal-soft:#DDEBE4; --shadow:0 18px 40px -18px rgba(60,40,10,.35);
  --disp:'Syne',Georgia,serif; --mono:'IBM Plex Mono','Courier New',monospace;
  --sans:'IBM Plex Sans',system-ui,sans-serif;
}
*{box-sizing:border-box}
html,body{height:100%;margin:0}
body{
  font-family:var(--sans); color:var(--ink); background:var(--bg);
  overflow:hidden; position:relative;
  background-image:
    radial-gradient(1200px 600px at 12% -10%, #fff8ec 0%, transparent 60%),
    radial-gradient(900px 500px at 100% 110%, #f0e2cf 0%, transparent 60%),
    repeating-linear-gradient(45deg, rgba(27,27,27,.015) 0 2px, transparent 2px 6px);
}
.app{height:100vh; display:grid; grid-template-rows:auto auto 1fr; padding:18px 22px 20px; gap:14px}
/* Header */
header{display:flex; align-items:center; gap:18px}
.brand{display:flex; align-items:center; gap:14px}
.logo{width:14px;height:34px;border-radius:8px;background:linear-gradient(180deg,var(--amber),#B23E00);box-shadow:0 0 0 4px rgba(229,96,12,.15)}
h1{font-family:var(--disp); font-weight:800; font-size:clamp(18px,2.4vw,30px); line-height:.95; margin:0; letter-spacing:-.02em}
.brand small{font-family:var(--mono); color:var(--muted); font-size:10px; letter-spacing:.18em; text-transform:uppercase}
.spacer{flex:1}
.badges{display:flex; gap:10px; align-items:center}
.badge{font-family:var(--mono); font-size:11px; padding:7px 12px; border-radius:999px; border:1px solid var(--line); background:var(--card)}
.badge i{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:7px;background:var(--teal)}
.badge.alarm i{background:var(--amber); box-shadow:0 0 0 4px var(--amber-soft)}
.badge.alarm{color:#8a3c00; border-color:#ecc7a8}
.mode-pill{font-family:var(--mono); font-size:12px; padding:8px 16px; border-radius:999px; background:var(--ink); color:#fff; font-weight:500}
/* Health bar */
.healthbar{display:flex; gap:10px; flex-wrap:wrap; align-items:center}
.hchip{font-family:var(--mono); font-size:11px; color:var(--muted); background:var(--card); border:1px solid var(--line); border-radius:999px; padding:5px 12px; display:inline-flex; gap:6px; align-items:center}
.hchip b{color:var(--ink); font-weight:500}
.hchip .warn{color:var(--amber)}
/* Main grid */
.main{display:grid; grid-template-columns:1.15fr 1.15fr .95fr; gap:16px; min-height:0}
.card{background:var(--card); border:1px solid var(--line); border-radius:22px; padding:18px 18px 16px; box-shadow:var(--shadow); display:flex; flex-direction:column; min-height:0; position:relative; overflow:hidden}
.card::before{content:""; position:absolute; top:0; left:0; right:0; height:4px; background:var(--accent, linear-gradient(90deg,var(--amber),#D98A00))}
.card.pv{--accent:linear-gradient(90deg,var(--teal),#0a5a4e)}
.card.mv{--accent:linear-gradient(90deg,var(--amber),#D98A00)}
.card-title{display:flex; justify-content:space-between; align-items:baseline; margin-bottom:4px}
.card-title h2{font-family:var(--disp); font-size:15px; font-weight:700; margin:0; letter-spacing:.02em; text-transform:uppercase}
.card-title .unit{font-family:var(--mono); font-size:11px; color:var(--muted)}
.reading{display:flex; align-items:center; gap:14px}
.bignum{font-family:var(--mono); font-size:clamp(34px,4.4vw,56px); font-weight:500; line-height:1; letter-spacing:-.03em; min-width:108px}
.bignum small{font-size:.42em; color:var(--muted); font-weight:400; margin-left:4px}
/* Gauge */
.gauge{width:110px; height:64px; flex:none}
.gauge svg{width:100%; height:100%; overflow:visible}
.gauge .bg{fill:none; stroke:#E9E1D3; stroke-width:12; stroke-linecap:round; pathLength:100; stroke-dasharray:100 100}
.gauge .val{fill:none; stroke:var(--teal); stroke-width:12; stroke-linecap:round; pathLength:100; stroke-dasharray:0 100; transition:stroke-dasharray .7s cubic-bezier(.2,.8,.2,1), stroke .3s}
/* Chart */
.chart{flex:1; min-height:136px; margin-top:10px}
.chart canvas{width:100%; height:100%; display:block}
/* Control panel */
.ctrl{background:var(--ink); color:#F5F0E8; border-radius:22px; padding:18px; display:flex; flex-direction:column; gap:14px; min-height:0; overflow:auto}
.ctrl h2{font-family:var(--disp); font-size:14px; font-weight:700; margin:0 0 2px; text-transform:uppercase; letter-spacing:.04em}
.ctrl .sub{font-family:var(--mono); color:#9a9a91; font-size:10px; letter-spacing:.14em; text-transform:uppercase}
.field{display:flex; flex-direction:column; gap:5px}
.field label{font-family:var(--mono); font-size:11px; color:#c8c8bf; letter-spacing:.04em}
input[type=number],input[type=range],select{width:100%}
input[type=number],select{
  font-family:var(--mono); background:#242424; color:#fff; border:1px solid #3a3a3a;
  border-radius:12px; padding:9px 12px; font-size:14px
}
input[type=number]:focus,select:focus{outline:2px solid var(--amber); outline-offset:1px}
input[type=range]{-webkit-appearance:none; appearance:none; height:4px; background:#3a3a3a; border-radius:999px}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none; width:18px; height:18px; border-radius:50%; background:var(--amber); cursor:pointer; border:3px solid #1B1B1B; box-shadow:0 0 0 2px var(--amber)}
.btnrow{display:flex; gap:8px}
button{font-family:var(--sans); font-weight:600; font-size:13px; border:1px solid #3a3a3a; background:#2a2a2a; color:#fff; border-radius:12px; padding:9px 14px; cursor:pointer; transition:.18s}
button:hover{background:#3a3a3a; transform:translateY(-1px)}
button.active{background:var(--amber); border-color:var(--amber); color:#fff}
button.primary{background:var(--teal); border-color:var(--teal)}
button.primary:hover{background:#0a5a4e}
.chk{display:flex; align-items:center; gap:9px; font-family:var(--mono); font-size:12px; color:#dcdcd2}
.chk input{width:16px; height:16px; accent-color:var(--amber); cursor:pointer}
.hint{font-family:var(--sans); font-size:11px; color:#8a8a80; line-height:1.35}
.hint b{color:#cfcfc5}
/* Animations */
.card,.ctrl{opacity:0; transform:translateY(14px); animation:rise .6s cubic-bezier(.2,.8,.2,1) forwards}
.card:nth-child(1){animation-delay:.05s}
.card:nth-child(2){animation-delay:.15s}
.ctrl{animation-delay:.25s}
@keyframes rise{to{opacity:1; transform:none}}
@media(max-width:760px){
  .main{grid-template-columns:1fr 1fr}
  .ctrl{grid-column:1 / -1}
}
</style>
</head>
<body>
<div class="app">
  <header>
    <div class="brand">
      <span class="logo"></span>
      <div>
        <h1>Controle<br>Térmico</h1>
        <small>NodeMCU · ESP8266</small>
      </div>
    </div>
    <div class="spacer"></div>
    <div class="badges">
      <span class="badge" id="sensor-badge" title="Estado do sensor de temperatura: OK ou ERRO (falha de leitura)."><i></i>SENSOR</span>
      <span class="badge" id="alarm-badge" title="Alarme: ativa em ≥80 °C e desliga quando a temperatura fica <78 °C."><i></i>ALARME</span>
      <span class="mode-pill" id="mode-pill" title="Modo atual do controlador: Manual, Automático ou Autotuning.">MANUAL</span>
    </div>
  </header>

  <div class="healthbar">
    <span class="hchip" title="Frequência do clock da CPU (MHz).">CPU <b id="h-cpu">--</b></span>
    <span class="hchip" title="Carga de trabalho do loop (tempo ocupado, %).">Carga <b id="h-load">--</b></span>
    <span class="hchip" title="Ociosidade do MCU (100% - carga).">Idle <b id="h-idle">--</b></span>
    <span class="hchip" title="Heap (RAM) livre e fragmentação do heap.">Heap <b id="h-heap">--</b></span>
    <span class="hchip" title="Tempo de execução desde o boot.">Uptime <b id="h-up">--</b></span>
    <span class="hchip" title="Clientes conectados ao Access Point.">WiFi <b id="h-wifi">--</b></span>
  </div>

  <div class="main">
    <section class="card pv">
      <div class="card-title"><h2>Variável de Processo</h2><span class="unit">PV · °C</span></div>
      <div class="reading">
        <div class="bignum" title="Temperatura medida (PV), faixa 20–90 °C."><span id="pv-val">--</span><small>°C</small></div>
        <div class="gauge" title="Indicador da temperatura (PV): o arco se preenche de 20 a 90 °C.">
          <svg viewBox="0 0 110 64">
            <path class="bg" d="M15 58 A40 40 0 0 1 95 58"></path>
            <path class="val" id="pv-g" d="M15 58 A40 40 0 0 1 95 58"></path>
          </svg>
        </div>
      </div>
      <div class="chart" title="Histórico da temperatura (PV) ao longo do tempo."><canvas id="pv-chart"></canvas></div>
    </section>

    <section class="card mv">
      <div class="card-title"><h2>Variável Manipulada</h2><span class="unit">MV · %</span></div>
      <div class="reading">
        <div class="bignum" title="Potência aplicada à resistência (MV), faixa 0–100%."><span id="mv-val">--</span><small>%</small></div>
        <div class="gauge" title="Indicador da potência (MV): o arco se preenche de 0 a 100%.">
          <svg viewBox="0 0 110 64">
            <path class="bg" d="M15 58 A40 40 0 0 1 95 58"></path>
            <path class="val" id="mv-g" d="M15 58 A40 40 0 0 1 95 58" style="stroke:var(--amber)"></path>
          </svg>
        </div>
      </div>
      <div class="chart" title="Histórico da potência (MV) ao longo do tempo."><canvas id="mv-chart"></canvas></div>
    </section>

    <section class="ctrl">
      <div>
        <h2>Painel de Controle</h2>
        <p class="sub">Ajuste e sintonia em tempo real</p>
      </div>

      <div class="field">
        <label for="sp">Setpoint (20–80 °C)<span class="hint" title="Temperatura alvo do controle (modo automático)."> ?</span></label>
        <input type="number" id="sp" min="20" max="80" step="0.5" value="50" title="Temperatura desejada. Aceita 20 a 80 °C; no modo automático o PID busca este valor.">
        <span class="hint">Defina a temperatura desejada (<b>20 a 80 °C</b>).</span>
      </div>

      <div class="field">
        <label>Modo de operação <span class="hint" title="Manual = potência definida por você; Auto = PID mantém o setpoint."> ?</span></label>
        <div class="btnrow">
          <button id="btn-manual" class="active" title="Modo manual (malha aberta): você define a potência da resistência. O PID é pausado e resetado.">Manual</button>
          <button id="btn-auto" title="Modo automático (malha fechada): o PID modula a resistência para manter o setpoint.">Automático</button>
        </div>
      </div>

      <div class="field">
        <label for="slider">Potência da resistência (0–100%)</label>
        <input type="range" id="slider" min="0" max="100" value="0" title="Potência da resistência (PWM de 10 bits). No modo manual define a MV diretamente (0 a 100%).">
        <div style="display:flex;justify-content:space-between;font-family:var(--mono);font-size:12px;color:#9a9a91"><span id="slider-val">0%</span><span>PWM</span></div>
        <span class="hint">No modo <b>manual</b> define a MV diretamente (malha aberta).</span>
      </div>

      <div class="field">
        <label>Parâmetros PID <span class="hint" title="Ganhos proporcional, integral e derivativo. I é sempre ativo."> ?</span></label>
        <div class="btnrow" style="gap:8px">
          <input type="number" id="kp" step="0.1" value="1.5" placeholder="P" title="Ganho proporcional (P): resposta imediata ao erro. Pode ser habilitado/desabilitado.">
          <input type="number" id="ki" step="0.1" value="0.2" placeholder="I" title="Ganho integral (I): elimina o erro de regime permanente. Sempre ativo.">
          <input type="number" id="kd" step="0.01" value="0.05" placeholder="D" title="Ganho derivativo (D): amortecimento e antecipação de tendência. Pode ser habilitado/desabilitado.">
        </div>
        <div class="btnrow">
          <label class="chk"><input type="checkbox" id="enP" checked title="Habilita/desabilita o termo proporcional (P)."> habilitar P</label>
          <label class="chk"><input type="checkbox" id="enD" title="Habilita/desabilita o termo derivativo (D)."> habilitar D</label>
        </div>
        <span class="hint">A integral (<b>I</b>) não pode ser desabilitada.</span>
      </div>

      <div class="field">
        <label for="tmethod">Autotuning <span class="hint" title="Identifica P, I e D automaticamente para o processo."> ?</span></label>
        <div class="btnrow">
          <select id="tmethod" title="Método de autotuning: relé (Ziegler-Nichols) identifica o período crítico; degrau (Cohen-Coon) estima o modelo FOPDT.">
            <option value="relay">Relé (Ziegler-Nichols)</option>
            <option value="step">Degrau (Cohen-Coon)</option>
          </select>
          <button id="btn-tune" class="primary" title="Inicia a sintonia automática de P, I e D. O sistema excita a planta e aplica as constantes ao terminar.">Iniciar</button>
        </div>
        <span class="hint">Excita a planta e calcula as constantes. <b>Requer modo seguro.</b></span>
      </div>
    </section>
  </div>
</div>

<script>
const POLL_MS = 1000;
let pvHist=[], mvHist=[];
const PN=20, PX=90; // PV range

const $=id=>document.getElementById(id);
function clamp(v,a,b){return Math.max(a,Math.min(b,v));}

async function postControl(body){
  const r=await fetch('/api/control',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
  if(r.ok){ fetchState(); }
}

let fetching=false;
async function fetchState(){
  if(fetching) return;               // evita requisições sobrepostas (esp8266)
  fetching=true;
  try{
    const ctrl=new AbortController();
    const t=setTimeout(()=>ctrl.abort(),2500);
    const s=await (await fetch('/api/state',{signal:ctrl.signal})).json();
    clearTimeout(t);
    updateUI(s);
  }catch(e){}finally{fetching=false;}
}

function setGauge(id,pct,color){
  const el=$(id); el.style.strokeDasharray=clamp(pct,0,100)+' 100'; el.style.stroke=color;
}

function drawChart(id,data,lo,hi,step,color){
  const c=$(id); const ctx=c.getContext('2d');
  const dpr=window.devicePixelRatio||1;
  const w=c.clientWidth*dpr, h=c.clientHeight*dpr;
  if(c.width!==w||c.height!==h){c.width=w;c.height=h;}
  ctx.clearRect(0,0,w,h);

  // Margens: esquerda (escala Y), inferior (eixo X / tempo), topo e direita.
  const left=38*dpr, right=10*dpr, top=8*dpr, bottom=30*dpr;
  const pw=w-left-right, ph=h-top-bottom;
  if(pw<=0||ph<=0) return;

  ctx.lineWidth=1.5*dpr; ctx.font=(9*dpr)+'px "IBM Plex Mono", monospace';

  // Grade horizontal (leituras) em passos de `step` — cinza claro.
  ctx.textAlign='right'; ctx.textBaseline='middle';
  for(let v=lo; v<=hi+0.001; v+=step){
    const y=Math.round(top+(1-(v-lo)/(hi-lo))*ph);
    ctx.strokeStyle='rgba(115,115,115,.55)';
    ctx.beginPath(); ctx.moveTo(left,y); ctx.lineTo(left+pw,y); ctx.stroke();
    ctx.fillStyle='rgba(27,27,27,.55)';
    ctx.fillText(((Math.round(v*10)/10)).toFixed((hi-lo)<20?1:0), left-5, y);
  }

  // Grade vertical (tempo) + rótulos do eixo X.
  const tcount=4;                       // 4 intervalos -> 5 linhas
  const spanSec=(data.length>1)?((data.length-1)*(POLL_MS/1000)):30;
  ctx.textAlign='center'; ctx.textBaseline='top';
  for(let i=0;i<=tcount;i++){
    const x=Math.round(left+(i/tcount)*pw);
    ctx.strokeStyle='rgba(115,115,115,.45)';
    ctx.beginPath(); ctx.moveTo(x,top); ctx.lineTo(x,top+ph); ctx.stroke();
    const secs=Math.round(spanSec*(1-i/tcount));
    ctx.fillStyle='rgba(27,27,27,.5)';
    ctx.fillText(secs===0?'agora':('-'+secs+'s'), x, top+ph+3);
  }

  // Unidade do eixo X (tempo).
  ctx.font=(8*dpr)+'px "IBM Plex Mono", monospace';
  ctx.fillStyle='rgba(27,27,27,.4)'; ctx.textAlign='center';
  ctx.fillText('tempo (s)', left+pw/2, top+ph+16);

  // Linha do valor.
  if(data.length<2) return;
  ctx.beginPath(); ctx.strokeStyle=color; ctx.lineWidth=2*dpr; ctx.lineJoin='round';
  data.forEach((v,i)=>{
    const x=left+(i/(data.length-1))*pw;
    const y=top+(1-(clamp(v,lo,hi)-lo)/(hi-lo))*ph;
    i?ctx.lineTo(x,y):ctx.moveTo(x,y);
  });
  ctx.stroke();
  const lx=left+pw, ly=top+(1-(clamp(data[data.length-1],lo,hi)-lo)/(hi-lo))*ph;
  ctx.beginPath(); ctx.arc(lx,ly,3*dpr,0,7); ctx.fillStyle=color; ctx.fill();
}

function updateUI(s){
  $('pv-val').textContent=(s.pv!=null)?s.pv.toFixed(1):'--';
  $('mv-val').textContent=(s.mv!=null)?s.mv.toFixed(0):'--';
  $('sp').value=s.setpoint;
  const pvPct=((s.pv-PN)/(PX-PN))*100;
  setGauge('pv-g',pvPct, s.alarm?'#E5600C':'#0F7D6B');
  setGauge('mv-g',s.mv,'#E5600C');
  const sb=$('sensor-badge');
  if(s.sensor_fail){sb.classList.add('alarm');sb.innerHTML='<i></i>ERRO SENSOR';}
  else{sb.classList.remove('alarm');sb.innerHTML='<i></i>SENSOR';}
  const ab=$('alarm-badge');
  ab.classList.toggle('alarm',s.alarm);
  ab.innerHTML= s.alarm?'<i></i>ALARME':'<i></i>ALARME';
  $('mode-pill').textContent=(s.mode||'').toUpperCase();
  $('btn-manual').classList.toggle('active',s.mode==='manual');
  $('btn-auto').classList.toggle('active',s.mode==='auto');
  $('slider').value=s.mv; $('slider-val').textContent=Math.round(s.mv)+'%';
  $('kp').value=s.pid.p; $('ki').value=s.pid.i; $('kd').value=s.pid.d;
  $('enP').checked=s.pid.enableP; $('enD').checked=s.pid.enableD;
  pvHist.push(s.pv); if(pvHist.length>60)pvHist.shift();
  mvHist.push(s.mv); if(mvHist.length>60)mvHist.shift();
  drawChart('pv-chart',pvHist,PN,PX,10,'#0F7D6B');
  drawChart('mv-chart',mvHist,0,100,10,'#E5600C');

  // Saúde do MCU (cabeçalho).
  const h=s.health||{};
  $('h-cpu').textContent=(h.cpu!=null)?(Math.round(h.cpu)+' MHz'):'--';
  $('h-load').textContent=(h.load!=null)?(h.load.toFixed(1)+'%'):'--';
  $('h-idle').textContent=(h.idle!=null)?(h.idle.toFixed(1)+'%'):'--';
  const heapKB=(h.heap!=null)?(h.heap/1024):null;
  $('h-heap').textContent=(heapKB!=null)?(heapKB.toFixed(1)+' KB · '+Math.round(h.frag||0)+'%'):'--';
  $('h-up').textContent=(h.up!=null)?fmtUptime(h.up):'--';
  $('h-wifi').textContent=(h.wifi!=null)?(h.wifi+' cli'):'--';
}

function fmtUptime(sec){
  if(sec==null)return '--';
  const h=Math.floor(sec/3600), m=Math.floor((sec%3600)/60), s2=Math.floor(sec%60);
  return h>0? (h+'h '+m+'m') : (m>0? (m+'m '+s2+'s') : (s2+'s'));
}

$('sp').addEventListener('change',()=>postControl({setpoint:parseFloat($('sp').value)}));
$('btn-manual').addEventListener('click',()=>postControl({mode:'manual'}));
$('btn-auto').addEventListener('click',()=>postControl({mode:'auto'}));
$('slider').addEventListener('input',()=>$('slider-val').textContent=$('slider').value+'%');
$('slider').addEventListener('change',()=>postControl({mv:parseFloat($('slider').value),mode:'manual'}));
$('kp').addEventListener('change',()=>postControl({pid:{p:parseFloat($('kp').value)}}));
$('ki').addEventListener('change',()=>postControl({pid:{i:parseFloat($('ki').value)}}));
$('kd').addEventListener('change',()=>postControl({pid:{d:parseFloat($('kd').value)}}));
$('enP').addEventListener('change',()=>postControl({pid:{enableP:$('enP').checked}}));
$('enD').addEventListener('change',()=>postControl({pid:{enableD:$('enD').checked}}));
$('btn-tune').addEventListener('click',()=>postControl({tuning:{method:$('tmethod').value}}));

fetchState();
setInterval(fetchState,POLL_MS);
</script>
</body>
</html>
)rawliteral";
