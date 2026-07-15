const usd = new Intl.NumberFormat("en-US", { style: "currency", currency: "USD" });
const pct = (v) => (v == null || isNaN(v) ? "—" : (v >= 0 ? "+" : "") + v.toFixed(2) + "%");

function cls(v) { return v == null ? "" : v > 0 ? "pos" : v < 0 ? "neg" : ""; }

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
  const username = prompt("Robinhood username:");
  if (!username) return;
  const password = prompt("Robinhood password:");
  if (!password) return;
  const mfa_code = prompt("MFA code (leave blank if none):") || null;
  const btn = document.getElementById("btn-robinhood");
  btn.disabled = true; btn.textContent = "Connecting… check your Robinhood app if prompted";
  try {
    const r = await fetch("/api/auth/robinhood", {
      method: "POST", headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ username, password, mfa_code }),
    });
    const j = await r.json();
    if (j.status === "mfa_required") alert("MFA required — enter the code and retry.");
    else if (!r.ok) alert("Login failed: " + (j.detail || JSON.stringify(j)));
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
const tabChat = document.getElementById("tab-chat");
const tabScript = document.getElementById("tab-script");
const panelChat = document.getElementById("tab-panel-chat");
const panelScript = document.getElementById("tab-panel-script");

tabChat.onclick = () => {
  tabChat.classList.add("active"); tabScript.classList.remove("active");
  panelChat.classList.add("active"); panelScript.classList.remove("active");
};
tabScript.onclick = () => {
  tabScript.classList.add("active"); tabChat.classList.remove("active");
  panelScript.classList.add("active"); panelChat.classList.remove("active");
  loadScriptList();
};

// --- Script editor ---
let currentScripts = [];

async function loadScriptList() {
  try {
    currentScripts = await getJSON("/api/scripts");
    const sel = document.getElementById("script-select");
    sel.innerHTML = currentScripts.map(s => `<option value="${s.name}">${s.name} (${(s.targets||[]).join(",")||"all"})</option>`).join("");
    if (currentScripts.length > 0) {
      sel.selectedIndex = 0;
      loadScriptIntoEditor(currentScripts[0]);
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
      loadScriptList();
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

refresh();
stream();
setInterval(refresh, 15000);
