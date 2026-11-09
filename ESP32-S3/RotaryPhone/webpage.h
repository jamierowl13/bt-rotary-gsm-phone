// Web interface served by the ESP32 (rotary dial + green/red buttons)
#pragma once
#include <pgmspace.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Rotary Phone</title>
<style>
:root{--bg:#1d1a17;--panel:#2a2622;--ink:#f3ead8;--muted:#a99c87;--cream:#efe4cc;--plate:#141210;
--green:#1f9d4c;--green2:#27c45f;--red:#c62828;--red2:#ef4040;--brass:#c9a45c}
*{box-sizing:border-box}
body{margin:0;background:radial-gradient(circle at 50% 0,#3a332b,var(--bg) 70%);color:var(--ink);
font-family:Georgia,"Times New Roman",serif;min-height:100vh;display:flex;justify-content:center}
.wrap{width:100%;max-width:440px;padding:16px}
header{display:flex;justify-content:space-between;align-items:center;margin-bottom:12px}
h1{font-size:20px;margin:0;letter-spacing:.06em}
.sig{display:flex;align-items:flex-end;gap:2px;height:16px}
.sig i{display:block;width:4px;background:#5a5148;border-radius:1px}
.sig i.on{background:var(--brass)}
.meta{font:12px system-ui,sans-serif;color:var(--muted);text-align:right}
.screen{background:#0f1a12;border:2px solid #000;border-radius:10px;padding:10px 14px;margin-bottom:14px;
box-shadow:inset 0 0 18px rgba(0,0,0,.8)}
.state{font:600 13px system-ui,sans-serif;letter-spacing:.12em;text-transform:uppercase;color:#7fd48f}
.num{font:28px "Courier New",monospace;color:#bff5c8;min-height:36px;word-break:break-all;
text-shadow:0 0 8px rgba(120,255,150,.5)}
.sub{font:13px system-ui,sans-serif;color:#7fae86;min-height:18px}
.state.ring{color:#ffcf6e;animation:blink .6s steps(2) infinite}
@keyframes blink{50%{opacity:.25}}
svg{width:100%;height:auto;display:block;touch-action:none;user-select:none;-webkit-user-select:none}
.hole{cursor:grab}
.btns{display:flex;justify-content:space-around;margin:16px 0 8px}
.big{width:96px;height:96px;border-radius:50%;border:0;color:#fff;cursor:pointer;display:flex;
flex-direction:column;align-items:center;justify-content:center;gap:2px;font:600 13px system-ui,sans-serif;
box-shadow:0 6px 0 rgba(0,0,0,.45),inset 0 2px 6px rgba(255,255,255,.25);transition:transform .08s}
.big:active{transform:translateY(4px);box-shadow:0 2px 0 rgba(0,0,0,.45)}
.big svg{width:34px;height:34px}
.green{background:radial-gradient(circle at 35% 30%,var(--green2),var(--green))}
.red{background:radial-gradient(circle at 35% 30%,var(--red2),var(--red))}
.green.pulse{animation:pulse 1s infinite}
@keyframes pulse{0%{box-shadow:0 0 0 0 rgba(39,196,95,.8),0 6px 0 rgba(0,0,0,.45)}
100%{box-shadow:0 0 0 22px rgba(39,196,95,0),0 6px 0 rgba(0,0,0,.45)}}
.small{display:flex;justify-content:center;gap:8px;margin-bottom:16px}
.small button{background:var(--panel);color:var(--ink);border:1px solid #4a423a;border-radius:8px;
padding:8px 12px;font:13px system-ui,sans-serif;cursor:pointer}
.log{background:var(--panel);border-radius:10px;padding:8px 12px;font:14px system-ui,sans-serif}
.log h2{font:600 12px system-ui,sans-serif;letter-spacing:.12em;text-transform:uppercase;color:var(--muted);margin:4px 0 6px}
.log div.e{display:flex;justify-content:space-between;padding:7px 0;border-top:1px solid #3a342e;cursor:pointer}
.log .m{color:#ef8a8a}.log small{color:var(--muted)}
.hint{font:12px system-ui,sans-serif;color:var(--muted);text-align:center;margin:6px 0 0}
</style></head><body><div class="wrap">
<header><h1>☎ ROTARY PHONE</h1>
<div class="meta"><div class="sig" id="sig"><i style="height:4px"></i><i style="height:7px"></i><i style="height:10px"></i><i style="height:13px"></i><i style="height:16px"></i></div>
<span id="op">—</span></div></header>

<div class="screen"><div class="state" id="state">connecting…</div>
<div class="num" id="num"></div><div class="sub" id="sub"></div></div>

<svg id="dial" viewBox="-160 -160 320 320" aria-label="Rotary dial">
<defs>
<radialGradient id="gPlate" cx="35%" cy="30%"><stop offset="0" stop-color="#3b3631"/><stop offset="1" stop-color="#0d0b0a"/></radialGradient>
<radialGradient id="gCenter" cx="40%" cy="35%"><stop offset="0" stop-color="#fffaf0"/><stop offset="1" stop-color="#ddd0b4"/></radialGradient>
<linearGradient id="gStop" x1="0" x2="1"><stop offset="0" stop-color="#eee"/><stop offset=".5" stop-color="#999"/><stop offset="1" stop-color="#ddd"/></linearGradient>
<mask id="holes"><circle r="150" fill="#fff"/><g id="maskHoles"></g></mask>
</defs>
<circle r="156" fill="#0a0908"/>
<circle r="150" fill="var(--cream)"/>
<g id="numbers" font-family="Georgia,serif" font-size="26" font-weight="bold" text-anchor="middle" fill="#1a1612"></g>
<g id="plate">
  <circle r="148" fill="url(#gPlate)" mask="url(#holes)" opacity=".96"/>
  <circle r="148" fill="none" stroke="#000" stroke-width="2"/>
  <g id="hitHoles"></g>
</g>
<circle r="58" fill="url(#gCenter)" stroke="#8a7b62" stroke-width="2"/>
<text y="-12" text-anchor="middle" font-size="12" fill="#5b4c35" letter-spacing="2">GSM</text>
<text y="8" text-anchor="middle" font-size="15" fill="#2a2016" font-weight="bold" id="ctrTxt">ROTARY</text>
<text y="26" text-anchor="middle" font-size="10" fill="#5b4c35" id="ctrIp"></text>
<g id="stop"></g>
</svg>
<p class="hint">Drag a finger hole round to the stop — or just tap it</p>

<div class="btns">
<button class="big green" id="green" title="Call / Answer (Enter)">
<svg viewBox="0 0 24 24" fill="#fff"><path d="M6.6 10.8a15.1 15.1 0 0 0 6.6 6.6l2.2-2.2a1 1 0 0 1 1-.25 11.4 11.4 0 0 0 3.6.57 1 1 0 0 1 1 1V20a1 1 0 0 1-1 1A17 17 0 0 1 3 4a1 1 0 0 1 1-1h3.5a1 1 0 0 1 1 1c0 1.25.2 2.45.57 3.57a1 1 0 0 1-.25 1z"/></svg>
<span id="greenTxt">CALL</span></button>
<button class="big red" id="red" title="Hang up (Esc)">
<svg viewBox="0 0 24 24" fill="#fff"><path transform="rotate(135 12 12)" d="M6.6 10.8a15.1 15.1 0 0 0 6.6 6.6l2.2-2.2a1 1 0 0 1 1-.25 11.4 11.4 0 0 0 3.6.57 1 1 0 0 1 1 1V20a1 1 0 0 1-1 1A17 17 0 0 1 3 4a1 1 0 0 1 1-1h3.5a1 1 0 0 1 1 1c0 1.25.2 2.45.57 3.57a1 1 0 0 1-.25 1z"/></svg>
<span id="redTxt">CLEAR</span></button>
</div>
<div class="small"><button id="bs">⌫ Delete</button><button id="bell">🔔 Test bell</button></div>

<div class="log"><h2>Recent calls</h2><div id="log"><small>No calls yet</small></div></div>
</div>
<script>
const $=id=>document.getElementById(id), NS='http://www.w3.org/2000/svg';
const DIGITS=[1,2,3,4,5,6,7,8,9,0], R=112, HOLE=21, STOP=30;     // angles in degrees, clockwise from 3 o'clock
const holeAng=i=>-30-30*i, maxRot=i=>60+30*i;                     // rotation needed to reach the finger stop
const rad=a=>a*Math.PI/180, el=(t,a)=>{const e=document.createElementNS(NS,t);for(const k in a)e.setAttribute(k,a[k]);return e};

// ---- draw the dial ----
DIGITS.forEach((d,i)=>{
  const a=rad(holeAng(i)), x=R*Math.cos(a), y=R*Math.sin(a);
  const t=el('text',{x:x,y:y+9});t.textContent=d;$('numbers').appendChild(t);
  $('maskHoles').appendChild(el('circle',{cx:x,cy:y,r:HOLE,fill:'#000'}));
  $('hitHoles').appendChild(el('circle',{cx:x,cy:y,r:HOLE,fill:'transparent','stroke':'#000','stroke-width':1.5,class:'hole','data-i':i}));
});
(()=>{const a=rad(STOP+15),c=Math.cos(a),s=Math.sin(a);
 $('stop').appendChild(el('path',{d:`M${88*c} ${88*s} L${158*c} ${158*s}`,stroke:'url(#gStop)','stroke-width':9,'stroke-linecap':'round'}));
 $('stop').appendChild(el('path',{d:`M${88*c} ${88*s} L${158*c} ${158*s}`,stroke:'#555','stroke-width':1,'stroke-linecap':'round',opacity:.5}));})();

let rot=0, busy=false, drag=null;
const setRot=r=>{rot=r;$('plate').setAttribute('transform',`rotate(${r})`)};

// ---- little click sound while the dial returns ----
let ac=null;
function click(){try{ac=ac||new (window.AudioContext||window.webkitAudioContext)();
 const o=ac.createOscillator(),g=ac.createGain();o.type='square';o.frequency.value=1800;
 g.gain.setValueAtTime(.08,ac.currentTime);g.gain.exponentialRampToValueAtTime(.001,ac.currentTime+.025);
 o.connect(g).connect(ac.destination);o.start();o.stop(ac.currentTime+.03);}catch(e){}}

function animate(from,to,degPerSec,onStep){return new Promise(res=>{
 const t0=performance.now(),dur=Math.abs(to-from)/degPerSec*1000;
 function f(t){const k=Math.min(1,(t-t0)/Math.max(dur,1)),r=from+(to-from)*k;onStep&&onStep(r);setRot(r);
  k<1?requestAnimationFrame(f):res();}requestAnimationFrame(f);})}

async function returnDial(from,pulses){busy=true;let lastP=-1;
 await animate(from,0,300,r=>{const p=Math.floor((from-r)/30);if(pulses&&p!==lastP&&p<pulses){lastP=p;click()}});
 busy=false}

async function dialDigit(i){ if(busy)return; busy=true;
 await animate(0,maxRot(i),600); send(DIGITS[i]); await returnDial(maxRot(i),DIGITS[i]||10)}

function svgAngle(e){const s=$('dial'),p=s.createSVGPoint();p.x=e.clientX;p.y=e.clientY;
 const q=p.matrixTransform(s.getScreenCTM().inverse());return Math.atan2(q.y,q.x)*180/Math.PI}

$('hitHoles').addEventListener('pointerdown',e=>{
 if(busy||!e.target.dataset.i)return; e.preventDefault();
 const i=+e.target.dataset.i; drag={i,last:svgAngle(e),moved:0,t:performance.now()};
 $('dial').setPointerCapture(e.pointerId); busy=true;});
$('dial').addEventListener('pointermove',e=>{ if(!drag)return;
 const a=svgAngle(e); let d=a-drag.last; if(d>180)d-=360; if(d<-180)d+=360; drag.last=a; drag.moved+=Math.abs(d);
 setRot(Math.max(0,Math.min(maxRot(drag.i),rot+d)));});
function endDrag(){ if(!drag)return; const i=drag.i,m=drag.moved; drag=null; busy=false;
 if(rot>=maxRot(i)-8){ setRot(maxRot(i)); send(DIGITS[i]); returnDial(rot,DIGITS[i]||10);}
 else if(m<4){ setRot(0); dialDigit(i);} else returnDial(rot,0);}
$('dial').addEventListener('pointerup',endDrag); $('dial').addEventListener('pointercancel',endDrag);

// ---- talk to the ESP32 ----
let S={};
async function api(path){try{const r=await fetch(path,{method:'POST'});if(r.ok)render(await r.json())}catch(e){}}
const send=d=>api('/api/digit?d='+d);
$('green').onclick=()=>api('/api/call');
$('red').onclick=()=>api('/api/hangup');
$('bs').onclick=()=>api('/api/backspace');
$('bell').onclick=()=>api('/api/bell');
document.addEventListener('keydown',e=>{
 if(/^[0-9]$/.test(e.key)){dialDigit(DIGITS.indexOf(+e.key))}
 else if(e.key==='Enter')$('green').click(); else if(e.key==='Escape')$('red').click();
 else if(e.key==='Backspace')$('bs').click();});

const LABEL={idle:'Ready',offhook:'Handset lifted – dial',outgoing:'Calling…',connected:'Connected',incoming:'Incoming call',ended:'Call ended – replace handset'};
const fmt=s=>{s=Math.max(0,s|0);return Math.floor(s/60)+':'+String(s%60).padStart(2,'0')};
const ago=ms=>{const s=ms/1000|0;return s<60?s+'s ago':s<3600?(s/60|0)+'m ago':(s/3600|0)+'h ago'};

function render(s){ S=s;
 const st=$('state'); st.textContent=LABEL[s.state]||s.state; st.className='state'+(s.state==='incoming'?' ring':'');
 let num=s.dial, sub='';
 if(s.state==='incoming'){num=s.caller||'Unknown caller';sub=s.hook?'':'Lift the handset or press green to answer'}
 else if(s.state==='outgoing'){num=s.number;sub='Ringing out'}
 else if(s.state==='connected'){num=s.number||s.caller;sub='⏱ '+fmt((s.now-s.callStart)/1000)+(s.hook?'':' · lift the handset to talk')}
 else if(s.state==='idle'){sub=s.dial?'Press green to call':(s.hook?'':'Handset on hook')}
 $('num').textContent=num||''; $('sub').textContent=sub;
 const inc=s.state==='incoming', active=['outgoing','connected','incoming','ended'].includes(s.state);
 $('green').classList.toggle('pulse',inc); $('greenTxt').textContent=inc?'ANSWER':'CALL';
 $('redTxt').textContent=inc?'REJECT':active?'HANG UP':'CLEAR';
 const bars=s.csq===99?0:s.csq>=20?5:s.csq>=15?4:s.csq>=10?3:s.csq>=5?2:s.csq>=2?1:0;
 [...$('sig').children].forEach((b,i)=>b.classList.toggle('on',i<bars));
 $('op').textContent=(s.op||'')+(s.sim&&s.sim!=='registered'&&s.sim!=='ready'?' ('+s.sim+')':'') || '—';
 $('ctrIp').textContent=s.ip||'';
 const L=$('log'); L.innerHTML='';
 if(!s.log.length)L.innerHTML='<small>No calls yet</small>';
 s.log.forEach(e=>{const d=document.createElement('div');d.className='e'+(e.d==='m'?' m':'');
  const icon=e.d==='o'?'↗':e.d==='i'?'↙':'✕';
  d.innerHTML=`<span>${icon} <b></b></span><small>${ago(s.now-e.t)}</small>`;d.querySelector('b').textContent=e.n||'Unknown';
  if(e.n)d.onclick=()=>api('/api/number?n='+encodeURIComponent(e.n));L.appendChild(d)});
}
async function poll(){try{const r=await fetch('/api/status');if(r.ok)render(await r.json())}catch(e){$('state').textContent='offline'}
 setTimeout(poll,700)}
poll();
</script></body></html>)rawliteral";
