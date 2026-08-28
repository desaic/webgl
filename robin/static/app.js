const usd = new Intl.NumberFormat("en-US", { style: "currency", currency: "USD" });
const usdShort = new Intl.NumberFormat("en-US", { style: "currency", currency: "USD", minimumFractionDigits: 0, maximumFractionDigits: 0 });
const pct = (v) => (v == null || isNaN(v) ? "—" : (v >= 0 ? "+" : "") + v.toFixed(2) + "%");

function cls(v) { return v == null ? "" : v > 0 ? "pos" : v < 0 ? "neg" : ""; }

let chartData = [];

async function loadChart() {
  try {
    const raw = await getJSON("/api/history");
    const series = [];
    const keys = Object.keys(raw).sort();
    const cutoff = new Date(Date.now() - 30 * 86400000).toISOString().slice(0, 10);
    for (const k of keys) {
      if (k < cutoff) continue;
      for (const [ts, val] of raw[k]) series.push([ts, val]);
    }
    chartData = series;
  } catch (e) { chartData = []; }
  drawChart();
}

function drawChart() {
  const canvas = document.getElementById("history-chart");
  if (!canvas) return;
  const rect = canvas.parentElement.getBoundingClientRect();
  const dpr = window.devicePixelRatio || 1;
  canvas.width = rect.width * dpr;
  canvas.height = 260 * dpr;
  canvas.style.width = rect.width + "px";
  canvas.style.height = "260px";
  const ctx = canvas.getContext("2d");
  ctx.scale(dpr, dpr);
  const w = rect.width;
  const h = 260;

  const isLight = document.body.classList.contains("light");
  const bg = isLight ? "#f2efe9" : "#161b22";
  const fg = isLight ? "#1a1a1a" : "#c9d1d9";
  const muted = isLight ? "#6b6b6b" : "#8b949e";
  const accent = isLight ? "#2563eb" : "#58a6ff";
  const pos = isLight ? "#2d7a3e" : "#3fb950";
  const negColor = isLight ? "#c0392b" : "#f85149";

  ctx.clearRect(0, 0, w, h);
  ctx.fillStyle = bg;
  ctx.fillRect(0, 0, w, h);

  if (chartData.length < 2) {
    ctx.fillStyle = muted;
    ctx.font = "14px ui-sans-serif, system-ui, sans-serif";
    ctx.textAlign = "center";
    ctx.fillText("No history yet — data appears after 5-min polls", w / 2, h / 2);
    return;
  }

  const pad = { top: 12, right: 16, bottom: 30, left: 60 };
  const pw = w - pad.left - pad.right;
  const ph = h - pad.top - pad.bottom;

  const tvals = chartData.map(d => d[1]);
  let maxVal = Math.max(...tvals);
  let minVal = Math.max(Math.min(...tvals), 0.8 * maxVal);

  const xScale = (i) => pad.left + (i / (chartData.length - 1)) * pw;
  const yScale = (v) => pad.top + ph - ((v - minVal) / (maxVal - minVal)) * ph;

  const lastVal = tvals[tvals.length - 1];
  const firstVal = tvals[0];
  const trend = lastVal > firstVal ? pos : lastVal < firstVal ? negColor : muted;
  const pctChg = firstVal ? ((lastVal - firstVal) / firstVal) * 100 : 0;

  ctx.strokeStyle = muted;
  ctx.lineWidth = 0.5;
  const gridLines = 5;
  for (let i = 0; i <= gridLines; i++) {
    const v = minVal + (maxVal - minVal) * (i / gridLines);
    const y = yScale(v);
    ctx.beginPath();
    ctx.moveTo(pad.left, y);
    ctx.lineTo(w - pad.right, y);
    ctx.stroke();
    ctx.fillStyle = muted;
    ctx.font = "10px ui-monospace, monospace";
    ctx.textAlign = "right";
    ctx.fillText(usdShort.format(v), pad.left - 6, y + 4);
  }

  const dateLabels = [];
  const tickMap = new Map();
  for (const [ts] of chartData) {
    const d = ts.slice(0, 10);
    if (tickMap.has(d)) continue;
    tickMap.set(d, chartData.findIndex(([t]) => t.slice(0, 10) === d));
  }
  const days = [...tickMap.entries()].sort((a, b) => a[0].localeCompare(b[0]));
  const skip = Math.max(1, Math.floor(days.length / 6));
  for (let i = 0; i < days.length; i += skip) {
    dateLabels.push({ date: days[i][0], idx: days[i][1] });
  }
  if (dateLabels.length === 0 || dateLabels[dateLabels.length - 1].idx !== chartData.length - 1) {
    dateLabels.push({ date: chartData[chartData.length - 1][0].slice(0, 10), idx: chartData.length - 1 });
  }

  ctx.font = "10px ui-sans-serif, system-ui, sans-serif";
  for (const dl of dateLabels) {
    const x = xScale(dl.idx);
    const d = new Date(dl.date + "T00:00:00");
    ctx.fillStyle = muted;
    ctx.textAlign = "center";
    ctx.fillText(d.toLocaleDateString(undefined, { month: "short", day: "numeric" }), x, h - 4);
  }

  const grad = ctx.createLinearGradient(0, pad.top, 0, pad.top + ph);
  grad.addColorStop(0, trend === pos ? "rgba(63,185,80,0.15)" : "rgba(248,81,73,0.15)");
  grad.addColorStop(1, "rgba(0,0,0,0)");

  ctx.beginPath();
  ctx.moveTo(xScale(0), yScale(tvals[0]));
  for (let i = 1; i < tvals.length; i++) {
    ctx.lineTo(xScale(i), yScale(tvals[i]));
  }
  ctx.strokeStyle = trend;
  ctx.lineWidth = 2;
  ctx.stroke();

  ctx.lineTo(xScale(tvals.length - 1), pad.top + ph);
  ctx.lineTo(xScale(0), pad.top + ph);
  ctx.closePath();
  ctx.fillStyle = grad;
  ctx.fill();

  const legendDiv = canvas.parentElement.querySelector(".chart-legend");
  if (!legendDiv) {
    const div = document.createElement("div");
    div.className = "chart-legend";
    const sign = pctChg >= 0 ? "+" : "";
    div.innerHTML = `<span><span class="dot val"></span> ${usd.format(lastVal)}</span><span style="color:${trend}">${sign}${usd.format(lastVal - firstVal)} (${sign}${pctChg.toFixed(1)}%)</span>`;
    canvas.parentElement.appendChild(div);
  } else {
    const sign = pctChg >= 0 ? "+" : "";
    legendDiv.innerHTML = `<span><span class="dot val"></span> ${usd.format(lastVal)}</span><span style="color:${trend}">${sign}${usd.format(lastVal - firstVal)} (${sign}${pctChg.toFixed(1)}%)</span>`;
  }

  canvas._chartMeta = { pad, xScale, yScale, w, h, tvals, minVal, maxVal };
}

