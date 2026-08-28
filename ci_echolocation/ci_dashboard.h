#pragma once
// root dashboard — mobile tabs, full radar, highlight filters

static const char DASHBOARD_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover"/>
<meta name="apple-mobile-web-app-capable" content="yes"/>
<title>root · Wi‑Fi · BLE · Sub‑GHz recon</title>
<style>
:root{
  --bg:#07080c;--bg2:#0c0e14;--panel:rgba(18,20,28,.92);--glass:rgba(255,255,255,.04);
  --ink:#f4f6fb;--muted:#8b93a7;--line:rgba(255,255,255,.08);
  --accent:#3ee8c5;--accent-dim:rgba(62,232,197,.22);--accent-glow:rgba(62,232,197,.12);
  --near:#f87171;--mid:#fbbf24;--far:#6b8f7a;--lora:#c084fc;--subghz:#fb923c;
  --radius:14px;--radius-sm:10px;--shadow:0 12px 40px rgba(0,0,0,.45);
  --font:Inter,ui-sans-serif,system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;
  --mono:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;
}
*,*::before,*::after{box-sizing:border-box;margin:0;padding:0;-webkit-tap-highlight-color:transparent}
html{font-size:16px;-webkit-text-size-adjust:100%}
body{
  font-family:var(--font);background:var(--bg);color:var(--ink);
  background-image:radial-gradient(ellipse 120% 80% at 50% -20%,rgba(62,232,197,.07),transparent 55%),
    radial-gradient(ellipse 60% 40% at 100% 100%,rgba(192,132,252,.05),transparent 50%);
  min-height:100dvh;min-height:100svh;display:grid;grid-template-rows:auto 1fr auto;
  height:100dvh;height:100svh;overflow:hidden;touch-action:manipulation;
  padding:env(safe-area-inset-top) env(safe-area-inset-right) env(safe-area-inset-bottom) env(safe-area-inset-left);
}

.topbar{
  grid-row:1;display:flex;align-items:center;gap:12px;padding:14px 16px 12px;
  background:linear-gradient(180deg,var(--panel),rgba(12,14,20,.88));
  border-bottom:1px solid var(--line);backdrop-filter:blur(16px);flex-shrink:0;flex-wrap:wrap;
}
.brand{display:flex;align-items:center;gap:10px;min-width:0}
.brand-mark{
  width:34px;height:34px;border-radius:11px;flex-shrink:0;
  background:linear-gradient(135deg,rgba(62,232,197,.25),rgba(62,232,197,.05));
  border:1px solid var(--accent-dim);display:grid;place-items:center;
  font-family:var(--mono);font-size:.72rem;font-weight:800;color:var(--accent);letter-spacing:-.04em;
}
.brand-text{min-width:0}
.brand-title{font-size:.95rem;font-weight:700;letter-spacing:-.02em;line-height:1.1}
.brand-tag{font-size:.68rem;color:var(--muted);letter-spacing:.02em}
.status-dot{width:8px;height:8px;border-radius:50%;background:var(--accent);flex-shrink:0;box-shadow:0 0 12px var(--accent-dim)}
.status-dot.off{background:var(--near);box-shadow:0 0 12px rgba(248,113,113,.4)}
.status-count{
  font-family:var(--mono);font-size:.82rem;font-weight:700;padding:7px 12px;border-radius:999px;
  background:var(--glass);border:1px solid var(--line);color:var(--accent);flex-shrink:0;
}
.band-pills{display:flex;gap:6px;flex-wrap:wrap;font-size:.68rem;font-family:var(--mono)}
.band-pill{padding:4px 9px;border-radius:999px;border:1px solid var(--line);color:var(--muted);background:var(--glass)}
.band-pill.wifi{color:var(--accent);border-color:var(--accent-dim)}
.band-pill.subghz{color:var(--subghz);border-color:rgba(251,146,60,.28)}
.band-pill.lora{color:var(--lora);border-color:rgba(192,132,252,.28)}
.toolbar-actions{display:flex;gap:6px;margin-left:auto;flex-wrap:wrap}
.btn{
  padding:9px 12px;min-height:40px;border:1px solid var(--line);border-radius:var(--radius-sm);
  background:var(--glass);color:var(--ink);font-size:.78rem;font-weight:600;cursor:pointer;
  font-family:var(--font);transition:background .15s,border-color .15s,transform .1s;
}
.btn:active{transform:scale(.98)}
.btn:hover{background:rgba(255,255,255,.07)}
.btn.primary{border-color:var(--accent-dim);color:var(--accent);background:var(--accent-glow)}
.btn.paused{border-color:rgba(251,191,36,.35);color:var(--mid)}
.btn.icon{padding:9px 10px;min-width:40px;display:inline-flex;align-items:center;justify-content:center}
.btn svg{width:15px;height:15px}

.shell{grid-row:2;min-height:0;overflow:hidden;display:flex;flex-direction:column}
@media(min-width:721px){
  .shell{display:grid;grid-template-columns:1.05fr .95fr;gap:10px;padding:10px}
  .mob-tabs{display:none!important}
}
.panel{display:none;flex-direction:column;min-height:0;overflow:hidden;background:var(--panel);border:1px solid var(--line);box-shadow:var(--shadow)}
.panel.on{display:flex}
@media(min-width:721px){.panel{display:flex!important;border-radius:var(--radius)}}

.panel-head{
  flex-shrink:0;padding:12px 14px;display:flex;justify-content:space-between;align-items:center;
  border-bottom:1px solid var(--line);background:rgba(255,255,255,.02);
}
.panel-title{font-size:.82rem;font-weight:700;letter-spacing:-.01em}
.panel-meta{font-size:.72rem;color:var(--muted);font-family:var(--mono)}

