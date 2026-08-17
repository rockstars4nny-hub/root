#pragma once
// CI dashboard — served from PROGMEM at GET /
// Keep the closing raw-literal tag out of this file's JavaScript.

static const char DASHBOARD_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>CI — SEE</title>
<style>
  :root {
    --bg: #070b10;
    --panel: #0d141c;
    --ink: #e7f6ee;
    --muted: #7f9488;
    --line: #1c2a22;
    --ping: #3dff9a;
    --ping-dim: rgba(61,255,154,.16);
    --far: #ff5d73;
    --mid: #ffd166;
    --near: #3dff9a;
  }
  * { box-sizing: border-box; }
  html, body { margin: 0; height: 100%; background: var(--bg); color: var(--ink);
    font-family: ui-sans-serif, system-ui, "Segoe UI", sans-serif; }
  body { display: flex; flex-direction: column; min-height: 100%; }
  header {
    display: flex; align-items: center; justify-content: space-between; gap: 16px;
    padding: 18px 22px 10px; border-bottom: 1px solid var(--line);
  }
  .brand { display: flex; align-items: baseline; gap: 10px; }
  .brand h1 { margin: 0; font-size: 28px; letter-spacing: .22em; font-weight: 700; }
  .brand .see { color: var(--ping); font-size: 12px; letter-spacing: .4em; text-transform: uppercase; }
  .meta { display: flex; flex-wrap: wrap; gap: 10px; color: var(--muted); font-size: 12px; }
  .pill {
    border: 1px solid var(--line); background: var(--panel); border-radius: 999px;
    padding: 6px 10px; font-variant-numeric: tabular-nums;
  }
  .pill b { color: var(--ping); font-weight: 600; }
  main {
    flex: 1; display: grid; grid-template-columns: minmax(280px, 42%) 1fr;
    gap: 18px; padding: 18px 22px 24px;
  }
  @media (max-width: 860px) { main { grid-template-columns: 1fr; } }
  .radar-wrap {
    background: radial-gradient(circle at 50% 50%, #102018 0%, var(--panel) 62%, #080c10 100%);
    border: 1px solid var(--line); border-radius: 18px; position: relative; overflow: hidden;
    min-height: 360px;
  }
  canvas { display: block; width: 100%; height: 100%; }
  .radar-label {
    position: absolute; left: 14px; bottom: 12px; color: var(--muted); font-size: 11px;
    letter-spacing: .12em; text-transform: uppercase;
  }
  .table-wrap {
    background: var(--panel); border: 1px solid var(--line); border-radius: 18px;
    overflow: auto; max-height: calc(100vh - 140px);
  }
  table { width: 100%; border-collapse: collapse; font-size: 13px; }
  th, td { padding: 10px 12px; text-align: left; border-bottom: 1px solid var(--line);
    font-variant-numeric: tabular-nums; white-space: nowrap; }
  th { color: var(--muted); font-size: 11px; letter-spacing: .08em; text-transform: uppercase; position: sticky; top: 0; background: #101820; }
  tr:hover td { background: rgba(61,255,154,.04); }
  .mac { font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace; }
  .zone { font-size: 11px; letter-spacing: .08em; text-transform: uppercase; }
  .Near { color: var(--near); }
  .Mid { color: var(--mid); }
  .Far { color: var(--far); }
  .tag { font-size: 10px; border: 1px solid var(--line); border-radius: 999px; padding: 1px 6px; color: var(--muted); }
  footer { padding: 0 22px 16px; color: var(--muted); font-size: 11px; }
  select, button {
    background: #101820; color: var(--ink); border: 1px solid var(--line); border-radius: 8px;
    padding: 6px 8px; font: inherit;
  }
  button { cursor: pointer; }
  button:hover { border-color: var(--ping); }
</style>
</head>
<body>
<header>
  <div class="brand">
    <h1>CI</h1>
    <div class="see">see</div>
  </div>
  <div class="meta">
    <div class="pill">devices <b id="count">0</b></div>
    <div class="pill">ch <b id="ch">-</b></div>
    <div class="pill">hop <b id="hop">-</b></div>
    <div class="pill">uptime <b id="up">0s</b></div>
    <label class="pill">channel
      <select id="chSel">
        <option value="auto">auto</option>
      </select>
    </label>
  </div>
</header>
<main>
  <section class="radar-wrap">
    <canvas id="radar"></canvas>
    <div class="radar-label">passive rssi radar · probe / beacon / data</div>
  </section>
  <section class="table-wrap">
    <table>
      <thead>
        <tr>
          <th>MAC</th>
          <th>Avg RSSI</th>
          <th>Distance</th>
          <th>Zone</th>
          <th>Last seen</th>
          <th>Kind</th>
        </tr>
      </thead>
      <tbody id="rows"></tbody>
    </table>
  </section>
</main>
<footer>CI listens in promiscuous mode. Distances are RSSI estimates, not ranging. Use only on networks and premises you are authorized to observe.</footer>
<script>
const canvas = document.getElementById("radar");
const ctx = canvas.getContext("2d");
const chSel = document.getElementById("chSel");
let devices = [];
let sweep = 0;

for (let i = 1; i <= 13; i++) {
  const o = document.createElement("option");
  o.value = String(i);
  o.textContent = String(i);
  chSel.appendChild(o);
}

function resize() {
  const r = canvas.parentElement.getBoundingClientRect();
  const dpr = Math.min(window.devicePixelRatio || 1, 2);
  canvas.width = Math.floor(r.width * dpr);
  canvas.height = Math.floor(r.height * dpr);
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
}
window.addEventListener("resize", resize);
resize();

function macAngle(mac) {
  let h = 2166136261;
  for (let i = 0; i < mac.length; i++) {
    h ^= mac.charCodeAt(i);
    h = Math.imul(h, 16777619);
  }
  return ((h >>> 0) % 360) * Math.PI / 180;
}
function rssiR(rssi, maxR) {
  const t = Math.min(1, Math.max(0, (-35 - rssi) / 60));
  return 22 + t * (maxR - 30);
}
function zoneColor(z) {
  if (z === "Near") return "#3dff9a";
  if (z === "Mid") return "#ffd166";
  return "#ff5d73";
}
function fmtAge(ms) {
  if (ms < 1000) return ms + "ms";
  const s = ms / 1000;
  if (s < 60) return s.toFixed(1) + "s";
  return Math.floor(s / 60) + "m";
}
function fmtUp(ms) {
  const s = Math.floor(ms / 1000);
  const m = Math.floor(s / 60);
  const h = Math.floor(m / 60);
  if (h) return h + "h " + (m % 60) + "m";
  if (m) return m + "m " + (s % 60) + "s";
  return s + "s";
}

function draw() {
  const w = canvas.width / (window.devicePixelRatio || 1);
  const h = canvas.height / (window.devicePixelRatio || 1);
  const cx = w / 2, cy = h / 2;
  const maxR = Math.max(40, Math.min(cx, cy) - 16);
  ctx.clearRect(0, 0, w, h);

  ctx.strokeStyle = "rgba(61,255,154,.14)";
  ctx.lineWidth = 1;
  for (let i = 1; i <= 4; i++) {
    ctx.beginPath();
    ctx.arc(cx, cy, maxR * (i / 4), 0, Math.PI * 2);
    ctx.stroke();
  }
  ctx.beginPath();
  ctx.moveTo(cx - maxR, cy); ctx.lineTo(cx + maxR, cy);
  ctx.moveTo(cx, cy - maxR); ctx.lineTo(cx, cy + maxR);
  ctx.stroke();

  sweep += 0.018;
  const grd = ctx.createConicGradient(sweep, cx, cy);
  grd.addColorStop(0, "rgba(61,255,154,0)");
  grd.addColorStop(0.08, "rgba(61,255,154,.22)");
  grd.addColorStop(0.12, "rgba(61,255,154,0)");
  ctx.fillStyle = grd;
  ctx.beginPath();
  ctx.arc(cx, cy, maxR, 0, Math.PI * 2);
  ctx.fill();

  ctx.fillStyle = "#3dff9a";
  ctx.beginPath();
  ctx.arc(cx, cy, 3, 0, Math.PI * 2);
  ctx.fill();

  for (const d of devices) {
    const a = macAngle(d.mac);
    const r = rssiR(d.avg, maxR);
    const x = cx + Math.cos(a) * r;
    const y = cy + Math.sin(a) * r;
    const fresh = d.last_seen_ms < 2500;
    ctx.fillStyle = zoneColor(d.zone);
    ctx.globalAlpha = fresh ? 1 : 0.45;
    ctx.beginPath();
    ctx.arc(x, y, fresh ? 5 : 3.5, 0, Math.PI * 2);
    ctx.fill();
    if (fresh) {
      ctx.globalAlpha = 0.25;
      ctx.beginPath();
      ctx.arc(x, y, 11, 0, Math.PI * 2);
      ctx.fill();
    }
    ctx.globalAlpha = 1;
  }
  requestAnimationFrame(draw);
}
draw();

async function poll() {
  try {
    const res = await fetch("/api/devices", { cache: "no-store" });
    const data = await res.json();
    devices = data.devices || [];
    document.getElementById("count").textContent = data.count ?? devices.length;
    document.getElementById("ch").textContent = data.channel;
    document.getElementById("hop").textContent = data.hopping ? "on" : "lock";
    document.getElementById("up").textContent = fmtUp(data.uptime_ms || 0);
    if (data.hopping) chSel.value = "auto";
    else if (data.channel) chSel.value = String(data.channel);

    const tb = document.getElementById("rows");
    tb.innerHTML = devices.map(d => {
      const rand = d.rand ? '<span class="tag">rand</span>' : "";
      return `<tr>
        <td class="mac">${d.mac} ${rand}</td>
        <td>${Number(d.avg).toFixed(1)} dBm</td>
        <td>~${Number(d.distance_m).toFixed(1)}m</td>
        <td class="zone ${d.zone}">${d.zone}</td>
        <td>${fmtAge(d.last_seen_ms)}</td>
        <td>${d.kind} <span class="tag">ch${d.ch}</span></td>
      </tr>`;
    }).join("");
  } catch (e) {}
}
setInterval(poll, 2000);
poll();

chSel.addEventListener("change", async () => {
  const v = chSel.value;
  try { await fetch("/api/channel?ch=" + encodeURIComponent(v)); } catch (e) {}
});
</script>
</body>
</html>
)HTML";