let tooltip = null;
function ensureTooltip() {
  if (!tooltip) {
    tooltip = document.createElement("div");
    tooltip.className = "chart-tooltip";
    document.querySelector(".chart-container").appendChild(tooltip);
  }
  return tooltip;
}

document.getElementById("history-chart").addEventListener("mousemove", function(e) {
  const meta = this._chartMeta;
  if (!meta || chartData.length < 2) return;
  const rect = this.getBoundingClientRect();
  const mx = e.clientX - rect.left;
  const { pad, xScale, tvals } = meta;
  if (mx < pad.left || mx > meta.w - pad.right) { ensureTooltip().style.display = "none"; return; }

  const norm = (mx - pad.left) / (meta.w - pad.left - pad.right);
  const idx = Math.round(norm * (chartData.length - 1));
  const clamped = Math.max(0, Math.min(idx, chartData.length - 1));
  const [ts, val] = chartData[clamped];
  const d = new Date(ts);
  const tip = ensureTooltip();
  tip.style.display = "block";
  tip.innerHTML = `<div class="tooltip-date">${d.toLocaleString()}</div><div class="tooltip-val">${usd.format(val)}</div>`;
  const x = xScale(clamped);
  tip.style.left = (x - tip.offsetWidth / 2) + "px";
  tip.style.top = (pad.top - 40) + "px";
});

