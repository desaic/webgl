// On-screen console for phones, enabled by adding ?debug=1 to the URL.
// Mirrors console.* and uncaught errors into a small panel at the bottom of the page.

export const debugEnabled = new URLSearchParams(location.search).has("debug");

const MAX_LINES = 300;

export function initDebugConsole() {
  if (!debugEnabled) return;

  const panel = document.createElement("div");
  panel.id = "dbgPanel";

  const bar = document.createElement("div");
  bar.id = "dbgBar";
  const title = document.createElement("span");
  title.textContent = "debug";
  const toggle = document.createElement("button");
  toggle.textContent = "hide";
  const clear = document.createElement("button");
  clear.textContent = "clear";
  const copy = document.createElement("button");
  copy.textContent = "copy";
  bar.append(title, copy, clear, toggle);

  const log = document.createElement("div");
  log.id = "dbgLog";
  panel.append(bar, log);
  document.body.appendChild(panel);

  toggle.addEventListener("click", () => {
    const hidden = panel.classList.toggle("collapsed");
    toggle.textContent = hidden ? "show" : "hide";
  });
  clear.addEventListener("click", () => log.replaceChildren());
  copy.addEventListener("click", () => {
    navigator.clipboard?.writeText(log.innerText).catch(() => {});
  });

  const add = (level: string, args: unknown[]) => {
    const line = document.createElement("div");
    line.className = "dbg-" + level;
    const t = (performance.now() / 1000).toFixed(3);
    line.textContent = t + " " + args.map(fmt).join(" ");
    log.appendChild(line);
    while (log.childElementCount > MAX_LINES) log.firstElementChild!.remove();
    log.scrollTop = log.scrollHeight;
  };

  for (const level of ["log", "info", "warn", "error", "debug"] as const) {
    const orig = console[level].bind(console);
    console[level] = (...args: unknown[]) => {
      orig(...args);
      add(level, args);
    };
  }
  window.addEventListener("error", (e) => add("error", [e.message, e.filename + ":" + e.lineno]));
  window.addEventListener("unhandledrejection", (e) => add("error", ["unhandled rejection", e.reason]));

  console.log("debug console on", navigator.userAgent, "dpr", devicePixelRatio);
}

function fmt(v: unknown): string {
  if (typeof v === "string") return v;
  if (v instanceof Error) return v.stack || v.message;
  try {
    return JSON.stringify(v);
  } catch {
    return String(v);
  }
}
