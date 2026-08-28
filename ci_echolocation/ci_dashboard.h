#pragma once
// root dashboard — mobile tabs, full radar, highlight filters

static const char DASHBOARD_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover"/>
<meta http-equiv="Cache-Control" content="no-cache, no-store, must-revalidate"/>
<meta http-equiv="Pragma" content="no-cache"/>
<meta http-equiv="Expires" content="0"/>
<meta name="apple-mobile-web-app-capable" content="yes"/>
<title>root · Wi‑Fi · BLE · Sub‑GHz recon</title>
<style>
:root{
  --bg:#07080c;--bg2:#0c0e14;--panel:rgba(18,20,28,.92);--glass:rgba(255,255,255,.04);
  --ink:#f4f6fb;--muted:#8b93a7;--muted-dim:rgba(139,147,167,.55);--line:rgba(255,255,255,.08);
  --accent:#3ee8c5;--accent-dim:rgba(62,232,197,.22);--accent-glow:rgba(62,232,197,.12);
  --near:#f87171;--mid:#fbbf24;--far:#6b8f7a;--lora:#c084fc;--subghz:#fb923c;
  --radius:14px;--radius-sm:10px;--shadow:0 12px 40px rgba(0,0,0,.45);
  --shadow-lg:0 20px 50px rgba(0,0,0,.55),0 0 0 1px rgba(255,255,255,.06);
  --font:Inter,ui-sans-serif,system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;
  --mono:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;
  --t-xs:11px;--t-sm:13px;--t-base:15px;--t-lg:18px;--t-xl:24px;
  --s1:4px;--s2:8px;--s3:12px;--s4:16px;--s5:24px;
  --ease:180ms ease;
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
  grid-row:1;display:flex;align-items:center;gap:var(--s3);padding:var(--s4) var(--s4) var(--s3);
  background:linear-gradient(180deg,var(--panel),rgba(12,14,20,.88));
  border-bottom:1px solid var(--line);backdrop-filter:blur(16px);flex-shrink:0;flex-wrap:wrap;
}
.brand{display:flex;align-items:center;gap:var(--s3);min-width:0}
.brand-mark{
  width:36px;height:36px;border-radius:11px;flex-shrink:0;
  background:linear-gradient(135deg,rgba(62,232,197,.25),rgba(62,232,197,.05));
  border:1px solid var(--accent-dim);display:grid;place-items:center;
  font-family:var(--mono);font-size:var(--t-xs);font-weight:800;color:var(--accent);letter-spacing:-.04em;
}
.brand-text{min-width:0}
.brand-title{font-size:var(--t-lg);font-weight:700;letter-spacing:-.02em;line-height:1.15}
.brand-tag{font-size:var(--t-xs);color:var(--muted-dim);letter-spacing:.04em;text-transform:uppercase}
.brand-sub{font-size:var(--t-sm);color:var(--muted);margin-top:var(--s1);line-height:1.35}
.status-dot{width:8px;height:8px;border-radius:50%;background:var(--accent);flex-shrink:0;box-shadow:0 0 12px var(--accent-dim);transition:background var(--ease),box-shadow var(--ease)}
.status-dot.off{background:var(--near);box-shadow:0 0 12px rgba(248,113,113,.4)}
.status-count{
  font-family:var(--mono);font-size:var(--t-sm);font-weight:700;padding:var(--s2) var(--s3);border-radius:999px;
  background:var(--glass);border:1px solid var(--line);color:var(--accent);flex-shrink:0;
}
.band-pills{display:flex;gap:var(--s2);flex-wrap:wrap;font-size:var(--t-xs);font-family:var(--mono)}
.band-pill{padding:var(--s1) var(--s2);border-radius:999px;border:1px solid var(--line);color:var(--muted);background:var(--glass);transition:border-color var(--ease),color var(--ease)}
.band-pill.wifi{color:var(--accent);border-color:var(--accent-dim)}
.band-pill.subghz{color:var(--subghz);border-color:rgba(251,146,60,.28)}
.band-pill.lora{color:var(--lora);border-color:rgba(192,132,252,.28)}
.band-pill-sub{font-size:10px;color:var(--muted-dim);font-weight:500}
.toolbar-actions{display:flex;gap:var(--s2);margin-left:auto;flex-wrap:wrap}
.btn{
  padding:0 var(--s3);min-height:40px;min-width:40px;border:1px solid var(--line);border-radius:var(--radius-sm);
  background:var(--glass);color:var(--ink);font-size:var(--t-sm);font-weight:600;cursor:pointer;
  font-family:var(--font);transition:background var(--ease),border-color var(--ease),color var(--ease),transform 120ms ease,box-shadow var(--ease);
  display:inline-flex;align-items:center;justify-content:center;gap:var(--s2);
}
.btn:hover{background:rgba(255,255,255,.08);border-color:rgba(255,255,255,.14)}
.btn:focus-visible{outline:2px solid var(--accent-dim);outline-offset:2px}
.btn:active{transform:scale(.97);background:rgba(255,255,255,.05)}
.btn.primary{border-color:var(--accent-dim);color:var(--accent);background:var(--accent-glow)}
.btn.primary:hover{background:rgba(62,232,197,.18);border-color:rgba(62,232,197,.45)}
.btn.paused{border-color:rgba(251,191,36,.35);color:var(--mid)}
.btn.icon{padding:0 var(--s2);min-width:40px}
.btn svg{width:16px;height:16px;flex-shrink:0}

.shell{grid-row:2;min-height:0;overflow:hidden;display:flex;flex-direction:column}
@media(max-width:720px){
  .shell{flex:1;min-height:0}
  .panel.on{flex:1;min-height:0}
  #panelList.on .list-scroll{flex:1;min-height:0}
}
@media(min-width:721px){
  .shell{display:grid;grid-template-columns:1.05fr .95fr;gap:10px;padding:10px}
  .mob-tabs{display:none!important}
  .dev-cards{display:grid;grid-template-columns:1fr 1fr;gap:var(--s3);align-content:start}
}
.panel{display:none;flex-direction:column;min-height:0;overflow:hidden;background:var(--panel);border:1px solid var(--line);box-shadow:var(--shadow);transition:opacity var(--ease),border-color var(--ease)}
.panel.on{display:flex}
@media(min-width:721px){.panel{display:flex!important;border-radius:var(--radius)}}