document.getElementById("history-chart").addEventListener("mouseleave", function() {
  if (tooltip) tooltip.style.display = "none";
});

window.addEventListener("resize", drawChart);


function setStatus(s) {
  const el = document.getElementById("status");
  const items = [
    ["Robinhood", s.robinhood_logged_in ? "connected" : (s.robinhood_has_session ? "session" : "offline"), s.robinhood_logged_in ? "ok" : "warn"],
    ["Mode", s.robinhood_mode, s.robinhood_mode === "real" ? "ok" : "warn"],
    ["Market", s.market_open ? "open" : "closed", s.market_open ? "ok" : "warn"],
    ["Gemini", s.gemini_ready ? "ready" : (s.gemini_has_key ? "init fail" : "no key"), s.gemini_ready ? "ok" : "warn"],
    ["Scripts", s.scripts, "info"],
    ["Model", s.gemini_model, "info"],
  ];
  el.innerHTML = items.map(([k, v, c]) => `<span class="chip ${c}">${k}: ${v}</span>`).join("");
  document.getElementById("mode-badge").textContent = s.robinhood_mode === "real" ? "live" : "simulated";
  document.getElementById("mode-badge").className = "badge " + (s.robinhood_mode === "real" ? "ok" : "warn");
  const banner = document.getElementById("market-banner");
  if (s.market_open) {
    banner.textContent = "Market is OPEN — prices every 5 min, holdings every 30 min";
    banner.className = "banner open";
  } else {
    banner.textContent = "Market is CLOSED — no polling until next trading session. Use Refresh to fetch manually.";
    banner.className = "banner closed";
  }
}

function renderPortfolio(p) {
  if (!p) return;
  document.getElementById("total-value").textContent = usd.format(p.total_market_value);
  document.getElementById("total-cost").textContent = usd.format(p.total_cost);
  document.getElementById("holding-count").textContent = (p.holding_count ?? p.holdings?.length ?? 0) + " positions";
  const pl = document.getElementById("total-pl");
  pl.textContent = usd.format(p.total_unrealized_pl);
  pl.className = "value " + cls(p.total_unrealized_pl);
  const plpct = document.getElementById("total-pl-pct");
  plpct.textContent = pct(p.total_unrealized_pl_pct);
  plpct.className = "sub " + cls(p.total_unrealized_pl_pct);
  const day = document.getElementById("day-pl");
  day.textContent = usd.format(p.day_pl);
  day.className = "value " + cls(p.day_pl);
  const daySub = document.getElementById("day-pl-sub");
  daySub.textContent = "as of " + new Date(p.updated_at).toLocaleTimeString();
  daySub.className = "sub " + cls(p.day_pl);

  document.getElementById("buying-power").textContent = usd.format(p.buying_power ?? 0);
  document.getElementById("cash-sub").textContent = "cash " + usd.format(p.cash ?? 0);
  document.getElementById("unsettled-funds").textContent = usd.format(p.unsettled_funds ?? 0);

  const tbody = document.querySelector("#holdings tbody");
  tbody.innerHTML = (p.holdings || []).map(h => `
    <tr>
      <td class="sym">${h.symbol}</td>
      <td>${h.name}</td>
      <td class="num">${h.quantity}</td>
      <td class="num">${usd.format(h.avg_cost)}</td>
      <td class="num">${usd.format(h.current_price)}</td>
      <td class="num">${usd.format(h.market_value)}</td>
      <td class="num ${cls(h.day_pl_pct)}">${pct(h.day_pl_pct)}</td>
      <td class="num ${cls(h.unrealized_pl)}">${usd.format(h.unrealized_pl)}</td>
      <td class="num ${cls(h.unrealized_pl_pct)}">${pct(h.unrealized_pl_pct)}</td>
      <td class="num">${pct(h.equity_pct)}</td>
    </tr>`).join("");

  const optTbody = document.querySelector("#options tbody");
  const opts = p.options || [];
  optTbody.innerHTML = opts.map(o => {
    const exp = o.expiration ? new Date(o.expiration).toLocaleDateString() : "—";
    const typeCls = o.covered ? "cov" : (o.direction === "short" ? "neg" : "pos");
    return `<tr>
      <td class="sym">${o.symbol}</td>
      <td class="${typeCls}">${o.option_type}</td>
      <td class="num">${usd.format(o.strike)}</td>
      <td>${exp}</td>
      <td class="num">${o.quantity}</td>
      <td class="num">${usd.format(o.avg_cost)}</td>
      <td class="num">${usd.format(o.current_price)}</td>
      <td class="num">${usd.format(o.market_value)}</td>
      <td class="num ${cls(o.unrealized_pl)}">${usd.format(o.unrealized_pl)}</td>
      <td class="num ${cls(o.unrealized_pl_pct)}">${pct(o.unrealized_pl_pct)}</td>
    </tr>`;
  }).join("");
  document.getElementById("option-count").textContent = opts.length;
  document.getElementById("no-options").style.display = opts.length ? "none" : "block";

  const wl = p.watchlist || [];
  document.querySelector("#watchlist-table tbody").innerHTML = wl.map(w => `
    <tr>
      <td class="sym">${w.symbol}</td>
      <td>${w.name}</td>
      <td class="num">${usd.format(w.current_price ?? 0)}</td>
    </tr>`).join("");
  document.getElementById("watchlist-count").textContent = wl.length;
  document.getElementById("no-watchlist").style.display = wl.length ? "none" : "block";
}

