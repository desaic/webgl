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
    return `<tr>
      <td class="sym">${o.symbol}</td>
      <td>${o.option_type}</td>
      <td class="num">${usd.format(o.strike)}</td>
      <td>${exp}</td>
      <td class="num">${o.quantity}</td>
      <td class="num">${usd.format(o.avg_cost)}</td>
      <td class="num">${usd.format(o.current_price)}</td>
      <td class="num">${usd.format(o.market_value)}</td>
      <td class="num ${cls(o.unrealized_pl)}">${usd.format(o.unrealized_pl)}</td>
      <td class="num ${cls(o.unrealized_pl_pct)}">${pct(o.unrealized_pl_pct)}</td>
      <td class="num ${cls(o.intraday_pl)}">${usd.format(o.intraday_pl)}</td>
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
  if (!confirm("Logout and invalidate all auth? You'll need to re-connect Google and Robinhood.")) return;
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

document.getElementById("btn-ask").onclick = async () => {
  const prompt = document.getElementById("ask-input").value.trim();
  if (!prompt) return;
  const out = document.getElementById("ask-output");
  out.textContent = "thinking…";
  const r = await fetch("/api/llm/ask", {
    method: "POST", headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ prompt }),
  });
  const j = await r.json();
  out.textContent = r.ok ? (j.answer || "(no answer)") : "error: " + (j.detail || JSON.stringify(j));
};

refresh();
stream();
setInterval(refresh, 15000);