.panel-head{
  flex-shrink:0;padding:var(--s3) var(--s4);display:flex;justify-content:space-between;align-items:center;
  border-bottom:1px solid var(--line);background:rgba(255,255,255,.02);
}
.panel-title{font-size:var(--t-sm);font-weight:700;letter-spacing:.06em;text-transform:uppercase;color:var(--muted-dim)}
.panel-meta{font-size:var(--t-xs);color:var(--muted);font-family:var(--mono)}

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
.radar-legend{
  position:absolute;top:var(--s3);right:var(--s3);z-index:3;padding:var(--s2) var(--s3);border-radius:var(--radius-sm);
  background:rgba(8,10,14,.9);border:1px solid var(--line);backdrop-filter:blur(8px);
  font-size:var(--t-xs);line-height:1.55;color:var(--muted-dim);pointer-events:none;font-family:var(--mono);
}
.radar-legend .leg-row{display:flex;align-items:center;gap:var(--s2)}
.radar-legend i{display:inline-block;width:8px;height:8px;border-radius:50%;flex-shrink:0}
.radar-tip{
  position:absolute;left:50%;bottom:var(--s4);transform:translateX(-50%);z-index:4;max-width:min(92%,420px);
  padding:var(--s3) var(--s4) var(--s3) var(--s3);border-radius:var(--radius);
  background:rgba(10,14,22,.92);backdrop-filter:blur(16px);
  border:1px solid rgba(62,232,197,.28);color:var(--ink);font-size:var(--t-sm);line-height:1.45;
  box-shadow:var(--shadow-lg);display:none;pointer-events:none;
}
.tip-close{
  position:absolute;top:var(--s2);right:var(--s2);width:32px;height:32px;border:0;border-radius:var(--radius-sm);
  background:rgba(255,255,255,.06);color:var(--muted);font-size:var(--t-lg);line-height:1;cursor:pointer;
  display:grid;place-items:center;transition:background var(--ease),color var(--ease);
}
.tip-close:hover{background:rgba(255,255,255,.12);color:var(--ink)}
.tip-close:focus-visible{outline:2px solid var(--accent-dim);outline-offset:1px}
.radar-tip.show{display:block;pointer-events:auto}
.radar-tip b{display:block;font-size:var(--t-base);font-weight:700;margin-bottom:var(--s1);padding-right:var(--s5)}
.radar-tip .sub{color:var(--muted);font-family:var(--mono);font-size:var(--t-xs);word-break:break-all}
.radar-tip .tip-actions{display:flex;gap:var(--s2);margin-top:var(--s3)}
.radar-hint{position:absolute;top:var(--s3);left:var(--s3);z-index:3;font-size:var(--t-xs);color:var(--muted-dim);pointer-events:none;font-family:var(--mono)}
.radar-disclaimer{
  position:absolute;left:10px;right:10px;bottom:10px;z-index:3;pointer-events:none;
  padding:8px 10px;border-radius:8px;background:rgba(8,10,14,.88);border:1px solid var(--line);
  font-size:.64rem;line-height:1.35;color:var(--muted);
}
.radar-disclaimer b{color:var(--accent);font-weight:600}

.list-toolbar{flex-shrink:0;padding:var(--s3);border-bottom:1px solid var(--line);display:flex;flex-direction:column;gap:var(--s3);background:rgba(255,255,255,.015)}
@media(max-width:720px){
  .list-toolbar{padding:var(--s2) var(--s3);gap:var(--s2)}
  .lists-box:not([open]){padding:0;border:0}
}
.search-wrap{position:relative}
.search{
  width:100%;padding:var(--s3) var(--s4) var(--s3) 38px;border-radius:var(--radius-sm);border:1px solid var(--line);
  background:rgba(0,0,0,.25);color:var(--ink);font-size:var(--t-base);font-family:var(--font);
  transition:border-color var(--ease),background var(--ease);
}
.search:focus{outline:none;border-color:var(--accent-dim);background:rgba(0,0,0,.35)}
.search-wrap::before{content:"⌕";position:absolute;left:13px;top:50%;transform:translateY(-50%);color:var(--muted);font-size:var(--t-base);pointer-events:none}
.filters{display:flex;gap:var(--s2);align-items:center;flex-wrap:wrap;padding-bottom:var(--s1)}
.chip-hint{font-size:var(--t-xs);color:var(--muted-dim);flex-shrink:0;text-transform:uppercase;letter-spacing:.06em}
.chip{
  flex-shrink:0;padding:var(--s2) var(--s3);min-height:36px;border-radius:999px;border:1px solid var(--line);
  background:var(--glass);color:var(--muted);font-size:var(--t-xs);font-weight:600;
  display:inline-flex;align-items:center;cursor:pointer;transition:background var(--ease),border-color var(--ease),color var(--ease),transform 120ms ease;
}
.chip:hover{background:rgba(255,255,255,.07);color:var(--ink)}
.chip:focus-visible{outline:2px solid var(--accent-dim);outline-offset:2px}
.chip:active{transform:scale(.97)}
.chip.on{border-color:var(--accent-dim);background:var(--accent-glow);color:var(--accent)}
.chip.on::after{content:attr(data-count);margin-left:var(--s1);padding:0 5px;border-radius:999px;background:rgba(62,232,197,.2);font-size:10px;font-weight:700}

.filters-wrap{position:relative;flex-shrink:0}