function renderEvent(d) {
  const li = document.createElement("li");
  li.className = "event " + (d.severity || "info");
  const time = new Date(d.ts).toLocaleTimeString();
  const llm = d.llm ? `<div class="llm"><b>${d.llm.action || ""}</b> ${d.llm.message || ""}</div>` : "";
  li.innerHTML = `<div class="ev-head"><span class="ev-kind">${d.kind}${d.script ? " / " + d.script : ""}</span><span class="ev-time">${time}</span></div>
    <div class="ev-msg">${d.symbol ? "[" + d.symbol + "] " : ""}${d.message || d.payload?.message || ""}</div>${llm}`;
  const list = document.getElementById("events");
  list.prepend(li);
  while (list.children.length > 50) list.removeChild(list.lastChild);
}

function renderScripts(scripts) {
  const list = document.getElementById("scripts");
  list.innerHTML = (scripts || []).map(s => `<li><b>${s.name}</b> <span class="muted">${(s.targets || []).join(", ") || "all"}</span></li>`).join("");
}

async function getJSON(url) { const r = await fetch(url); if (!r.ok) throw new Error(await r.text()); return r.json(); }

async function refresh() {
  try {
    const [s, p, e, sc] = await Promise.all([
      getJSON("/api/status"), getJSON("/api/portfolio"), getJSON("/api/events"), getJSON("/api/scripts"),
    ]);
    setStatus(s); renderPortfolio(p); renderScripts(sc);
    document.getElementById("events").innerHTML = "";
    e.reverse().forEach(renderEvent);
    loadChart();
  } catch (err) { console.error(err); }
}

function stream() {
  const es = new EventSource("/api/stream");
  es.onmessage = (m) => {
    const d = JSON.parse(m.data);
    if (d.kind === "portfolio") renderPortfolio(d.payload);
    else renderEvent(d);
  };
  es.onerror = () => { /* browser auto-reconnects */ };
}

document.getElementById("btn-gemini-key").onclick = async () => {
  const key = document.getElementById("gemini-key").value.trim();
  if (!key) return;
  const btn = document.getElementById("btn-gemini-key");
  btn.disabled = true; btn.textContent = "Saving…";
  try {
    const r = await fetch("/api/auth/gemini", {
      method: "POST", headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ api_key: key }),
    });
    const j = await r.json();
    if (!r.ok) alert("Failed: " + (j.detail || JSON.stringify(j)));
    document.getElementById("gemini-key").value = "";
  } catch (e) { alert(e); }
  btn.disabled = false; btn.textContent = "Set Gemini Key";
  refresh();
};