.radar-stage{flex:1;min-height:0;position:relative;background:radial-gradient(circle at center,#0a1018 0%,#07080c 70%);overflow:hidden}
@media(max-width:720px){#panelRadar.on .radar-stage{min-height:46vh}}
.radar-wrap{position:absolute;inset:0}
.radar-wrap canvas{display:block;width:100%;height:100%;touch-action:none;cursor:crosshair}
.radar-paused{
  position:absolute;inset:0;display:none;align-items:center;justify-content:center;
  background:rgba(7,8,12,.55);backdrop-filter:blur(4px);pointer-events:none;z-index:2;
  font-family:var(--mono);font-size:.72rem;letter-spacing:.18em;color:var(--muted);
}
.radar-paused.show{display:flex}
.radar-tip{
  position:absolute;left:50%;bottom:14px;transform:translateX(-50%);z-index:4;max-width:min(92%,420px);
  padding:12px 14px;border-radius:var(--radius-sm);background:rgba(12,16,24,.94);
  border:1px solid var(--accent-dim);color:var(--ink);font-size:.78rem;line-height:1.4;
  box-shadow:var(--shadow);display:none;pointer-events:none;
}
.radar-tip.show{display:block;pointer-events:auto}
.radar-tip b{display:block;font-size:.86rem;font-weight:700;margin-bottom:4px}
.radar-tip .sub{color:var(--muted);font-family:var(--mono);font-size:.7rem;word-break:break-all}
.radar-tip .tip-actions{display:flex;gap:6px;margin-top:10px}
.radar-hint{position:absolute;top:10px;right:12px;left:12px;z-index:3;font-size:.62rem;color:var(--muted);pointer-events:none;font-family:var(--mono);text-align:right}
.radar-disclaimer{
  position:absolute;left:10px;right:10px;bottom:10px;z-index:3;pointer-events:none;
  padding:8px 10px;border-radius:8px;background:rgba(8,10,14,.88);border:1px solid var(--line);
  font-size:.64rem;line-height:1.35;color:var(--muted);
}
.radar-disclaimer b{color:var(--accent);font-weight:600}

.list-toolbar{flex-shrink:0;padding:12px;border-bottom:1px solid var(--line);display:flex;flex-direction:column;gap:10px;background:rgba(255,255,255,.015)}
.search-wrap{position:relative}
.search{
  width:100%;padding:11px 14px 11px 38px;border-radius:var(--radius-sm);border:1px solid var(--line);
  background:rgba(0,0,0,.25);color:var(--ink);font-size:16px;font-family:var(--font);
}
.search-wrap::before{content:"⌕";position:absolute;left:13px;top:50%;transform:translateY(-50%);color:var(--muted);font-size:1rem;pointer-events:none}
.filters{display:flex;gap:6px;align-items:center;overflow-x:auto;-webkit-overflow-scrolling:touch;padding-bottom:2px}
.filters::-webkit-scrollbar{display:none}
.chip-hint{font-size:.7rem;color:var(--muted);flex-shrink:0}
.chip{
  flex-shrink:0;padding:8px 12px;min-height:36px;border-radius:999px;border:1px solid var(--line);
  background:var(--glass);color:var(--muted);font-size:.75rem;font-weight:600;
  display:inline-flex;align-items:center;cursor:pointer;transition:all .15s;
}
.chip.on{border-color:var(--accent-dim);background:var(--accent-glow);color:var(--accent)}

.list-head{
  flex-shrink:0;padding:8px 14px;font-size:.68rem;font-weight:600;color:var(--muted);
  border-bottom:1px solid var(--line);letter-spacing:.06em;text-transform:uppercase;font-family:var(--mono);
}
.list-scroll{flex:1;min-height:0;overflow:auto;-webkit-overflow-scrolling:touch;overscroll-behavior:contain;padding:10px}

.dev-cards{display:flex;flex-direction:column;gap:10px}
.dev-card{
  display:flex;align-items:stretch;gap:8px;padding:14px 14px 12px 16px;border-radius:var(--radius-sm);
  background:var(--glass);border:1px solid var(--line);transition:border-color .15s,background .15s;
  position:relative;overflow:hidden;cursor:pointer;flex-direction:column;
}
.dev-card>.card-actions{
  flex-direction:row;flex-wrap:wrap;gap:6px;margin-top:10px;padding-top:10px;border-top:1px solid var(--line);
}
.dev-summary{
  font-size:.74rem;color:var(--muted);line-height:1.45;margin:4px 0 10px;
  padding:8px 10px;border-radius:8px;background:rgba(0,0,0,.18);border:1px solid var(--line);
}
.dev-fields{display:flex;flex-direction:column;gap:8px}
.dev-field{padding:6px 0;border-bottom:1px solid rgba(255,255,255,.04)}
.dev-field:last-child{border-bottom:0;padding-bottom:0}
.dev-field-h{display:flex;justify-content:space-between;align-items:baseline;gap:10px;margin-bottom:3px}
.dev-field-label{
  font-size:.62rem;font-weight:700;letter-spacing:.06em;text-transform:uppercase;color:var(--accent);flex-shrink:0;
}
.dev-field-val{font-family:var(--mono);font-size:.72rem;color:var(--ink);text-align:right;word-break:break-all}
.dev-field-note{font-size:.68rem;color:var(--muted);line-height:1.4}
.dev-actions-note{
  width:100%;font-size:.68rem;color:var(--muted);line-height:1.4;margin-bottom:4px;
}
.card-actions .icon-btn,.card-actions .mini{width:auto;min-width:36px;height:34px}
.dev-card::before{
  content:"";position:absolute;left:0;top:0;bottom:0;width:3px;background:var(--accent);opacity:.5;
}
.dev-card.subghz::before{background:var(--subghz)}
.dev-card.lora::before{background:var(--lora)}
.dev-card.sel{border-color:var(--accent-dim);background:rgba(62,232,197,.06)}
.dev-card.dim{opacity:.38}
.dev-card.trust-bl{border-color:rgba(248,113,113,.25)}
.dev-card.trust-wl{border-color:rgba(107,163,255,.25)}
.dev-card-main{flex:1;min-width:0}
.dev-top{display:flex;justify-content:space-between;align-items:flex-start;gap:8px;margin-bottom:4px}
.dev-name{font-size:.88rem;font-weight:600;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.dev-dist{font-family:var(--mono);font-size:.78rem;color:var(--accent);flex-shrink:0}
.dev-mac{font-family:var(--mono);font-size:.68rem;color:var(--muted);margin-bottom:6px}
.dev-row{display:flex;flex-wrap:wrap;gap:6px;align-items:center}
.tag{
  font-size:.62rem;border:1px solid var(--line);border-radius:999px;padding:2px 7px;
  color:var(--muted);font-family:var(--mono);text-transform:lowercase;
}
.tag.wifi{color:var(--accent);border-color:var(--accent-dim)}
.tag.subghz{color:var(--subghz);border-color:rgba(251,146,60,.28)}
.tag.lora{color:var(--lora);border-color:rgba(192,132,252,.28)}
.tag.ssid{max-width:140px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.card-actions{display:flex;flex-direction:column;gap:4px;flex-shrink:0}
.icon-btn{
  width:36px;height:36px;border-radius:10px;border:1px solid var(--line);background:rgba(0,0,0,.2);
  color:var(--muted);cursor:pointer;display:grid;place-items:center;transition:all .15s;
}
.icon-btn:hover,.icon-btn:active{color:var(--accent);border-color:var(--accent-dim);background:var(--accent-glow)}
.icon-btn svg{width:15px;height:15px}
.mini{
  padding:4px 8px;border:1px solid var(--line);border-radius:8px;background:transparent;
  color:var(--muted);font-size:.62rem;font-weight:700;cursor:pointer;font-family:var(--mono);
}
.mini.wl{border-color:rgba(107,163,255,.35);color:#6ba3ff}
.mini.bl{border-color:rgba(248,113,113,.35);color:var(--near)}

.lists-box{flex-shrink:0;border-top:1px solid var(--line);padding:8px 0 0;font-size:.82rem}
.lists-box summary{cursor:pointer;color:var(--muted);font-weight:600;font-size:.75rem}
.lists-box summary::-webkit-details-marker{display:none}
.lists-add{display:flex;gap:6px;margin:8px 0;flex-wrap:wrap}
.list-in{
  flex:1;min-width:140px;padding:10px 12px;border-radius:var(--radius-sm);border:1px solid var(--line);
  background:rgba(0,0,0,.25);color:var(--ink);font-size:16px;font-family:var(--mono);
}
.list-btn{padding:10px 12px;min-height:40px;border:1px solid var(--line);border-radius:var(--radius-sm);background:var(--glass);font-size:.75rem;font-weight:600;cursor:pointer}
.list-btn.wl{border-color:rgba(107,163,255,.35);color:#6ba3ff}
.list-btn.bl{border-color:rgba(248,113,113,.35);color:var(--near)}
.lists-cols{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:6px}
.lists-cols h4{font-size:.65rem;color:var(--muted);letter-spacing:.06em;text-transform:uppercase;margin-bottom:4px}
.list-ul{list-style:none;max-height:88px;overflow-y:auto}
.list-ul li{display:flex;justify-content:space-between;gap:6px;padding:4px 0;border-bottom:1px solid var(--line);font-family:var(--mono);font-size:.7rem}
.list-ul .rm{color:var(--muted);cursor:pointer;padding:4px}

.empty{padding:40px 20px;text-align:center;color:var(--muted);font-size:.88rem;line-height:1.6}

.mob-tabs{grid-row:3;display:flex;background:var(--panel);border-top:1px solid var(--line);flex-shrink:0;padding:4px 8px calc(4px + env(safe-area-inset-bottom))}
.mob-tabs button{
  flex:1;border:0;background:transparent;color:var(--muted);font-size:.82rem;font-weight:600;
  padding:12px 8px;min-height:48px;cursor:pointer;border-radius:var(--radius-sm);transition:all .15s;
}
.mob-tabs button.on{color:var(--accent);background:var(--accent-glow)}

.desktop-foot{grid-row:3;display:none}
@media(min-width:721px){
  .desktop-foot{display:flex;padding:8px 14px;border-top:1px solid var(--line);justify-content:space-between;align-items:center;font-size:.72rem;color:var(--muted);background:rgba(0,0,0,.2)}
  body{grid-template-rows:auto 1fr auto}
  .mob-tabs{display:none!important}
  .desktop-foot{grid-row:3}
}
.desktop-foot summary{cursor:pointer;font-weight:600;list-style:none}
.desktop-foot summary::-webkit-details-marker{display:none}

.toast{
  position:fixed;left:50%;bottom:calc(72px + env(safe-area-inset-bottom));transform:translateX(-50%) translateY(12px);
  z-index:99;padding:10px 16px;border-radius:999px;background:rgba(12,16,24,.95);border:1px solid var(--accent-dim);
  color:var(--accent);font-size:.8rem;font-weight:600;box-shadow:var(--shadow);opacity:0;pointer-events:none;
  transition:opacity .2s,transform .2s;font-family:var(--mono);
}
.toast.show{opacity:1;transform:translateX(-50%) translateY(0)}
</style>
</head>
<body>

<header class="topbar">
  <div class="brand">
    <div class="brand-mark">RT</div>
    <div class="brand-text">
      <div class="brand-title">root</div>
      <div class="brand-tag">Wi‑Fi · BLE · Sub‑GHz recon</div>
      <div class="brand-sub" id="statusMsg">Scanning…</div>
    </div>
  </div>
  <div class="status-dot" id="dot"></div>
  <div class="band-pills" id="bandPills"></div>
  <div class="status-count" id="count">0/0</div>
  <div class="toolbar-actions">
    <button type="button" class="btn primary" id="btnCopyVisible" title="Copy visible devices">Copy</button>
    <button type="button" class="btn" id="btnCopyJson" title="Copy visible as JSON">JSON</button>
    <button type="button" class="btn" id="btnPause">Pause</button>
    <button type="button" class="btn" id="btnClear">Clear</button>
    <button type="button" class="btn icon" id="btnShot" title="Screenshot radar">
      <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M23 19a2 2 0 0 1-2 2H3a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h4l2-3h6l2 3h4a2 2 0 0 1 2 2z"/><circle cx="12" cy="13" r="4"/></svg>
    </button>
    <button type="button" class="btn icon" id="btnExport" title="Export session JSON">
      <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/></svg>
    </button>
  </div>
</header>

<div class="shell">
  <section class="panel on" id="panelRadar">
    <div class="panel-head">
      <div class="panel-title">Signal range map</div>
      <div class="panel-meta" id="radarMeta">listening</div>
    </div>
    <div class="radar-stage" id="radarStage">
      <div class="radar-paused" id="radarPaused">PAUSED</div>
      <div class="radar-hint">tap blip for details</div>
      <div class="radar-wrap"><canvas id="radar"></canvas></div>
      <div class="radar-disclaimer"><b>Real:</b> RSSI, distance estimate, band, SSID, packets. <b>Not measured:</b> direction — dots spread around each ring for readability only.</div>
      <div class="radar-tip" id="radarTip"></div>
    </div>
  </section>

  <section class="panel" id="panelList">
    <div class="list-toolbar">
      <div class="search-wrap">
        <input class="search" id="search" type="search" enterkeyhint="search" placeholder="Search MAC, SSID, band…" autocomplete="off"/>
      </div>
      <div class="filters" id="filters">
        <span class="chip-hint">Highlight</span>
        <span class="chip" data-f="subghz">Sub-GHz</span>
        <span class="chip" data-f="fixed">Fixed RF</span>
        <span class="chip" data-f="lora">LoRa</span>
        <span class="chip" data-f="probe">Phones</span>
        <span class="chip" data-f="beacon">Wi‑Fi</span>
        <span class="chip" data-f="Near">Close</span>
        <span class="chip" data-f="Mid">Near</span>
        <span class="chip" data-f="Far">Far</span>
        <span class="chip" data-f="fresh">Recent</span>
        <span class="chip" data-f="whitelist">White</span>
        <span class="chip" data-f="blacklist">Black</span>
        <span class="chip" data-f="clear">Clear</span>
      </div>
      <details class="lists-box">
        <summary>whitelist / blacklist</summary>
        <div class="lists-add">
          <input class="list-in" id="listEntry" type="text" placeholder="MAC or IP" autocapitalize="characters" autocomplete="off"/>
          <button type="button" class="list-btn wl" id="btnAddWl">+ White</button>
          <button type="button" class="list-btn bl" id="btnAddBl">+ Black</button>
        </div>
        <div class="lists-cols">
          <div><h4>Whitelist</h4><ul class="list-ul" id="wlList"></ul></div>
          <div><h4>Blacklist</h4><ul class="list-ul" id="blList"></ul></div>
        </div>
      </details>
    </div>
    <div class="list-head" id="listHead">passive rf · probe / beacon / data / lora</div>
    <div class="list-scroll" id="list">
      <div class="empty" id="empty">No devices yet — listening on Wi‑Fi, sub‑GHz, and LoRa…</div>
      <div class="dev-cards" id="devCards"></div>
    </div>
  </section>
</div>

<div class="toast" id="toast"></div>

<nav class="mob-tabs" id="mobTabs">
  <button type="button" class="on" data-tab="radar">Radar</button>
  <button type="button" data-tab="list">Devices</button>
</nav>

<footer class="desktop-foot">
  <details><summary>Advanced</summary>
    Ch <b id="ch">—</b> · <b id="hop">—</b> · up <b id="up">—</b>
    <select id="chSel"><option value="auto">Auto</option></select>
  </details>
  <span>root · local only (no internet) · 315/433/868/915 MHz</span>
</footer>

<script>
const canvas=document.getElementById("radar"),ctx=canvas.getContext("2d");
const radarStage=document.getElementById("radarStage"),chSel=document.getElementById("chSel");
const radarTip=document.getElementById("radarTip");
let allDevices=[],searchQ="",activeFilters=new Set();
const deviceStore=new Map();
const CLIENT_HOLD_MS=600000;
const LIVE_MS=60000;
let rfMeta={subghz:null,lora:null};
let radarW=300,radarH=300,dpr=1,paused=false,frozenSweep=0,pollTimer=null;
let scanMeta={channel:null,hopping:null,uptime_ms:0,session_ms:0,name:"root"};
let whitelist=[],blacklist=[];
let selectedMac=null,blipAnim={},hitList=[];

const LS_WL="root_whitelist",LS_BL="root_blacklist";
function loadLists(){
  try{whitelist=JSON.parse(localStorage.getItem(LS_WL)||"[]");}catch(e){whitelist=[];}
  try{blacklist=JSON.parse(localStorage.getItem(LS_BL)||"[]");}catch(e){blacklist=[];}
}
function saveLists(){
  localStorage.setItem(LS_WL,JSON.stringify(whitelist));
  localStorage.setItem(LS_BL,JSON.stringify(blacklist));
  renderListEntries();
}
function normHex(s){return (s||"").replace(/[^0-9a-fA-F]/g,"").toUpperCase();}
function normMac(s){
  const h=normHex(s);
  if(h.length===12) return h.match(/.{2}/g).join(":");
  if(h.length>=6&&h.length<=12) return h;
  return null;
}
function normIp(s){
  const t=(s||"").trim();
  if(/^\d{1,3}(\.\d{1,3}){3}$/.test(t)) return t;
  return null;
}
function parseEntry(raw){
  const t=(raw||"").trim(); if(!t) return null;
  const ip=normIp(t); if(ip) return {type:"ip",id:ip};
  const mac=normMac(t); if(mac) return {type:"mac",id:mac.length===17?mac:mac};
  return null;
}
function macMatch(devMac,entry){
  const dm=normHex(devMac),em=normHex(entry);
  if(!dm||!em) return false;
  return dm===em||dm.startsWith(em)||em.startsWith(dm);
}
function deviceTrust(d){
  const mac=d.mac||"";
  for(const e of blacklist) if(e.type==="mac"&&macMatch(mac,e.id)) return "blacklist";
  for(const e of whitelist) if(e.type==="mac"&&macMatch(mac,e.id)) return "whitelist";
  return "none";
}
function addToList(kind,raw){
  const e=parseEntry(raw); if(!e) return false;
  const list=kind==="whitelist"?whitelist:blacklist;
  const other=kind==="whitelist"?blacklist:whitelist;
  if(list.some(x=>x.type===e.type&&x.id===e.id)) return true;
  const oi=other.findIndex(x=>x.type===e.type&&x.id===e.id);
  if(oi>=0) other.splice(oi,1);
  list.push(e); saveLists(); renderList(); return true;
}
function removeFromList(kind,idx){
  const list=kind==="whitelist"?whitelist:blacklist;
  if(idx>=0&&idx<list.length){list.splice(idx,1);saveLists();renderList();}
}
function renderListEntries(){
  const wl=document.getElementById("wlList"),bl=document.getElementById("blList");
  const row=(e,i,k)=>"<li><span><span class='ty'>"+e.type+"</span> "+e.id+"</span><span class='rm' data-rm='"+k+"' data-i='"+i+"'>✕</span></li>";
  wl.innerHTML=whitelist.length?whitelist.map((e,i)=>row(e,i,"whitelist")).join(""):"<li style='color:var(--muted)'>empty</li>";
  bl.innerHTML=blacklist.length?blacklist.map((e,i)=>row(e,i,"blacklist")).join(""):"<li style='color:var(--muted)'>empty</li>";
}
loadLists(); renderListEntries();
const mob=()=>window.innerWidth<=720;
for(let i=1;i<=13;i++){const o=document.createElement("option");o.value=i;o.textContent=i;chSel.appendChild(o);}

const BANDS=new Set(["subghz","lora","wifi"]);
const FIXED_RF=/fixed emitter/i;
const KINDS=new Set(["probe","beacon"]);
const ZONES=new Set(["Near","Mid","Far"]);
const TRUST_FILTERS=new Set(["whitelist","blacklist"]);

const COPY_SVG='<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="9" y="9" width="13" height="13" rx="2"/><path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1"/></svg>';

function deviceCopyText(d){
  if(!d) return "";
  const lines=[
    "root device",
    "mac: "+(d.mac||""),
    "name: "+plainName(d),
    d.ssid?"ssid: "+d.ssid:"",
    "band: "+(d.band||"wifi"),
    "kind: "+(d.kind||""),
    d.ch?"channel: "+d.ch:"",
    "rssi: "+Number(d.avg||d.rssi).toFixed(1)+" dBm",
    "distance: ~"+Number(d.distance_m||0).toFixed(1)+" m",
    "zone: "+(d.zone||""),
    "seen: "+(d.seen_count||1)+"x",
    "last: "+fmtAge(d.last_seen_ms||0),
    "coords: "+fmtCoords(d)
  ];
  if(d.lat&&d.lon) lines.push("lat: "+d.lat,"lon: "+d.lon);
  return lines.filter(Boolean).join("\n");
}
function deviceCopyJson(d){
  const o={...d}; delete o._feedMs; return JSON.stringify(o,null,2);
}
function visibleDevices(){return filtOn()?allDevices.filter(matches):allDevices;}
let toastTimer=null;
function toast(msg){
  const el=document.getElementById("toast");
  if(!el) return;
  el.textContent=msg||"Copied";
  el.classList.add("show");
  if(toastTimer) clearTimeout(toastTimer);
  toastTimer=setTimeout(()=>el.classList.remove("show"),1800);
}
async function copyText(text,label){
  if(!text) return toast("Nothing to copy");
  try{
    if(navigator.clipboard&&navigator.clipboard.writeText) await navigator.clipboard.writeText(text);
    else throw new Error("clip");
  }catch(e){
    const ta=document.createElement("textarea");
    ta.value=text;ta.style.position="fixed";ta.style.left="-9999px";
    document.body.appendChild(ta);ta.focus();ta.select();
    try{document.execCommand("copy");}catch(err){}
    ta.remove();
  }
  toast(label||"Copied");
}
function copyDevice(d){copyText(deviceCopyText(d),"Device copied");}
function copyDeviceJson(d){copyText(deviceCopyJson(d),"JSON copied");}
function copyVisibleText(){
  const rows=visibleDevices();
  copyText(rows.map(deviceCopyText).join("\n\n———\n\n"),rows.length+" device"+(rows.length===1?"":"s")+" copied");
}
function copyVisibleJson(){
  const rows=visibleDevices().map(d=>{const o={...d};delete o._feedMs;return o;});
  copyText(JSON.stringify(rows,null,2),"JSON copied");
}

function kindExplain(k){
  const m={
    probe:"Scanned for Wi-Fi networks — phone, laptop, tablet, or IoT waking up.",
    beacon:"Broadcasts a network name — router, extender, or hotspot.",
    data:"Active Wi-Fi traffic right now — device is talking on the air.",
    deauth:"Deauth / disassoc frame — roam, reset, or possible attack.",
    subghz:"Sub-GHz RF detection — remote, sensor, TPMS, or fixed emitter.",
    lora:"915 MHz LoRa payload — sensor, Meshtastic, or industrial link.",
    mgmt:"Wi-Fi management frame — join, leave, or control traffic."
  };
  return m[k]||"Wireless emission classified on this band.";
}
function bandExplain(b){
  const m={
    wifi:"2.4 GHz Wi-Fi (passive promiscuous). Listen-only — no transmit.",
    subghz:"CC1101 scanner: 315 / 433 / 868 / 915 MHz bursts and carriers.",
    lora:"E22 LoRa UART at 915 MHz. Needs real LoRa traffic nearby."
  };
  return m[b]||"RF band this hit was sorted into.";
}
function zoneExplain(z,m){
  const n=Math.round(Number(m)||0);
  if(z==="Near"||n<5) return "Very close — strong signal, likely same room.";
  if(z==="Mid"||n<15) return "Nearby — same floor or adjacent space (estimate).";
  return "Far — weak signal, could be outside or through walls.";
}
function deviceWhatIs(d){
  const band=d.band||"wifi";
  if(band==="lora") return "LoRa transmitter or sensor on 915 MHz.";
  if(band==="subghz") return (d.ssid&&d.ssid.trim())?d.ssid.trim():"Sub-GHz emitter — remote, sensor, or fixed RF.";
  if(d.ssid&&d.ssid.trim()) return "Wi-Fi device associated with «"+d.ssid.trim()+"».";
  if(d.kind==="probe") return "Device probing for networks — often a phone or laptop.";
  if(d.kind==="beacon") return "Wi-Fi access point or mesh node advertising a network.";
  return "Wireless device heard on the passive scanner.";
}
function fieldRow(label,value,note){
  return '<div class="dev-field"><div class="dev-field-h"><span class="dev-field-label">'+label+'</span><span class="dev-field-val">'+value+'</span></div><div class="dev-field-note">'+note+'</div></div>';
}
function renderDeviceFields(d){
  const band=d.band||"wifi",kind=d.kind||"?",z=d.zone||"Far";
  const rssi=Number(d.avg!=null?d.avg:d.rssi).toFixed(1);
  const dist=Number(d.distance_m||0).toFixed(1);
  let html="";
  html+=fieldRow("MAC",d.mac||"—","Unique hardware address. Copy → vendor lookup (OUI), whitelist/blacklist, or paste into reports.");
  if(d.rand) html+=fieldRow("Privacy","Randomized MAC","Locally administered address — real vendor may be hidden. Track by pattern + timing.");
  html+=fieldRow("Band",band,bandExplain(band));
  html+=fieldRow("Type",kind,kindExplain(kind));
  if(d.ssid&&d.ssid.trim()) html+=fieldRow("SSID / label",d.ssid.replace(/</g,"&lt;"),band==="wifi"?"Network name chosen by owner — OSINT, wardriving DBs, or location context.":"Human-readable label from the RF decoder.");
  if(d.ch) html+=fieldRow("Channel","ch"+d.ch,"Wi-Fi channel in 2.4 GHz. Same channel = shares airtime with your radio.");
  html+=fieldRow("RSSI",rssi+" dBm","Received signal strength. Higher (less negative) = usually closer. Walls and metal skew this.");
  html+=fieldRow("Distance","~"+dist+" m (estimated)","From RSSI path-loss model — not laser/GPS range. Walls and body fade change this.");
  html+=fieldRow("Zone",z,zoneExplain(z,d.distance_m));
  html+=fieldRow("Seen",(d.seen_count||1)+"×","Times heard this session. Rising count = persistent or repeating emitter.");
  html+=fieldRow("Last heard",fmtAge(d.last_seen_ms||0),"Time since last packet. Seconds = actively on the air right now.");
  if((d.gps===true||d.gps==="true")&&d.lat&&d.lon){
    html+=fieldRow("GPS",Number(d.lat).toFixed(5)+", "+Number(d.lon).toFixed(5),"Measured position when GPS module is attached and has a fix.");
  }
  return html;
}
function renderDeviceCard(d,opts){
  opts=opts||{};
  const trust=deviceTrust(d);
  const tcl=trust==="blacklist"?" trust-bl":trust==="whitelist"?" trust-wl":"";
  const sel=d.mac===selectedMac?" sel":"";
  const dim=opts.dim?" dim":"";
  const bandCls=d.band==="lora"?" lora":d.band==="subghz"?" subghz":"";
  const macEsc=(d.mac||"").replace(/"/g,"&quot;");
  return '<div class="dev-card'+dim+tcl+sel+bandCls+'" data-mac="'+macEsc+'">'+
    '<div class="dev-card-main">'+
    '<div class="dev-top"><div class="dev-name">'+plainName(d).replace(/</g,"&lt;")+'</div>'+
    '<div class="dev-dist">~'+Number(d.distance_m||0).toFixed(1)+'m</div></div>'+
    '<div class="dev-summary">'+deviceWhatIs(d).replace(/</g,"&lt;")+'</div>'+
    '<div class="dev-fields">'+renderDeviceFields(d)+'</div></div>'+
    '<div class="card-actions">'+
    '<div class="dev-actions-note"><b>Actions:</b> Copy = paste into notes/reports · JSON = automation · W = whitelist (highlight) · B = blacklist (flag)</div>'+
    '<button type="button" class="icon-btn" data-copy="'+macEsc+'" title="Copy device report">'+COPY_SVG+'</button>'+
    '<button type="button" class="icon-btn" data-copy-json="'+macEsc+'" title="Copy JSON">{}</button>'+
    '<button type="button" class="mini wl" data-add-wl="'+macEsc+'" title="Add to whitelist">W</button>'+
    '<button type="button" class="mini bl" data-add-bl="'+macEsc+'" title="Add to blacklist">B</button>'+
    '</div></div>';
}

function plainName(d){
  if(d.ssid&&d.ssid.trim()) return d.ssid.trim();
  if(d.band==="subghz"){
    if(d.ssid&&d.ssid.includes("fixed emitter")) return d.ssid.trim();
    if(d.ssid&&d.ssid.trim()) return d.ssid.trim();
    return "Sub-GHz signal";
  }
  if(d.band==="lora") return d.ssid&&d.ssid.trim()?d.ssid.trim():"915 MHz LoRa";
  if(d.kind==="beacon") return "Wi‑Fi network";
  if(d.kind==="probe") return "Phone / tablet";
  return "Wireless device";
}
function plainShort(m,z){
  const n=Math.round(m);
  if(z==="Near"||m<5) return n+"m close";
  if(z==="Mid"||m<15) return n+"m near";
  return n+"m far";
}
function plainSeen(ms){const s=Math.round(ms/1000);if(s<5)return"now";if(s<60)return s+"s";return Math.floor(s/60)+"m";}
function fmtCoords(d){
  if((d.gps===true||d.gps==="true")&&d.lat&&d.lon) return Number(d.lat).toFixed(5)+","+Number(d.lon).toFixed(5);
  return "—";
}
function sightCoords(s){
  if((s.gps===true||s.gps==="true")&&s.lat&&s.lon) return Number(s.lat).toFixed(5)+","+Number(s.lon).toFixed(5);
  if(s.distance_m!=null) return "~"+Number(s.distance_m).toFixed(1)+"m";
  return "—";
}
function zoneRank(z){return z==="Near"?0:z==="Mid"?1:2;}
function zoneColor(z){return z==="Near"?"var(--near)":z==="Mid"?"var(--mid)":"var(--far)";}
function bandColor(d){
  if(d.band==="lora") return "#c084fc";
  if(d.band==="subghz") return "#fb923c";
  return zoneColor(d.zone||"Far");
}
function mergeDevices(incoming){
  const now=Date.now();
  const seen=new Set();
  for(const d of incoming){
    if(!d.mac) continue;
    seen.add(d.mac);
    const prev=deviceStore.get(d.mac);
    deviceStore.set(d.mac,{...(prev||{}),...d,_feedMs:now});
  }
  for(const [mac,d] of deviceStore){
    if(!seen.has(mac)&&(now-(d._feedMs||0))>CLIENT_HOLD_MS) deviceStore.delete(mac);
  }
  allDevices=[...deviceStore.values()];
}
function liveCount(){
  return allDevices.filter(d=>(d.last_seen_ms||999999)<LIVE_MS).length;
}
function updateCountBadge(){
  const el=document.getElementById("count");
  if(!el) return;
  const total=allDevices.length,live=liveCount();
  el.textContent=live+"/"+total;
  el.title=live+" live (60s) · "+total+" session";
}
function clearDevices(){
  deviceStore.clear();
  allDevices=[];
  selectedMac=null;
  blipAnim={};
  showTip(null);
  updateCountBadge();
  renderList();
}
function bandCounts(){
  let wifi=0,subghz=0,lora=0;
  for(const d of allDevices){
    if(d.band==="subghz") subghz++;
    else if(d.band==="lora") lora++;
    else wifi++;
  }
  return {wifi,subghz,lora};
}
function renderBandPills(){
  const el=document.getElementById("bandPills");
  if(!el) return;
  const c=bandCounts();
  el.innerHTML=
    '<span class="band-pill wifi">'+c.wifi+' Wi‑Fi</span>'+
    '<span class="band-pill subghz">'+c.subghz+' Sub‑GHz</span>'+
    '<span class="band-pill lora">'+c.lora+' LoRa</span>';
}
function fmtAge(ms){
  if(ms<1000) return ms+"ms";
  const s=ms/1000;
  if(s<60) return s.toFixed(1)+"s";
  return Math.floor(s/60)+"m";
}
function fmtUp(ms){const s=Math.floor(ms/1000),m=Math.floor(s/60);return m?m+"m":s+"s";}
function hay(d){return [plainName(d),d.ssid||"",d.mac||"",d.kind||"",d.band||"",d.zone||""].join(" ").toLowerCase();}
function dkind(d){return d.band||((d.kind==="probe"||d.kind==="data")?"probe":d.kind==="beacon"?"beacon":d.kind||"");}
function filtOn(){return activeFilters.size>0||!!searchQ.trim();}
function matches(d){
  const q=searchQ.trim().toLowerCase();
  if(q&&!hay(d).includes(q)) return false;
  if(!activeFilters.size) return true;
  const bands=[...activeFilters].filter(f=>BANDS.has(f));
  const ks=[...activeFilters].filter(f=>KINDS.has(f));
  const zs=[...activeFilters].filter(f=>ZONES.has(f));
  if(bands.length&&!bands.includes(d.band||"wifi")) return false;
  if(ks.length&&!ks.includes(dkind(d))) return false;
  if(zs.length&&!zs.includes(d.zone)) return false;
  if(activeFilters.has("fresh")&&(d.last_seen_ms||99999)>=30000) return false;
  if(activeFilters.has("fixed")&&!FIXED_RF.test(d.ssid||"")) return false;
  if(activeFilters.has("whitelist")&&deviceTrust(d)!=="whitelist") return false;
  if(activeFilters.has("blacklist")&&deviceTrust(d)!=="blacklist") return false;
  return true;
}
function sorted(a){return [...a].sort((x,y)=>zoneRank(x.zone)-zoneRank(y.zone)||(x.distance_m||99)-(y.distance_m||99));}

function downloadBlob(blob,name){
  const a=document.createElement("a");
  a.href=URL.createObjectURL(blob);a.download=name;a.rel="noopener";
  document.body.appendChild(a);a.click();a.remove();
  setTimeout(()=>URL.revokeObjectURL(a.href),3000);
}
function exportScanJson(){
  const payload={
    exported_at:new Date().toISOString(),
    name:scanMeta.name||"root",
    paused,
    channel:scanMeta.channel,
    hopping:scanMeta.hopping,
    uptime_ms:scanMeta.uptime_ms,
    session_ms:scanMeta.session_ms,
    count:allDevices.length,
    filters:{search:searchQ,active:[...activeFilters]},
    whitelist,blacklist,
    devices:allDevices
  };
  const ts=new Date().toISOString().replace(/[:.]/g,"-");
  downloadBlob(new Blob([JSON.stringify(payload,null,2)],{type:"application/json"}),"root-scan-"+ts+".json");
}
function screenshotRadar(){
  try{
    const url=canvas.toDataURL("image/png");
    const ts=new Date().toISOString().replace(/[:.]/g,"-");
    const a=document.createElement("a");
    a.href=url;a.download="root-radar-"+ts+".png";a.rel="noopener";
    document.body.appendChild(a);a.click();a.remove();
  }catch(e){alert("Screenshot failed — try again on Radar tab");}
}
function statusLine(){
  const live=liveCount(), total=scanMeta.total??allDevices.length;
  const c=bandCounts();
  const parts=[];
  if(c.wifi) parts.push(c.wifi+" Wi‑Fi");
  if(c.subghz) parts.push(c.subghz+" Sub‑GHz");
  else if(rfMeta.subghz&&rfMeta.subghz.ready) parts.push("Sub‑GHz listening");
  if(c.lora) parts.push(c.lora+" LoRa");
  else if(rfMeta.lora&&rfMeta.lora.ready) parts.push("LoRa listening");
  const mix=parts.length?parts.join(" · "):(total?live+"/"+total+" live":"0");
  if(paused) return "Paused · "+mix;
  return total?mix:"Scanning…";
}
function setPaused(on){
  paused=on;
  const btn=document.getElementById("btnPause");
  btn.textContent=on?"Resume":"Pause";
  btn.classList.toggle("paused",on);
  document.getElementById("radarPaused").classList.toggle("show",on);
  document.getElementById("statusMsg").textContent=statusLine();
  if(on){
    frozenSweep=((performance.now()/1000)%3)/3;
    if(pollTimer){clearInterval(pollTimer);pollTimer=null;}
  }else if(!pollTimer){
    poll();
    pollTimer=setInterval(poll,1500);
  }
}
function togglePause(){setPaused(!paused);}

document.getElementById("btnPause").onclick=togglePause;
document.getElementById("btnClear").onclick=()=>{if(confirm("Clear session device list?"))clearDevices();};
document.getElementById("btnCopyVisible").onclick=copyVisibleText;
document.getElementById("btnCopyJson").onclick=copyVisibleJson;
document.getElementById("btnShot").onclick=screenshotRadar;
document.getElementById("btnExport").onclick=exportScanJson;
function fitRadar(){
  const panel=document.getElementById("panelRadar");
  if(!radarStage||!panel.classList.contains("on")) return;
  const rect=radarStage.getBoundingClientRect();
  if(rect.width<40||rect.height<40) return;
  dpr=Math.min(window.devicePixelRatio||1,2);
  radarW=Math.floor(rect.width);
  radarH=Math.floor(rect.height);
  canvas.width=Math.floor(radarW*dpr);
  canvas.height=Math.floor(radarH*dpr);
  canvas.style.width=radarW+"px";
  canvas.style.height=radarH+"px";
  ctx.setTransform(dpr,0,0,dpr,0,0);
}

function setTab(tab){
  if(!mob()){document.getElementById("panelRadar").classList.add("on");document.getElementById("panelList").classList.add("on");setTimeout(fitRadar,50);return;}
  document.querySelectorAll(".mob-tabs button").forEach(b=>b.classList.toggle("on",b.dataset.tab===tab));
  document.getElementById("panelRadar").classList.toggle("on",tab==="radar");
  document.getElementById("panelList").classList.toggle("on",tab==="list");
  if(tab==="radar"){setTimeout(fitRadar,50);setTimeout(fitRadar,200);setTimeout(fitRadar,600);}
}

document.getElementById("mobTabs").onclick=e=>{const b=e.target.closest("button");if(b)setTab(b.dataset.tab);};
window.addEventListener("resize",fitRadar);
window.addEventListener("orientationchange",()=>setTimeout(fitRadar,400));
if(radarStage&&window.ResizeObserver) new ResizeObserver(()=>{if(document.getElementById("panelRadar").classList.contains("on"))fitRadar();}).observe(radarStage);
setTimeout(fitRadar,100);setTimeout(fitRadar,500);setTimeout(fitRadar,1200);

function macHash(mac){
  let h=2166136261;
  for(let i=0;i<(mac||"").length;i++){h^=mac.charCodeAt(i);h=Math.imul(h,16777619);}
  return h>>>0;
}
function radarLayout(devices,maxM){
  const buckets=[[],[],[],[]];
  for(const d of devices){
    const mac=d.mac||"";
    if(!mac) continue;
    const m=Number(d.distance_m!=null?d.distance_m:rssiFallback(d));
    let b=3;
    if(m<maxM*0.25) b=0;
    else if(m<maxM*0.5) b=1;
    else if(m<maxM*0.75) b=2;
    buckets[b].push(d);
  }
  const layout=new Map();
  for(let bi=0;bi<buckets.length;bi++){
    const bucket=buckets[bi].sort((a,b)=>(a.mac||"").localeCompare(b.mac||""));
    const n=bucket.length;
    for(let i=0;i<n;i++){
      const ang=n<=1?-Math.PI/2:((i/n)*Math.PI*2-Math.PI/2);
      layout.set(bucket[i].mac,{ang,bi});
    }
  }
  return layout;
}
function maxRingMeters(){
  let far=12;
  for(const d of allDevices){
    const m=Number(d.distance_m);
    if(Number.isFinite(m)&&m>far) far=m;
  }
  return Math.min(Math.max(far*1.15,12),80);
}
function distR(meters,maxR,maxM){
  const t=Math.min(Math.max((Number(meters)||40)/Math.max(maxM,1),0.02),1);
  return 14+t*(maxR-18);
}
function lerp(a,b,t){return a+(b-a)*t;}
function angLerp(a,b,t){
  let d=b-a;
  while(d>Math.PI) d-=Math.PI*2;
  while(d<-Math.PI) d+=Math.PI*2;
  return a+d*t;
}
function showTip(d){
  if(!d){radarTip.classList.remove("show");radarTip.innerHTML="";return;}
  const trust=deviceTrust(d);
  const tag=trust==="blacklist"?" · flagged":trust==="whitelist"?" · trusted":"";
  const macEsc=(d.mac||"").replace(/"/g,"&quot;");
  radarTip.innerHTML="<b>"+plainName(d).replace(/</g,"&lt;")+"</b>"+
    "<div class='sub'>"+deviceWhatIs(d).replace(/</g,"&lt;")+tag+"</div>"+
    "<div class='sub' style='margin-top:6px'>"+
    "MAC "+(d.mac||"")+" · "+Number(d.avg||d.rssi).toFixed(0)+" dBm · ~"+Number(d.distance_m||0).toFixed(1)+"m · "+(d.zone||"?")+
    "</div>"+
    "<div class='sub' style='margin-top:4px;color:var(--muted)'>"+
    bandExplain(d.band||"wifi")+" · "+kindExplain(d.kind||"")+
    " · distance is RSSI estimate only"+
    "</div>"+
    "<div class='tip-actions'>"+
    "<button type='button' class='btn primary' data-tip-copy='"+macEsc+"'>Copy</button>"+
    "<button type='button' class='btn' data-tip-json='"+macEsc+"'>JSON</button>"+
    "</div>";
  radarTip.classList.add("show");
}
function selectMac(mac){
  selectedMac=mac||null;
  const d=allDevices.find(x=>x.mac===selectedMac);
  showTip(d||null);
  renderList();
  if(selectedMac){
    fetchTimeout("/api/sightings?mac="+encodeURIComponent(selectedMac),6000)
      .then(r=>r.ok?r.json():null)
      .then(j=>{
        if(!j||!j.sightings||!selectedMac) return;
        const i=allDevices.findIndex(x=>x.mac===selectedMac);
        if(i<0) return;
        allDevices[i]={...allDevices[i],sightings:j.sightings};
        if(selectedMac===allDevices[i].mac){showTip(allDevices[i]);renderList();}
      }).catch(()=>{});
  }
}
function canvasPoint(ev){
  const rect=canvas.getBoundingClientRect();
  const t=ev.touches&&ev.touches[0]?ev.touches[0]:ev.changedTouches&&ev.changedTouches[0]?ev.changedTouches[0]:ev;
  return {x:(t.clientX-rect.left)*(radarW/Math.max(rect.width,1)),y:(t.clientY-rect.top)*(radarH/Math.max(rect.height,1))};
}
function hitTest(pt){
  let best=null,bestD=22;
  for(const h of hitList){
    const dx=pt.x-h.x,dy=pt.y-h.y,dd=Math.hypot(dx,dy);
    if(dd<bestD){bestD=dd;best=h.mac;}
  }
  return best;
}
function onRadarPointerDown(ev){
  try{canvas.setPointerCapture(ev.pointerId);}catch(e){}
  const pt=canvasPoint(ev);
  const mac=hitTest(pt);
  if(mac) selectMac(mac===selectedMac?null:mac);
  else selectMac(null);
  if(ev.cancelable) ev.preventDefault();
}
function onRadarPointerUp(){}
canvas.addEventListener("pointerdown",onRadarPointerDown,{passive:false});
canvas.addEventListener("pointerup",onRadarPointerUp);
canvas.addEventListener("pointercancel",onRadarPointerUp);
canvas.addEventListener("lostpointercapture",onRadarPointerUp);

function paint(){
  try{
    const w=radarW,h=radarH;
    if(w<40||h<40){requestAnimationFrame(paint);return;}
    const cx=w/2,cy=h/2,maxR=Math.max(36,Math.min(cx,cy)-16);
    const maxM=maxRingMeters();
    const now=performance.now();
    // FinchMobile-style continuous sweep (~3s/rev)
    const sweepT=paused?frozenSweep:((now/1000)%3)/3;
    if(!paused) frozenSweep=sweepT;
    const sweepAng=sweepT*Math.PI*2;

    ctx.fillStyle="#0d0d0d";
    ctx.fillRect(0,0,w,h);

    // Range rings + meter labels (like FinchMobile)
    ctx.strokeStyle="rgba(62,232,197,0.22)";ctx.lineWidth=1;
    for(let i=1;i<=4;i++){
      const rr=maxR*(i/4);
      ctx.beginPath();ctx.arc(cx,cy,rr,0,Math.PI*2);ctx.stroke();
      const meters=Math.round(maxM*(i/4));
      ctx.fillStyle="rgba(255,255,255,0.55)";
      ctx.font="10px ui-monospace,Menlo,Consolas,monospace";
      ctx.fillText(meters+"m",cx+4,cy-rr+11);
    }
    ctx.strokeStyle="rgba(62,232,197,0.12)";
    ctx.beginPath();ctx.moveTo(cx-maxR,cy);ctx.lineTo(cx+maxR,cy);
    ctx.moveTo(cx,cy-maxR);ctx.lineTo(cx,cy+maxR);ctx.stroke();

    // Sweep wedge (arc path — no createConicGradient; works everywhere)
    const wedge=0.35;
    ctx.beginPath();
    ctx.moveTo(cx,cy);
    ctx.arc(cx,cy,maxR,sweepAng-wedge,sweepAng,false);
    ctx.closePath();
    ctx.fillStyle="rgba(62,232,197,0.10)";
    ctx.fill();
    const lx=cx+Math.cos(sweepAng)*maxR,ly=cy+Math.sin(sweepAng)*maxR;
    ctx.strokeStyle="rgba(62,232,197,0.75)";ctx.lineWidth=1.5;
    ctx.beginPath();ctx.moveTo(cx,cy);ctx.lineTo(lx,ly);ctx.stroke();
    // trailing glow
    ctx.strokeStyle="rgba(62,232,197,0.18)";ctx.lineWidth=6;
    ctx.beginPath();ctx.moveTo(cx,cy);ctx.lineTo(lx,ly);ctx.stroke();

    const layout=radarLayout(allDevices,maxM);
    const seen={};
    hitList=[];
    for(const d of allDevices){
      const mac=d.mac||"";
      if(!mac) continue;
      seen[mac]=1;
      const lay=layout.get(mac);
      const targetA=lay?lay.ang:-Math.PI/2;
      const targetR=distR(d.distance_m!=null?d.distance_m:rssiFallback(d),maxR,maxM);
      let st=blipAnim[mac];
      if(!st){st=blipAnim[mac]={a:targetA,r:targetR};}
      else{
        const k=paused?1:0.12;
        st.a=angLerp(st.a,targetA,k);
        st.r=lerp(st.r,targetR,k);
      }
      const x=cx+Math.cos(st.a)*st.r,y=cy+Math.sin(st.a)*st.r;
      hitList.push({mac,x,y});
      const fresh=(d.last_seen_ms||9999)<2500;
      const trust=deviceTrust(d);
      let col=bandColor(d);
      if(trust==="blacklist") col="#ff5959";
      else if(trust==="whitelist") col="#6ba3ff";
      if((d.distance_m||99)<2) col="#3ee8c5";
      else if((d.distance_m||99)<8&&trust==="none") col="#fb923c";
      ctx.save();ctx.globalAlpha=1;
      const sel=mac===selectedMac;
      const sz=sel?6:(trust==="blacklist"?5:3.5);
      ctx.fillStyle=col;
      ctx.beginPath();ctx.arc(x,y,sz,0,Math.PI*2);ctx.fill();
      if(sel){
        ctx.strokeStyle="#3ee8c5";ctx.lineWidth=1.5;ctx.globalAlpha=0.9;
        ctx.beginPath();ctx.arc(x,y,sz+4,0,Math.PI*2);ctx.stroke();
      }else if(fresh){
        const pulse=0.35+0.25*Math.sin(now*0.008);
        ctx.strokeStyle=col;ctx.globalAlpha=pulse;ctx.lineWidth=1;
        ctx.beginPath();ctx.arc(x,y,sz+3+pulse*2,0,Math.PI*2);ctx.stroke();
      }
      ctx.restore();
    }
    for(const k of Object.keys(blipAnim)){if(!seen[k]) delete blipAnim[k];}
    if(selectedMac&&!seen[selectedMac]){selectedMac=null;showTip(null);}

    // YOU (phone center)
    ctx.fillStyle="#3ee8c5";
    ctx.beginPath();ctx.arc(cx,cy,5,0,Math.PI*2);ctx.fill();
    ctx.fillStyle="#3ee8c5";
    ctx.font="bold 9px ui-monospace,Menlo,Consolas,monospace";
    ctx.textAlign="center";
    ctx.fillText("YOU",cx,cy+16);
    ctx.textAlign="start";

    ctx.strokeStyle="rgba(62,232,197,0.35)";ctx.lineWidth=1;
    ctx.beginPath();ctx.arc(cx,cy,maxR,0,Math.PI*2);ctx.stroke();
  }catch(err){
    // keep the loop alive even if a frame fails
  }
  requestAnimationFrame(paint);
}
function rssiFallback(d){
  const rssi=Number(d.avg!=null?d.avg:d.rssi);
  if(!Number.isFinite(rssi)) return 25;
  return Math.pow(10,(-50-rssi)/40);
}
paint();

function renderList(){
  const empty=document.getElementById("empty"),head=document.getElementById("listHead");
  const cards=document.getElementById("devCards");
  const rows=sorted(allDevices),mc=allDevices.filter(matches).length;
  const live=liveCount();
  head.textContent=allDevices.length
    ?(live+" live · "+allDevices.length+" session"+(filtOn()?" · "+mc+" match":""))
    :"passive rf · probe / beacon / data / lora";
  const c=bandCounts();
  document.getElementById("radarMeta").textContent=allDevices.length
    ?(live+" live · "+c.wifi+" wifi · "+c.subghz+" sub‑ghz · "+c.lora+" lora")
    :(rfMeta.subghz&&rfMeta.subghz.ready
      ?("sub‑ghz "+(rfMeta.subghz.band||"?")+"MHz rssi "+(rfMeta.subghz.rssi??"—")+" · lora "+(rfMeta.lora&&rfMeta.lora.uart_bytes!=null?rfMeta.lora.uart_bytes+" uart bytes":"idle"))
      :"listening");
  renderBandPills();
  if(!allDevices.length){
    cards.innerHTML="";empty.style.display="block";return;
  }
  empty.style.display="none";
  const hl=filtOn();
  cards.innerHTML=rows.map(d=>renderDeviceCard(d,{dim:hl&&!matches(d)})).join("");
}

document.getElementById("search").oninput=e=>{searchQ=e.target.value;renderList();};
document.getElementById("btnAddWl").onclick=()=>{
  const v=document.getElementById("listEntry").value;
  if(!addToList("whitelist",v)) alert("Enter valid MAC (AA:BB:…) or IP (10.0.0.1)");
  else document.getElementById("listEntry").value="";
};
document.getElementById("btnAddBl").onclick=()=>{
  const v=document.getElementById("listEntry").value;
  if(!addToList("blacklist",v)) alert("Enter valid MAC (AA:BB:…) or IP (10.0.0.1)");
  else document.getElementById("listEntry").value="";
};
document.querySelector(".lists-box").onclick=e=>{
  const rm=e.target.closest("[data-rm]");
  if(rm) removeFromList(rm.dataset.rm,parseInt(rm.dataset.i,10));
};
document.getElementById("list").onclick=e=>{
  const cp=e.target.closest("[data-copy]");
  if(cp){
    const d=allDevices.find(x=>x.mac===cp.dataset.copy);
    if(d) copyDevice(d); return;
  }
  const cj=e.target.closest("[data-copy-json]");
  if(cj){
    const d=allDevices.find(x=>x.mac===cj.dataset.copyJson);
    if(d) copyDeviceJson(d); return;
  }
  const wl=e.target.closest("[data-add-wl]");
  if(wl){addToList("whitelist",wl.dataset.addWl);return;}
  const bl=e.target.closest("[data-add-bl]");
  if(bl){addToList("blacklist",bl.dataset.addBl);return;}
  const card=e.target.closest(".dev-card[data-mac]");
  if(card) selectMac(card.dataset.mac===selectedMac?null:card.dataset.mac);
};
radarTip.onclick=e=>{
  const cp=e.target.closest("[data-tip-copy]");
  if(cp){
    const d=allDevices.find(x=>x.mac===cp.dataset.tipCopy);
    if(d) copyDevice(d); return;
  }
  const cj=e.target.closest("[data-tip-json]");
  if(cj){
    const d=allDevices.find(x=>x.mac===cj.dataset.tipJson);
    if(d) copyDeviceJson(d);
  }
};
document.getElementById("filters").onclick=e=>{
  const c=e.target.closest(".chip");if(!c)return;
  if(c.dataset.f==="clear"){activeFilters.clear();searchQ="";document.getElementById("search").value="";
    document.querySelectorAll(".chip[data-f]").forEach(x=>{if(x.dataset.f!=="clear")x.classList.remove("on");});renderList();return;}
  c.classList.toggle("on");
  if(c.classList.contains("on"))activeFilters.add(c.dataset.f);else activeFilters.delete(c.dataset.f);
  renderList();
};

async function fetchTimeout(url,ms){
  const c=new AbortController();
  const t=setTimeout(()=>c.abort(),ms);
  try{return await fetch(url,{cache:"no-store",signal:c.signal});}
  finally{clearTimeout(t);}
}

async function pollRf(){
  try{
    const res=await fetchTimeout("/api/rf",4000);
    if(!res.ok) return;
    const j=await res.json();
    rfMeta.subghz=j.subghz||null;
    rfMeta.lora=j.lora||null;
  }catch(e){}
}

async function poll(){
  if(paused) return;
  try{
    const ping=await fetchTimeout("/api/ping",4000);
    if(!ping.ok) throw new Error("api "+ping.status);
    const res=await fetchTimeout("/api/devices",12000);
    if(!res.ok) throw new Error("api "+res.status);
    const text=await res.text();
    let data;
    try{data=JSON.parse(text);}catch(parseErr){throw new Error("json");}
    mergeDevices(data.devices||[]);
    scanMeta.channel=data.channel??null;
    scanMeta.hopping=data.hopping??null;
    scanMeta.uptime_ms=data.uptime_ms||0;
    scanMeta.session_ms=data.session_ms||data.uptime_ms||0;
    scanMeta.name=data.name||"root";
    scanMeta.total=allDevices.length;
    updateCountBadge();
    pollRf().then(()=>{
      document.getElementById("statusMsg").textContent=statusLine();
      renderList();
    });
    document.getElementById("statusMsg").textContent=statusLine();
    document.getElementById("dot").classList.remove("off");
    document.getElementById("ch").textContent=data.channel??"—";
    document.getElementById("hop").textContent=data.hopping?"auto":"fixed";
    document.getElementById("up").textContent=fmtUp(data.uptime_ms||0);
    if(data.hopping)chSel.value="auto";else if(data.channel)chSel.value=String(data.channel);
    renderList();
    if(selectedMac){
      const d=allDevices.find(x=>x.mac===selectedMac);
      if(d) showTip(d); else selectMac(null);
    }
  }catch(e){
    document.getElementById("dot").classList.add("off");
    const m=(e&&e.message)||"";
    let msg="Offline — stay on Wi-Fi root (no internet is OK)";
    if(m.startsWith("api 503")) msg="Busy — retrying…";
    else if(m.startsWith("api ")) msg="API error "+m.slice(4);
    else if(m==="json") msg="Bad data — retrying…";
    document.getElementById("statusMsg").textContent=msg;
  }
}
pollTimer=setInterval(poll,1500);poll();
chSel.onchange=()=>fetch("/api/channel?ch="+encodeURIComponent(chSel.value)).catch(()=>{});
setTab(mob()?"radar":"both");
</script>
</body>
</html>
)HTML";