.rf-monitor{
  flex-shrink:0;padding:var(--s3) var(--s4);border-bottom:1px solid var(--line);
  background:linear-gradient(180deg,rgba(251,146,60,.04),transparent);
  transition:border-color var(--ease),background var(--ease);
}
.rf-monitor>summary,.lora-monitor>summary{
  list-style:none;cursor:pointer;user-select:none;
}
.rf-monitor>summary::-webkit-details-marker,.lora-monitor>summary::-webkit-details-marker{display:none}
.rf-monitor>summary::after,.lora-monitor>summary::after{
  content:"▾";float:right;color:var(--muted-dim);font-size:10px;transition:transform var(--ease);
}
.rf-monitor:not([open])>summary::after,.lora-monitor:not([open])>summary::after{transform:rotate(-90deg)}
@media(max-width:720px){
  .rf-monitor,.lora-monitor{padding:var(--s2) var(--s3)}
  .rf-monitor:not(.on-filter):not([open]),.lora-monitor:not(.on-filter):not([open]){padding-bottom:var(--s2)}
  .rf-monitor-grid,.lora-monitor-grid{margin-top:var(--s2)}
}
.rf-monitor.on-filter{
  border-bottom-color:rgba(251,146,60,.35);
  background:linear-gradient(180deg,rgba(251,146,60,.1),rgba(251,146,60,.02));
}
.rf-monitor-title{
  font-size:var(--t-xs);font-weight:700;letter-spacing:.08em;text-transform:uppercase;
  color:var(--subghz);margin-bottom:0;
}
.rf-monitor[open] .rf-monitor-title,.lora-monitor[open] .lora-monitor-title{margin-bottom:var(--s2)}
.rf-monitor-grid{display:grid;grid-template-columns:repeat(4,1fr);gap:var(--s2)}
@media(max-width:520px){.rf-monitor-grid{grid-template-columns:repeat(2,1fr)}}
.rf-tile{
  padding:var(--s2) var(--s3);border-radius:var(--radius-sm);border:1px solid var(--line);
  background:rgba(0,0,0,.22);transition:border-color var(--ease),background var(--ease);
}
.rf-tile.live{border-color:rgba(251,146,60,.35);background:rgba(251,146,60,.08)}
.rf-tile-k{font-size:var(--t-xs);color:var(--muted-dim);text-transform:uppercase;letter-spacing:.06em;font-weight:600}
.rf-tile-v{font-family:var(--mono);font-size:var(--t-base);font-weight:700;color:var(--ink);margin-top:var(--s1)}
.rf-tile.live .rf-tile-v{color:var(--subghz)}
.rf-tile-sub{font-size:var(--t-xs);color:var(--muted);font-family:var(--mono);margin-top:var(--s1)}

.lora-monitor{
  flex-shrink:0;padding:var(--s3) var(--s4);border-bottom:1px solid var(--line);
  background:linear-gradient(180deg,rgba(192,132,252,.06),transparent);
  transition:border-color var(--ease),background var(--ease);
}
.lora-monitor.on-filter{
  border-bottom-color:rgba(192,132,252,.35);
  background:linear-gradient(180deg,rgba(192,132,252,.12),rgba(192,132,252,.02));
}
.lora-monitor-title{
  font-size:var(--t-xs);font-weight:700;letter-spacing:.08em;text-transform:uppercase;
  color:var(--lora);margin-bottom:0;
}
.lora-monitor-grid{display:grid;grid-template-columns:repeat(5,1fr);gap:var(--s2)}
@media(max-width:720px){
  .lora-monitor-grid{grid-template-columns:repeat(3,1fr)}
  .rf-monitor-grid{grid-template-columns:repeat(4,1fr);gap:var(--s1)}
  .rf-tile,.lora-tile{padding:6px var(--s2)}
  .rf-tile-v{font-size:var(--t-xs)}
  .lora-tile-v{font-size:11px}
  .rf-tile-sub,.lora-tile-sub{font-size:10px;margin-top:2px}
}
@media(max-width:400px){
  .lora-monitor-grid{grid-template-columns:repeat(2,1fr)}
  .rf-monitor-grid{grid-template-columns:repeat(2,1fr)}
}
.lora-tile{
  padding:var(--s2) var(--s3);border-radius:var(--radius-sm);border:1px solid var(--line);
  background:rgba(0,0,0,.22);transition:border-color var(--ease),background var(--ease);
}
.lora-tile.live{border-color:rgba(192,132,252,.35);background:rgba(192,132,252,.08)}
.lora-tile-k{font-size:var(--t-xs);color:var(--muted-dim);text-transform:uppercase;letter-spacing:.06em;font-weight:600}
.lora-tile-v{font-family:var(--mono);font-size:var(--t-sm);font-weight:700;color:var(--ink);margin-top:var(--s1);word-break:break-all}
.lora-tile.live .lora-tile-v{color:var(--lora)}
.lora-tile-sub{font-size:var(--t-xs);color:var(--muted);font-family:var(--mono);margin-top:var(--s1)}

.list-head{
  flex-shrink:0;padding:var(--s2) var(--s4);font-size:var(--t-xs);font-weight:600;color:var(--muted-dim);
  border-bottom:1px solid var(--line);letter-spacing:.06em;text-transform:uppercase;font-family:var(--mono);
}
.list-scroll{flex:1;min-height:0;overflow:auto;-webkit-overflow-scrolling:touch;overscroll-behavior:contain;padding:var(--s3)}
@media(max-width:720px){.list-scroll{padding:var(--s2) var(--s3) calc(var(--s2) + env(safe-area-inset-bottom))}}