document.getElementById("btn-robinhood").onclick = async () => {
  const btn = document.getElementById("btn-robinhood");
  btn.disabled = true; btn.textContent = "Opening browser…";
  try {
    const r = await fetch("/api/auth/robinhood", { method: "POST" });
    const j = await r.json();
    if (!r.ok) alert("Login failed: " + (j.detail || JSON.stringify(j)));
  } catch (e) { alert(e); }
  btn.disabled = false; btn.textContent = "Connect Robinhood";
  refresh();
};

document.getElementById("btn-strategy").onclick = async () => {
  const r = await fetch("/api/strategy/refresh", { method: "POST" });
  const j = await r.json();
  if (!r.ok) alert("Strategy refresh failed: " + (j.detail || JSON.stringify(j)));
  refresh();
};

document.getElementById("btn-refresh").onclick = async () => {
  const btn = document.getElementById("btn-refresh");
  btn.disabled = true; btn.textContent = "Refreshing…";
  try {
    const r = await fetch("/api/refresh", { method: "POST" });
    const j = await r.json();
    if (!r.ok) alert("Refresh failed: " + (j.detail || JSON.stringify(j)));
    else if (j.portfolio) renderPortfolio(j.portfolio);
  } catch (e) { alert(e); }
  btn.disabled = false; btn.textContent = "Refresh";
  refresh();
};

document.getElementById("btn-logout").onclick = async () => {
  if (!confirm("Logout and invalidate all auth? You'll need to re-connect Gemini and Robinhood.")) return;
  const btn = document.getElementById("btn-logout");
  btn.disabled = true; btn.textContent = "Logging out…";
  try {
    const r = await fetch("/api/auth/logout", { method: "POST" });
    const j = await r.json();
    if (!r.ok) alert("Logout failed: " + (j.detail || JSON.stringify(j)));
  } catch (e) { alert(e); }
  btn.disabled = false; btn.textContent = "Logout All";
  refresh();
};

// --- Chat sidebar ---

function addChatMsg(role, text) {
  const container = document.getElementById("chat-messages");
  const div = document.createElement("div");
  div.className = `msg ${role}`;
  if (role === "ai") {
    div.innerHTML = marked.parse(text);
  } else {
    div.textContent = text;
  }
  container.appendChild(div);
  container.scrollTop = container.scrollHeight;
  return div;
}

document.getElementById("btn-ask").onclick = sendChat;
document.getElementById("ask-input").addEventListener("keydown", (e) => {
  if (e.key === "Enter" && !e.shiftKey) { e.preventDefault(); sendChat(); }
});

async function sendChat() {
  const input = document.getElementById("ask-input");
  const prompt = input.value.trim();
  if (!prompt) return;
  input.value = "";
  addChatMsg("user", prompt);
  const thinking = addChatMsg("ai", "_thinking…_");
  thinking.classList.add("msg-thinking");
  try {
    const r = await fetch("/api/llm/ask", {
      method: "POST", headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ prompt }),
    });
    const j = await r.json();
    thinking.classList.remove("msg-thinking");
    if (r.ok) {
      thinking.innerHTML = marked.parse(j.answer || "(no answer)");
    } else {
      thinking.innerHTML = `<span class="msg-error">error: ${j.detail || JSON.stringify(j)}</span>`;
    }
  } catch (e) {
    thinking.classList.remove("msg-thinking");
    thinking.innerHTML = `<span class="msg-error">error: ${e}</span>`;
  }
  const container = document.getElementById("chat-messages");
  container.scrollTop = container.scrollHeight;
}

document.getElementById("btn-clear-chat").onclick = async () => {
  document.getElementById("chat-messages").innerHTML = "";
  try { await fetch("/api/chat/clear", { method: "POST" }); } catch (e) {}
};

// --- Tabs ---
const allTabs = [
  { btn: "tab-chat", panel: "tab-panel-chat" },
  { btn: "tab-script", panel: "tab-panel-script" },
  { btn: "tab-activity", panel: "tab-panel-activity" },
];

function switchTab(activeId) {
  for (const t of allTabs) {
    const btn = document.getElementById(t.btn);
    const panel = document.getElementById(t.panel);
    if (t.btn === activeId) {
      btn.classList.add("active"); panel.classList.add("active");
    } else {
      btn.classList.remove("active"); panel.classList.remove("active");
    }
  }
}