.dev-cards{display:flex;flex-direction:column;gap:var(--s2);align-content:start}
.dev-card{
  display:flex;align-items:stretch;gap:var(--s2);padding:var(--s3) var(--s4) var(--s3) calc(var(--s4) + 3px);border-radius:var(--radius-sm);
  background:var(--glass);border:1px solid var(--line);transition:border-color var(--ease),background var(--ease),opacity var(--ease),transform 120ms ease;
  position:relative;overflow:hidden;cursor:pointer;flex-direction:column;
}
@media(max-width:720px){
  .dev-card{padding:var(--s2) var(--s3) var(--s2) calc(var(--s3) + 3px);gap:var(--s1)}
  .dev-summary,.dev-kv-grid{display:none}
  .dev-card.sel .dev-summary,.dev-card.sel .dev-kv-grid{display:block}
  .dev-card:not(.sel) .card-actions{
    flex-direction:row;border:0;margin:0;padding:0;gap:var(--s1);
  }
  .dev-card:not(.sel) .card-actions .icon-btn{width:30px;height:30px;min-width:30px}
  .dev-card:not(.sel) .card-actions .mini{display:none}
  .dev-top{margin-bottom:0}
  .dev-mac{margin-bottom:var(--s1);font-size:10px}
  .dev-name{font-size:var(--t-sm)}
  .dev-dist{font-size:var(--t-xs)}
}
.dev-card:hover{background:rgba(255,255,255,.06);border-color:rgba(255,255,255,.12)}
.dev-card:focus-within{border-color:var(--accent-dim)}
.dev-card>.card-actions{
  flex-direction:row;flex-wrap:wrap;gap:var(--s2);margin-top:var(--s3);padding-top:var(--s3);border-top:1px solid var(--line);
}
.dev-summary{
  font-size:var(--t-sm);color:var(--muted);line-height:1.35;margin:0 0 var(--s2);
  padding:var(--s2) var(--s3);border-radius:var(--radius-sm);background:rgba(0,0,0,.18);border:1px solid var(--line);
}
.dev-compact{display:flex;flex-wrap:wrap;gap:4px;align-items:center;margin-top:var(--s1)}
@media(min-width:721px){.dev-compact{display:none}}
.dev-kv-grid{
  display:grid;grid-template-columns:minmax(72px,auto) 1fr;gap:var(--s1) var(--s4);
  align-items:baseline;
}
.dev-kv{display:contents}
.dev-kv-k{
  font-size:var(--t-xs);font-weight:600;letter-spacing:.06em;text-transform:uppercase;
  color:var(--muted-dim);padding:var(--s1) 0;
}
.dev-kv-v{
  font-family:var(--mono);font-size:var(--t-sm);font-weight:500;color:var(--ink);
  text-align:right;word-break:break-all;padding:var(--s1) 0;
}
.card-actions .icon-btn,.card-actions .mini{width:auto;min-width:36px;height:36px}
.dev-card::before{
  content:"";position:absolute;left:0;top:0;bottom:0;width:3px;background:var(--accent);opacity:.5;
}
.dev-card.subghz::before{background:var(--subghz)}
.dev-card.lora::before{background:var(--lora)}
.dev-card.sel{border-color:var(--accent-dim);background:rgba(62,232,197,.08);box-shadow:0 0 0 1px rgba(62,232,197,.12)}
.dev-card.trust-bl{border-color:rgba(248,113,113,.25)}
.dev-card.trust-wl{border-color:rgba(107,163,255,.25)}
.dev-card-main{flex:1;min-width:0}
.dev-top{display:flex;justify-content:space-between;align-items:flex-start;gap:var(--s2);margin-bottom:var(--s1)}
.dev-name{font-size:var(--t-base);font-weight:600;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.dev-dist{font-family:var(--mono);font-size:var(--t-sm);font-weight:600;color:var(--accent);flex-shrink:0}
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
  color:var(--muted);cursor:pointer;display:grid;place-items:center;transition:background var(--ease),border-color var(--ease),color var(--ease),transform 120ms ease;
}
.icon-btn:hover,.icon-btn:focus-visible{color:var(--accent);border-color:var(--accent-dim);background:var(--accent-glow)}
.icon-btn:active{transform:scale(.95)}
.icon-btn svg{width:15px;height:15px}
.mini{
  padding:var(--s1) var(--s2);border:1px solid var(--line);border-radius:8px;background:transparent;
  color:var(--muted);font-size:var(--t-xs);font-weight:700;cursor:pointer;font-family:var(--mono);
  transition:background var(--ease),border-color var(--ease),color var(--ease),transform 120ms ease;
}
.mini:hover{background:rgba(255,255,255,.06)}
.mini:active{transform:scale(.95)}
.mini.wl{border-color:rgba(107,163,255,.35);color:#6ba3ff}
.mini.bl{border-color:rgba(248,113,113,.35);color:var(--near)}

.lists-box{flex-shrink:0;border-top:1px solid var(--line);padding:var(--s2) 0 0;font-size:var(--t-sm)}
.lists-box summary{cursor:pointer;color:var(--muted);font-weight:600;font-size:var(--t-xs);text-transform:uppercase;letter-spacing:.04em;transition:color var(--ease)}
.lists-box summary:hover{color:var(--ink)}
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

.empty{padding:var(--s4) var(--s3);text-align:center;color:var(--muted);font-size:var(--t-sm);line-height:1.5}
@media(max-width:720px){.empty{padding:var(--s3) var(--s2)}}

.mob-tabs{grid-row:3;display:flex;background:var(--panel);border-top:1px solid var(--line);flex-shrink:0;padding:4px 8px calc(4px + env(safe-area-inset-bottom))}
.mob-tabs button{
  flex:1;border:0;background:transparent;color:var(--muted);font-size:var(--t-sm);font-weight:600;
  padding:var(--s3) var(--s2);min-height:48px;cursor:pointer;border-radius:var(--radius-sm);
  transition:color var(--ease),background var(--ease),transform 120ms ease;
}
.mob-tabs button:hover{background:rgba(255,255,255,.04);color:var(--ink)}
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
      <div class="radar-hint">tap blip · inspect</div>
      <div class="radar-legend" aria-hidden="true">
        <div class="leg-row"><i style="background:#3ee8c5"></i> close (&lt;2m)</div>
        <div class="leg-row"><i style="background:#fb923c"></i> mid range</div>
        <div class="leg-row"><i style="background:#6b8f7a"></i> far / weak</div>
        <div class="leg-row"><i style="background:#c084fc"></i> LoRa</div>
        <div class="leg-row"><i style="background:#6ba3ff"></i> whitelist</div>
        <div class="leg-row"><i style="background:#ff5959"></i> flagged</div>
      </div>
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
      <div class="filters-wrap">
        <div class="filters" id="filters">
          <span class="chip-hint">Filter</span>
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
          <span class="chip" data-f="flagged">Flagged</span>
          <span class="chip" data-f="clear">Clear</span>
        </div>
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
    <details class="rf-monitor" id="rfMonitor">
      <summary class="rf-monitor-title">RF Band Monitor</summary>
      <div class="rf-monitor-grid" id="rfMonitorGrid"></div>
    </details>
    <details class="lora-monitor" id="loraMonitor">
      <summary class="lora-monitor-title">LR22 LoRa · 915 MHz</summary>
      <div class="lora-monitor-grid" id="loraMonitorGrid"></div>
    </details>
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

const LS_WL="root_wl_v2",LS_BL="root_bl_v2";
function migrateListKey(oldKey,newKey){
  try{
    const cur=localStorage.getItem(newKey);
    if(cur) return;
    const leg=localStorage.getItem(oldKey);
    if(leg) localStorage.setItem(newKey,leg);
  }catch(e){}
}
function normalizeListEntry(e){
  if(!e||e.type!=="mac") return e;
  const m=normMac(e.id);
  if(m&&m.length===17) e.id=m;
  return e;
}
function loadLists(){
  migrateListKey("root_whitelist",LS_WL);
  migrateListKey("root_blacklist",LS_BL);
  try{whitelist=JSON.parse(localStorage.getItem(LS_WL)||"[]").map(normalizeListEntry);}catch(e){whitelist=[];}
  try{blacklist=JSON.parse(localStorage.getItem(LS_BL)||"[]").map(normalizeListEntry);}catch(e){blacklist=[];}
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
  normalizeListEntry(e);
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
const TRUST_FILTERS=new Set(["whitelist","flagged"]);
function trustKindForFilter(f){return f==="flagged"?"blacklist":f==="whitelist"?"whitelist":null;}
function trustStubs(kind){
  const list=kind==="blacklist"?blacklist:whitelist;
  const stubs=[];
  const liveMacs=new Set(listDevices().map(d=>(d.mac||"").toUpperCase()));
  for(const e of list){
    if(e.type!=="mac") continue;
    const mac=normMac(e.id);
    if(!mac||mac.length!==17) continue;
    if(liveMacs.has(mac.toUpperCase())) continue;
    stubs.push({
      mac,
      band:"wifi",
      kind:"unknown",
      ssid:kind==="blacklist"?"(flagged — not heard this session)":"(trusted — not heard this session)",
      zone:"Far",
      distance_m:0,
      avg:null,
      rssi:null,
      last_seen_ms:999999,
      seen_count:0,
      _stub:true
    });
  }
  return stubs;
}
function devicesForTrustFilter(filterKey){
  const kind=trustKindForFilter(filterKey);
  if(!kind) return [];
  const live=listDevices().filter(d=>deviceTrust(d)===kind);
  return [...live,...trustStubs(kind)];
}
function visibleDevices(){
  let rows=listDevices();
  const trustOnly=activeFilters.has("flagged")&&!activeFilters.has("whitelist")&&activeFilters.size===1&&!searchQ.trim();
  const wlOnly=activeFilters.has("whitelist")&&!activeFilters.has("flagged")&&activeFilters.size===1&&!searchQ.trim();
  if(trustOnly) rows=devicesForTrustFilter("flagged");
  else if(wlOnly) rows=devicesForTrustFilter("whitelist");
  else if(activeFilters.has("flagged")||activeFilters.has("whitelist")){
    const merged=[];
    const seen=new Set();
    if(activeFilters.has("flagged")){
      for(const d of devicesForTrustFilter("flagged")){
        if(!seen.has(d.mac)){seen.add(d.mac);merged.push(d);}
      }
    }
    if(activeFilters.has("whitelist")){
      for(const d of devicesForTrustFilter("whitelist")){
        if(!seen.has(d.mac)){seen.add(d.mac);merged.push(d);}
      }
    }
    rows=merged;
  }
  if(!filtOn()) return rows;
  return rows.filter(matches);
}
const RF_BAND_DEFS=[{mhz:315,idx:0},{mhz:433,idx:1},{mhz:868,idx:2},{mhz:915,idx:3}];

function isRfBandMonitor(d){
  if(!d) return false;
  const h=normHex(d.mac||"");
  if(h.length===12&&h.startsWith("0253000000")){
    const slot=parseInt(h.slice(10,12),16);
    if(slot>=0&&slot<=3) return true;
  }
  return /^\d+\s*MHz\s+scan/i.test(d.ssid||"");
}
function monitorBandIdx(d){
  const h=normHex(d.mac||"");
  if(h.length===12&&h.startsWith("0253000000")) return parseInt(h.slice(10,12),16);
  const m=(d.ssid||"").match(/^(\d+)/);
  if(m){
    const mhz=parseInt(m[1],10);
    const i=RF_BAND_DEFS.findIndex(b=>b.mhz===mhz||(mhz===433&&b.mhz===433));
    if(i>=0) return i;
  }
  return 0;
}
function isLoraListener(d){
  const h=normHex(d&&d.mac||"");
  return h==="024C91500001";
}
function listDevices(){return allDevices.filter(d=>!isRfBandMonitor(d));}
function monitorDevices(){return allDevices.filter(isRfBandMonitor);}

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
function kvRow(label,value){
  return '<div class="dev-kv"><span class="dev-kv-k">'+label+'</span><span class="dev-kv-v">'+value+'</span></div>';
}
function renderDeviceFields(d){
  const band=d.band||"wifi",kind=d.kind||"?",z=d.zone||"Far";
  const rssiRaw=d.avg!=null?d.avg:d.rssi;
  const rssi=Number.isFinite(Number(rssiRaw))?Number(rssiRaw).toFixed(1)+" dBm":"—";
  const dist=Number(d.distance_m||0).toFixed(1);
  let html="";
  html+=kvRow("MAC",d.mac||"—");
  if(d.rand) html+=kvRow("Privacy","Randomized");
  html+=kvRow("Band",band);
  html+=kvRow("Type",kind);
  if(d.ssid&&d.ssid.trim()) html+=kvRow("SSID / label",d.ssid.replace(/</g,"&lt;"));
  if(d.ch) html+=kvRow("Channel","ch"+d.ch);
  html+=kvRow("RSSI",rssi);
  html+=kvRow("Distance","~"+dist+" m");
  html+=kvRow("Zone",z);
  html+=kvRow("Seen",(d.seen_count||1)+"×");
  html+=kvRow("Last heard",fmtAge(d.last_seen_ms||0));
  if((d.gps===true||d.gps==="true")&&d.lat&&d.lon){
    html+=kvRow("GPS",Number(d.lat).toFixed(5)+", "+Number(d.lon).toFixed(5));
  }
  return '<div class="dev-kv-grid">'+html+'</div>';
}
function renderDeviceCard(d,opts){
  opts=opts||{};
  const trust=deviceTrust(d);
  const tcl=trust==="blacklist"?" trust-bl":trust==="whitelist"?" trust-wl":"";
  const sel=d.mac===selectedMac?" sel":"";
  const bandCls=d.band==="lora"?" lora":d.band==="subghz"?" subghz":"";
  const macEsc=(d.mac||"").replace(/"/g,"&quot;");
  const band=d.band||"wifi",kind=d.kind||"?",z=d.zone||"Far";
  const rssiRaw=d.avg!=null?d.avg:d.rssi;
  const rssi=Number.isFinite(Number(rssiRaw))?Number(rssiRaw).toFixed(0)+" dBm":"—";
  const tags=
    '<span class="tag '+band+'">'+band+'</span>'+
    '<span class="tag">'+kind+'</span>'+
    '<span class="tag">'+rssi+'</span>'+
    '<span class="tag">'+z+'</span>'+
    (d.ssid&&d.ssid.trim()?'<span class="tag ssid">'+d.ssid.replace(/</g,"&lt;")+'</span>':"");
  return '<div class="dev-card'+tcl+sel+bandCls+'" data-mac="'+macEsc+'">'+
    '<div class="dev-card-main">'+
    '<div class="dev-top"><div class="dev-name">'+plainName(d).replace(/</g,"&lt;")+'</div>'+
    '<div class="dev-dist">~'+Number(d.distance_m||0).toFixed(1)+'m</div></div>'+
    '<div class="dev-mac">'+(d.mac||"")+'</div>'+
    '<div class="dev-compact">'+tags+'</div>'+
    '<div class="dev-summary">'+deviceWhatIs(d).replace(/</g,"&lt;")+'</div>'+
    renderDeviceFields(d)+'</div>'+
    '<div class="card-actions">'+
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
  return listDevices().filter(d=>(d.last_seen_ms||999999)<LIVE_MS).length;
}
function updateCountBadge(){
  const el=document.getElementById("count");
  if(!el) return;
  const rows=listDevices();
  const total=rows.length,live=liveCount();
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
function rfBandLiveCount(){
  let n=0;
  for(const d of monitorDevices()){
    if((d.last_seen_ms||99999)<30000) n++;
  }
  return n;
}
function loraListening(){
  if(!rfMeta.lora||!rfMeta.lora.ready) return false;
  const pk=Number(rfMeta.lora.packets)||0;
  const uart=Number(rfMeta.lora.uart_bytes)||0;
  const active=Number(rfMeta.lora.active_ms);
  return pk>0||uart>0||active<30000;
}
function bandCounts(){
  let wifi=0,subghz=0,lora=0;
  for(const d of listDevices()){
    if(isLoraListener(d)) continue;
    if(d.band==="subghz") subghz++;
    else if(d.band==="lora") lora++;
    else wifi++;
  }
  const bands=rfBandLiveCount();
  const loListen=(rfMeta.lora&&rfMeta.lora.ready)?1:0;
  return {wifi,subghz,lora,bands,loListen};
}
function renderBandPills(){
  const el=document.getElementById("bandPills");
  if(!el) return;
  const c=bandCounts();
  const rfOn=rfMeta.subghz&&rfMeta.subghz.ready;
  const loOn=rfMeta.lora&&rfMeta.lora.ready;
  let sgMain,sgSub;
  if(c.subghz>0){
    sgMain=String(c.subghz);
    sgSub=c.bands>0?(c.bands+" band scan"):null;
  }else if(c.bands>0){
    sgMain=c.bands+" band";
    sgSub="scanning";
  }else if(rfOn){
    sgMain="scanning";
    sgSub=null;
  }else{
    sgMain="off";
    sgSub=null;
  }
  let loMain,loSub;
  if(c.lora>0){
    loMain=String(c.lora);
    loSub=loOn?"listening":"";
  }else if(loOn){
    loMain="listening";
    loSub=(Number(rfMeta.lora.packets)||0)+" pkts";
  }else{
    loMain="off";
    loSub=null;
  }
  el.innerHTML=
    '<span class="band-pill wifi">'+c.wifi+' Wi‑Fi</span>'+
    '<span class="band-pill subghz" title="Emitters in list · band noise floor in RF monitor">'+
      sgMain+(sgSub?' <span class="band-pill-sub">'+sgSub+'</span>':"")+' Sub‑GHz</span>'+
    '<span class="band-pill lora" title="LoRa packets + LR22 monitor">'+
      loMain+(loSub?' <span class="band-pill-sub">'+loSub+'</span>':"")+' LoRa</span>';
}
function renderLoraMonitor(){
  const el=document.getElementById("loraMonitor");
  const grid=document.getElementById("loraMonitorGrid");
  if(!el||!grid) return;
  const lo=rfMeta.lora;
  if(!lo||!lo.ready){el.style.display="none";return;}
  el.style.display="block";
  if(mob()&&!el.hasAttribute("data-touched")&&!activeFilters.has("lora")) el.open=false;
  const pk=Number(lo.packets)||0;
  const uart=Number(lo.uart_bytes)||0;
  const rssi=lo.last_rssi!=null?Number(lo.last_rssi):null;
  const active=Number(lo.active_ms)||999999;
  const live=uart>0&&active<5000;
  const lbl=(lo.last_label||"").trim();
  const tiles=[
    {k:"status",v:live?"live":(pk?"heard":"listening"),sub:live?"uart active":"915 MHz RX"},
    {k:"packets",v:String(pk),sub:"decoded bursts"},
    {k:"uart",v:String(uart),sub:"bytes from LR22"},
    {k:"rssi",v:rssi!=null?rssi+" dBm":"—",sub:"last signal"},
    {k:"last",v:lbl?lbl.slice(0,28):"—",sub:lbl?"last payload":"waiting for RF"}
  ];
  grid.innerHTML=tiles.map(t=>'<div class="lora-tile'+(live&&t.k==="status"?" live":"")+'">'+
    '<div class="lora-tile-k">'+t.k+'</div>'+
    '<div class="lora-tile-v">'+t.v+'</div>'+
    '<div class="lora-tile-sub">'+t.sub+'</div></div>').join("");
}
function renderRfMonitor(){
  const el=document.getElementById("rfMonitor");
  const grid=document.getElementById("rfMonitorGrid");
  if(!grid) return;
  if(el&&mob()&&!el.hasAttribute("data-touched")&&!activeFilters.has("subghz")) el.open=false;
  const tiles=RF_BAND_DEFS.map(b=>({mhz:b.mhz,idx:b.idx,dbm:null,delta:null,live:false,sub:"noise floor"}));
  for(const d of monitorDevices()){
    const i=monitorBandIdx(d);
    if(i<0||i>=tiles.length) continue;
    const rssi=Number(d.avg!=null?d.avg:d.rssi);
    tiles[i].dbm=Number.isFinite(rssi)?rssi:null;
    tiles[i].live=(d.last_seen_ms||99999)<5000;
    const dm=(d.ssid||"").match(/\(\+(\d+)dB\)/i);
    if(dm) tiles[i].delta=dm[1];
    tiles[i].sub=tiles[i].live?"live":"idle";
  }
  if(rfMeta.subghz&&rfMeta.subghz.ready){
    const cur=rfMeta.subghz.band;
    const ti=tiles.find(t=>String(t.mhz)===String(cur)||(cur==="433"&&t.mhz===433));
    if(ti&&ti.dbm==null&&rfMeta.subghz.rssi!=null) ti.dbm=Number(rfMeta.subghz.rssi);
  }
  grid.innerHTML=tiles.map(t=>{
    const val=t.dbm!=null?t.dbm.toFixed(0)+" dBm":"—";
    const sub=t.delta!=null?"+"+t.delta+" dB · "+t.sub:t.sub;
    return '<div class="rf-tile'+(t.live?" live":"")+'">'+
      '<div class="rf-tile-k">'+t.mhz+' MHz</div>'+
      '<div class="rf-tile-v">'+val+'</div>'+
      '<div class="rf-tile-sub">'+sub+'</div></div>';
  }).join("");
}
function fmtAge(ms){
  if(ms<1000) return ms+"ms";
  const s=ms/1000;
  if(s<60) return s.toFixed(1)+"s";
  return Math.floor(s/60)+"m";
}
function fmtUp(ms){const s=Math.floor(ms/1000),m=Math.floor(s/60);return m?m+"m":s+"s";}
function hay(d){return [plainName(d),d.ssid||"",d.mac||"",d.kind||"",d.band||"",d.zone||""].join(" ").toLowerCase();}
function dkind(d){
  if(d.band==="subghz"||d.band==="lora") return d.band;
  if(d.kind==="probe"||d.kind==="data") return "probe";
  if(d.kind==="beacon") return "beacon";
  return d.kind||"wifi";
}
function filtOn(){return activeFilters.size>0||!!searchQ.trim();}
function matchesWith(d,filters,qOverride){
  const q=(qOverride!=null?qOverride:searchQ).trim().toLowerCase();
  if(q&&!hay(d).includes(q)) return false;
  const af=filters||activeFilters;
  if(!af.size) return true;
  const bands=[...af].filter(f=>BANDS.has(f));
  const ks=[...af].filter(f=>KINDS.has(f));
  const zs=[...af].filter(f=>ZONES.has(f));
  if(bands.length&&!bands.includes(d.band||"wifi")) return false;
  if(ks.length&&!ks.includes(dkind(d))) return false;
  if(zs.length&&!zs.includes(d.zone)) return false;
  if(af.has("fresh")&&(d.last_seen_ms||99999)>=30000) return false;
  if(af.has("fixed")&&!FIXED_RF.test(d.ssid||"")) return false;
  if(af.has("whitelist")&&deviceTrust(d)!=="whitelist") return false;
  if(af.has("flagged")&&deviceTrust(d)!=="blacklist") return false;
  return true;
}
function matches(d){return matchesWith(d,activeFilters,null);}
function sorted(a){return [...a].sort((x,y)=>zoneRank(x.zone)-zoneRank(y.zone)||(x.distance_m||99)-(y.distance_m||99));}

const FILTER_NAMES={subghz:"Sub-GHz",fixed:"Fixed RF",lora:"LoRa",probe:"Phones",beacon:"Wi-Fi",Near:"Close",Mid:"Near",Far:"Far",fresh:"Recent",whitelist:"Whitelist",flagged:"Flagged"};
function filterSummary(){
  return [...activeFilters].map(f=>FILTER_NAMES[f]||f).join(" · ");
}
function matchesFilter(d,f){
  if(f==="flagged") return deviceTrust(d)==="blacklist";
  if(f==="whitelist") return deviceTrust(d)==="whitelist";
  return matchesWith(d,new Set([f]),"");
}
function countForFilter(f){
  if(f==="flagged") return devicesForTrustFilter("flagged").length;
  if(f==="whitelist") return devicesForTrustFilter("whitelist").length;
  return listDevices().filter(d=>matchesFilter(d,f)).length;
}
function updateChipCounts(){
  document.querySelectorAll(".chip[data-f]").forEach(chip=>{
    const f=chip.dataset.f;
    if(!f||f==="clear") return;
    const n=countForFilter(f);
    if(n>0) chip.setAttribute("data-count",String(n));
    else chip.removeAttribute("data-count");
  });
}

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
  const live=liveCount(), total=listDevices().length;
  const c=bandCounts();
  const parts=[];
  if(c.wifi) parts.push(c.wifi+" Wi‑Fi");
  if(c.subghz) parts.push(c.subghz+" Sub‑GHz");
  else if(c.bands) parts.push(c.bands+" band scan");
  else if(rfMeta.subghz&&rfMeta.subghz.ready) parts.push("Sub‑GHz listening");
  if(c.lora) parts.push(c.lora+" LoRa");
  else if(loraListening()) parts.push("LoRa listening");
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
    if(!mac||isRfBandMonitor(d)) continue;
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
      const d=bucket[i];
      const baseAng=n<=1?-Math.PI/2:((i/n)*Math.PI*2-Math.PI/2);
      const jitter=((macHash(d.mac)%1000)/1000-0.5)*0.22;
      layout.set(d.mac,{ang:baseAng+jitter,bi});
    }
  }
  return layout;
}
function maxRingMeters(){
  let far=12;
  for(const d of listDevices()){
    const m=Number(d.distance_m);
    if(Number.isFinite(m)&&m>far) far=m;
  }
  return Math.min(Math.max(far*1.15,12),80);
}
function distR(meters,maxR,maxM){
  const t=Math.min(Math.max((Number(meters)||40)/Math.max(maxM,1),0.02),1);
  return 14+t*(maxR-18);
}
function blipPos(mac,d,maxR,maxM,layout){
  const lay=layout.get(mac);
  const ang=lay?lay.ang:-Math.PI/2;
  const r=distR(d.distance_m!=null?d.distance_m:rssiFallback(d),maxR,maxM);
  return {ang,r};
}
function showTip(d){
  if(!d){radarTip.classList.remove("show");radarTip.innerHTML="";return;}
  const trust=deviceTrust(d);
  const tag=trust==="blacklist"?" · flagged":trust==="whitelist"?" · trusted":"";
  const macEsc=(d.mac||"").replace(/"/g,"&quot;");
  radarTip.innerHTML='<button type="button" class="tip-close" aria-label="Close">×</button>'+
    "<b>"+plainName(d).replace(/</g,"&lt;")+"</b>"+
    "<div class='sub'>"+deviceWhatIs(d).replace(/</g,"&lt;")+tag+"</div>"+
    "<div class='sub' style='margin-top:6px'>"+
    "MAC "+(d.mac||"")+" · "+Number(d.avg||d.rssi).toFixed(0)+" dBm · ~"+Number(d.distance_m||0).toFixed(1)+"m · "+(d.zone||"?")+
    "</div>"+
    "<div class='tip-actions'>"+
    "<button type='button' class='btn primary' data-tip-copy='"+macEsc+"'>Copy</button>"+
    "<button type='button' class='btn' data-tip-json='"+macEsc+"'>JSON</button>"+
    "</div>";
  radarTip.classList.add("show");
  const closeBtn=radarTip.querySelector(".tip-close");
  if(closeBtn) closeBtn.onclick=e=>{e.stopPropagation();selectMac(null);};
}
function selectMac(mac){
  selectedMac=mac||null;
  const d=listDevices().find(x=>x.mac===selectedMac);
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
  let best=null,bestD=28;
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

    const layout=radarLayout(listDevices(),maxM);
    const seen={};
    hitList=[];
    for(const d of listDevices()){
      const mac=d.mac||"";
      if(!mac) continue;
      seen[mac]=1;
      const pos=blipPos(mac,d,maxR,maxM,layout);
      const x=cx+Math.cos(pos.ang)*pos.r,y=cy+Math.sin(pos.ang)*pos.r;
      hitList.push({mac,x,y});
      const ageMs=d.last_seen_ms||99999;
      const fresh=ageMs<2500;
      const recent=ageMs<15000;
      const trust=deviceTrust(d);
      let col=bandColor(d);
      if(trust==="blacklist") col="#ff5959";
      else if(trust==="whitelist") col="#6ba3ff";
      if((d.distance_m||99)<2) col="#3ee8c5";
      else if((d.distance_m||99)<8&&trust==="none") col="#fb923c";
      ctx.save();
      const sel=mac===selectedMac;
      const sz=sel?6:(trust==="blacklist"?5:3.5);
      const baseAlpha=recent?1:0.42;
      ctx.globalAlpha=baseAlpha;
      ctx.fillStyle=col;
      ctx.beginPath();ctx.arc(x,y,sz,0,Math.PI*2);ctx.fill();
      if(fresh){
        const pulse=0.45+0.35*Math.sin(now*0.009);
        ctx.shadowColor=col;ctx.shadowBlur=8+pulse*6;
        ctx.strokeStyle=col;ctx.globalAlpha=pulse*baseAlpha;ctx.lineWidth=1.5;
        ctx.beginPath();ctx.arc(x,y,sz+3+pulse*3,0,Math.PI*2);ctx.stroke();
        ctx.shadowBlur=0;
      }
      if(sel){
        ctx.strokeStyle="#3ee8c5";ctx.globalAlpha=0.95;ctx.lineWidth=1.5;
        ctx.beginPath();ctx.arc(x,y,sz+5,0,Math.PI*2);ctx.stroke();
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
  const allRows=sorted(listDevices());
  const filtered=filtOn();
  const displayRows=filtered?sorted(visibleDevices()):allRows;
  const live=liveCount();
  updateChipCounts();
  if(filtered){
    head.textContent=displayRows.length+" filtered · "+filterSummary()+(allRows.length?" ("+allRows.length+" total)":"");
  }else{
    head.textContent=allRows.length
      ?(live+" live · "+allRows.length+" session")
      :"passive rf · probe / beacon / data / lora";
  }
  const c=bandCounts();
  const sgMeta=c.subghz>0?(c.subghz+" sub‑ghz"):(c.bands?(c.bands+" band scan"):"sub‑ghz idle");
  const loMeta=c.lora>0?(c.lora+" lora"):(loraListening()?"lora listening":"lora idle");
  document.getElementById("radarMeta").textContent=allRows.length
    ?(live+" live · "+c.wifi+" wifi · "+sgMeta+" · "+loMeta)
    :(rfMeta.subghz&&rfMeta.subghz.ready
      ?("sub‑ghz "+(rfMeta.subghz.band||"?")+"MHz rssi "+(rfMeta.subghz.rssi??"—")+" · lora "+(rfMeta.lora&&rfMeta.lora.uart_bytes!=null?rfMeta.lora.uart_bytes+" uart bytes":"idle"))
      :"listening");
  renderBandPills();
  renderRfMonitor();
  renderLoraMonitor();
  const rfMon=document.getElementById("rfMonitor");
  if(rfMon){
    rfMon.classList.toggle("on-filter",activeFilters.has("subghz"));
    if(mob()&&activeFilters.has("subghz")) rfMon.open=true;
  }
  const loMon=document.getElementById("loraMonitor");
  if(loMon){
    loMon.classList.toggle("on-filter",activeFilters.has("lora"));
    if(mob()&&activeFilters.has("lora")) loMon.open=true;
  }
  if(!displayRows.length){
    cards.innerHTML="";
    empty.style.display="block";
    if(filtered&&activeFilters.has("subghz")&&!searchQ.trim()&&c.bands>0&&!c.subghz){
      empty.textContent="No Sub‑GHz burst emitters — "+c.bands+" band"+(c.bands>1?"s":"")+" scanning in RF monitor above";
    }else if(filtered&&activeFilters.has("lora")&&!searchQ.trim()&&rfMeta.lora&&rfMeta.lora.ready){
      const pk=Number(rfMeta.lora.packets)||0;
      const uart=Number(rfMeta.lora.uart_bytes)||0;
      empty.textContent=pk>0||uart>0
        ?("No other LoRa rows — LR22 stats in monitor above · "+pk+" pkts · "+uart+" uart B")
        :"LR22 listening — stats in LoRa monitor above (needs matching 915 MHz LoRa traffic)";
    }else{
      empty.textContent=filtered
        ?("No devices match: "+filterSummary()+(searchQ.trim()?" · search «"+searchQ.trim()+"»":""))
        :"No devices yet — listening on Wi‑Fi, sub‑GHz, and LoRa…";
    }
    return;
  }
  empty.style.display="none";
  cards.innerHTML=displayRows.map(d=>renderDeviceCard(d)).join("");
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
  if(e.target.closest(".tip-close")) return;
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
  const f=c.dataset.f;
  if(f==="clear"){
    activeFilters.clear();searchQ="";document.getElementById("search").value="";
    document.querySelectorAll(".chip[data-f]").forEach(x=>{if(x.dataset.f!=="clear")x.classList.remove("on");});
    renderList();return;
  }
  const turningOn=!c.classList.contains("on");
  if(turningOn){
    activeFilters.clear();
    document.querySelectorAll(".chip[data-f]").forEach(x=>{if(x.dataset.f!=="clear")x.classList.remove("on");});
    c.classList.add("on");
    activeFilters.add(f);
  }else{
    c.classList.remove("on");
    activeFilters.delete(f);
  }
  if(mob()) setTab("list");
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
    scanMeta.total=listDevices().length;
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
      const d=listDevices().find(x=>x.mac===selectedMac);
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
["rfMonitor","loraMonitor"].forEach(id=>{
  const el=document.getElementById(id);
  if(el) el.addEventListener("toggle",()=>{if(el.open) el.setAttribute("data-touched","1");});
});
setTab(mob()?"radar":"both");
</script>
</body>
</html>
)HTML";