document.getElementById("tab-chat").onclick = () => switchTab("tab-chat");
document.getElementById("tab-script").onclick = () => { switchTab("tab-script"); loadScriptList(); };
document.getElementById("tab-activity").onclick = () => { switchTab("tab-activity"); loadActivity(); };

// --- Activity tab ---
async function loadActivity() {
  const list = document.getElementById("activity-list");
  try {
    const [txnData, events] = await Promise.all([
      getJSON("/api/transactions"), getJSON("/api/events?limit=30"),
    ]);
    const txns = txnData.transactions || [];
    const items = [];
    for (const t of txns) {
      const time = t.executed_at ? new Date(t.executed_at).toLocaleString() : "";
      const display = (t.raw_side || t.side || "").toUpperCase();
      const type = (t.type || "").toUpperCase();
      const cls = display === "BUY" ? "buy" : display === "SELL" ? "sell" : display === "DEBIT" ? "debit" : display === "CREDIT" ? "credit" : "";
      const label = display && type ? display + " " + type : display || type || "—";
      items.push({
        ts: t.executed_at || "",
        html: `<div class="act-row">
          <span class="act-type ${cls}">${label}</span>
          <span class="act-sym">${t.symbol}</span>
          <span class="act-qty">${t.quantity ? t.quantity + " sh" : ""}</span>
          <span class="act-price">${t.price ? usd.format(t.price) : ""}</span>
          <span class="act-total">${t.total ? usd.format(t.total) : ""}</span>
          <span class="act-status">${t.status || ""}</span>
          <span class="act-time">${time}</span>
        </div>`,
      });
    }
    for (const e of events) {
      if (e.kind === "portfolio") continue;
      const time = e.ts ? new Date(e.ts).toLocaleString() : "";
      items.push({
        ts: e.ts || "",
        html: `<div class="act-row event ${e.severity || "info"}">
          <span class="act-type">${e.kind}</span>
          <span class="act-sym">${e.symbol || ""}</span>
          <span class="act-msg">${e.message || ""}</span>
          <span class="act-time">${time}</span>
        </div>`,
      });
    }
    items.sort((a, b) => b.ts.localeCompare(a.ts));
    list.innerHTML = items.length
      ? items.slice(0, 30).map(i => i.html).join("")
      : '<p class="muted">No recent activity.</p>';
  } catch (e) {
    list.innerHTML = `<p class="muted">Failed to load activity: ${e}</p>`;
  }
}

document.getElementById("btn-refresh-activity").onclick = () => loadActivity();

// --- Script editor ---
let currentScripts = [];

async function loadScriptList() {
  try {
    const prevName = document.getElementById("script-select").value;
    currentScripts = await getJSON("/api/scripts");
    const sel = document.getElementById("script-select");
    sel.innerHTML = currentScripts.map(s => `<option value="${s.name}">${s.name} (${(s.targets||[]).join(",")||"all"})</option>`).join("");
    if (currentScripts.length > 0) {
      if (prevName && currentScripts.some(s => s.name === prevName)) {
        sel.value = prevName;
        const s = currentScripts.find(x => x.name === prevName);
        if (s && s.source !== document.getElementById("script-editor").value) {
          loadScriptIntoEditor(s);
        }
      } else {
        sel.selectedIndex = 0;
        loadScriptIntoEditor(currentScripts[0]);
      }
    } else {
      document.getElementById("script-editor").value = "";
    }
  } catch (e) { console.error(e); }
}

document.getElementById("script-select").onchange = () => {
  const name = document.getElementById("script-select").value;
  const s = currentScripts.find(x => x.name === name);
  if (s) loadScriptIntoEditor(s);
};

function loadScriptIntoEditor(s) {
  document.getElementById("script-editor").value = s.source || "";
  document.getElementById("script-status").textContent = "";
  document.getElementById("script-status").className = "script-status";
}

document.getElementById("btn-save-script").onclick = async () => {
  const name = document.getElementById("script-select").value;
  if (!name) return;
  const source = document.getElementById("script-editor").value;
  const status = document.getElementById("script-status");
  status.textContent = "compiling…";
  status.className = "script-status";
  try {
    const r = await fetch(`/api/scripts/${encodeURIComponent(name)}`, {
      method: "PUT", headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ source }),
    });
    const j = await r.json();
    if (j.status === "ok") {
      status.textContent = `✓ ${j.name} compiled OK (targets: ${(j.targets||[]).join(", ")||"all"})`;
      status.className = "script-status ok";
      await loadScriptList();
      document.getElementById("script-select").value = name;
    } else {
      status.textContent = `✗ ${j.error}`;
      status.className = "script-status error";
    }
  } catch (e) {
    status.textContent = `✗ ${e}`;
    status.className = "script-status error";
  }
};

document.getElementById("btn-delete-script").onclick = async () => {
  const name = document.getElementById("script-select").value;
  if (!name || !confirm(`Delete script '${name}'?`)) return;
  try {
    await fetch(`/api/scripts/${encodeURIComponent(name)}`, { method: "DELETE" });
    loadScriptList();
  } catch (e) { alert(e); }
};

document.getElementById("btn-new-script").onclick = () => {
  const name = prompt("Script name (e.g. stop_loss_aapl):");
  if (!name) return;
  const template = "TARGETS = []\n\ndef check(ctx):\n    # ctx keys: symbol, name, quantity, avg_cost,\n    # current_price, prev_close, day_high, day_low,\n    # market_value, unrealized_pl, unrealized_pl_pct,\n    # equity_pct, history, as_of, portfolio, state\n    #\n    # state: persistent dict saved to disk between polls.\n    # Use it to track things like high_water_mark:\n    #   hi = ctx['state'].get('high', 0)\n    #   ctx['state']['high'] = max(hi, ctx['current_price'])\n    return None\n";
  document.getElementById("script-select").innerHTML += `<option value="${name}">${name} (new)</option>`;
  const sel = document.getElementById("script-select");
  sel.value = name;
  document.getElementById("script-editor").value = template;
  document.getElementById("script-status").textContent = "Click Save & Compile to create";
  document.getElementById("script-status").className = "script-status";
};

// --- Theme toggle ---
const themeBtn = document.getElementById("theme-toggle");
if (localStorage.getItem("robin-theme") === "light") {
  document.body.classList.add("light");
  themeBtn.textContent = "☀";
}
themeBtn.onclick = () => {
  const isLight = document.body.classList.toggle("light");
  themeBtn.textContent = isLight ? "☀" : "🌙";
  localStorage.setItem("robin-theme", isLight ? "light" : "dark");
};

// --- Sidebar resizer ---
(function () {
  const resizer = document.getElementById("sidebar-resizer");
  const sidebar = document.querySelector(".chat-sidebar");
  if (!resizer || !sidebar) return;

  const saved = localStorage.getItem("robin-sidebar-w");
  if (saved) sidebar.style.width = saved + "px";

  let dragging = false;
  let startX = 0;
  let startW = 0;

  resizer.addEventListener("mousedown", (e) => {
    dragging = true;
    startX = e.clientX;
    startW = sidebar.getBoundingClientRect().width;
    resizer.classList.add("dragging");
    document.body.style.cursor = "col-resize";
    document.body.style.userSelect = "none";
    e.preventDefault();
  });

  document.addEventListener("mousemove", (e) => {
    if (!dragging) return;
    const dx = startX - e.clientX;
    const minW = parseInt(getComputedStyle(sidebar).minWidth) || 280;
    const maxW = window.innerWidth * 0.7;
    const newW = Math.min(Math.max(startW + dx, minW), maxW);
    sidebar.style.width = newW + "px";
  });

  document.addEventListener("mouseup", () => {
    if (!dragging) return;
    dragging = false;
    resizer.classList.remove("dragging");
    document.body.style.cursor = "";
    document.body.style.userSelect = "";
    localStorage.setItem("robin-sidebar-w", Math.round(sidebar.getBoundingClientRect().width));
  });
})();

refresh();
stream();
setInterval(refresh, 15000);
