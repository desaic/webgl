import * as api from "./api";
import { Inst, Pose, isTraj, parseNudge, parsePack, parseTraj } from "./parse";
import { Item, Viewer, labelOf } from "./scene";

const $ = <T extends HTMLElement>(id: string) => document.getElementById(id) as T;
const viewer = new Viewer($("view"));
(window as any).viewer = viewer;

const DEFAULT_ROOT = "F:/meshes/fruit_hand";
const state = {
  dir: DEFAULT_ROOT,
  meshDir: DEFAULT_ROOT + "/fruits_1",
  pack: "",
  containers: [] as string[],
  packMtime: 0,
  insts: [] as Inst[],
};

const statusHistory: string[] = [];
function logStatus(msg: string) {
  if (!msg) return;
  const t = new Date().toLocaleTimeString([], { hour12: false, hour: "2-digit", minute: "2-digit", second: "2-digit" });
  statusHistory.push(`[${t}] ${msg}`);
  if (statusHistory.length > 20) statusHistory.shift();
  const box = document.getElementById("statusLog");
  if (box) {
    box.textContent = statusHistory.join("\n");
    box.scrollTop = box.scrollHeight;
  }
}

function status(msg: string) {
  $("status").textContent = msg;
  logStatus(msg);
}

const baseName = (p: string) => p.split("/").pop() || p;

// ---- file browser ----
async function browse(dir: string) {
  try {
    const l = await api.list(dir);
    state.dir = l.path;
    $<HTMLInputElement>("dir").value = l.path;
    const box = $("files");
    box.textContent = "";
    $<HTMLButtonElement>("dirUp").disabled = !l.parent;
    $("dirUp").dataset.parent = l.parent || "";
    const rows = l.entries
      .filter((e) => e.dir || /\.(txt|obj|stl)$/i.test(e.name))
      .sort((a, b) => (a.dir !== b.dir ? (a.dir ? -1 : 1) : b.mtime - a.mtime));
    for (const e of rows) {
      const d = document.createElement("div");
      d.className = "item";
      const n = document.createElement("span");
      n.textContent = (e.dir ? "[+] " : "") + e.name;
      const m = document.createElement("span");
      m.className = "meta";
      m.textContent = e.dir ? "" : fmtSize(e.size);
      d.append(n, m);
      const full = l.path + "/" + e.name;
      if (full === $<HTMLInputElement>("contPath").value) d.classList.add("picked");
      d.onclick = () => {
        if (e.dir) browse(full);
        else if (/\.txt$/i.test(e.name)) loadPackFile(full);
        else {
          $<HTMLInputElement>("contPath").value = full;
          box.querySelectorAll(".picked").forEach((x) => x.classList.remove("picked"));
          d.classList.add("picked");
        }
      };
      box.appendChild(d);
    }
    status("");
  } catch (err: any) {
    status(String(err.message || err));
  }
}

function fmtSize(n: number): string {
  if (n > 1e6) return (n / 1e6).toFixed(1) + " MB";
  if (n > 1e3) return (n / 1e3).toFixed(0) + " KB";
  return n + " B";
}

// ---- loading ----
async function loadContainer(path: string, replace: boolean) {
  status("loading " + baseName(path));
  try {
    await viewer.addContainer(path, replace);
    if (replace) state.containers = [path];
    else if (!state.containers.includes(path)) state.containers.push(path);
    renderContainers();
    if (firstView) viewer.fit();
    firstView = false;
    status("");
    syncHash();
  } catch (err: any) {
    status(String(err.message || err));
  }
}

function renderContainers() {
  const box = $("contList");
  box.textContent = "";
  state.containers.forEach((c, i) => {
    const d = document.createElement("div");
    d.className = "item";
    d.title = c;
    const n = document.createElement("span");
    n.textContent = baseName(c);
    const x = document.createElement("span");
    x.className = "meta";
    x.textContent = "remove";
    x.onclick = () => {
      state.containers.splice(i, 1);
      viewer.containers.remove(viewer.containers.children[i]);
      viewer.redraw();
      renderContainers();
      syncHash();
    };
    d.append(n, x);
    box.appendChild(d);
  });
}

$("contSet").onclick = () => {
  const p = $<HTMLInputElement>("contPath").value.trim();
  if (p) loadContainer(p, true);
};
$("contAdd").onclick = () => {
  const p = $<HTMLInputElement>("contPath").value.trim();
  if (p) loadContainer(p, false);
};

// fit the camera only on the very first load; later loads keep the user's view
let firstView = true;

async function loadPackFile(path: string, keepCamera = false) {
  status("loading " + baseName(path));
  try {
    const text = await api.fetchText(path);
    const traj = isTraj(text);
    const insts = traj ? parseTraj(text) : parsePack(text);
    state.pack = path;
    state.packMtime = await api.mtime(path);
    state.insts = insts;
    nudgeTrails = new Map();
    if (!traj) {
      try {
        nudgeTrails = parseNudge(await api.fetchText(path + ".traj"));
        logStatus(`nudge: loaded ${nudgeTrails.size} trails from ${baseName(path)}.traj`);
      } catch {
        logStatus(`nudge: no ${baseName(path)}.traj`);
      }
    }
    $("packName").textContent = baseName(path) + (traj ? " (traj)" : "");
    const pathFor = (i: Inst) => i.meshPath || state.meshDir + "/" + i.name + ".obj";
    const failed = await viewer.setInstances(insts, pathFor, (n, t) => status("meshes " + n + "/" + t));
    status(failed.length ? "missing: " + failed.map(baseName).join(", ") : "");
    const step = $<HTMLInputElement>("step");
    step.max = String(viewer.maxStep);
    step.value = String(viewer.step);
    $("trajSec").hidden = viewer.maxStep === 0;
    $("stepLbl").textContent = viewer.step + " / " + viewer.maxStep;
    if (!keepCamera && firstView) viewer.fit();
    firstView = false;
    buildList();
    viewer.applyMaterials();
    $("count").textContent = "(" + viewer.items.length + ")";
    syncHash();
  } catch (err: any) {
    status(String(err.message || err));
  }
}

// ---- instance list ----
function buildList() {
  const box = $("list");
  box.textContent = "";
  const f = $<HTMLInputElement>("filter").value.toLowerCase();
  for (const it of viewer.items) {
    const label = labelOf(it.inst);
    if (f && !label.toLowerCase().includes(f)) continue;
    const d = document.createElement("div");
    d.className = "item" + (it === viewer.selected ? " sel" : "") + (it.visible ? "" : " off");
    d.dataset.idx = String(viewer.items.indexOf(it));
    const n = document.createElement("span");
    const dot = document.createElement("span");
    dot.className = "dot";
    dot.style.background = "#" + it.color.getHexString();
    n.append(dot, label);
    const eye = document.createElement("span");
    eye.className = "meta";
    eye.textContent = it.visible ? "hide" : "show";
    eye.onclick = (ev) => {
      ev.stopPropagation();
      viewer.setVisible(it, !it.visible);
      syncHideBtn();
      buildList();
    };
    d.append(n, eye);
    d.onclick = () => viewer.select(it);
    d.ondblclick = () => viewer.focus(it);
    box.appendChild(d);
  }
}

// ---- nudge playback of the selected fruit ----
let nudgeTrails = new Map<number, Pose[]>();
let nudgeTimer = 0;
let nudgeItem: Item | null = null;
function nudgePoses(): Pose[] | null {
  const it = viewer.selected;
  return it ? nudgeTrails.get(viewer.items.indexOf(it)) || null : null;
}
function stopNudge(reason?: string) {
  if (nudgeTimer && reason) logStatus(`nudge: stopped (${reason})`);
  clearInterval(nudgeTimer);
  nudgeTimer = 0;
  $("nudgePlay").textContent = "Play nudge";
}
function setNudgeStep(s: number) {
  const poses = nudgePoses();
  if (!poses || !viewer.selected) return;
  s = Math.max(0, Math.min(poses.length - 1, s));
  viewer.showPose(viewer.selected, poses[s]);
  $<HTMLInputElement>("nudgeStep").value = String(s);
  $("nudgeLbl").textContent = s + " / " + (poses.length - 1);
}
function syncNudgeUi() {
  stopNudge();
  const poses = nudgePoses();
  $("nudgeRow").hidden = !poses;
  if (!poses) return;
  $<HTMLInputElement>("nudgeStep").max = String(poses.length - 1);
  // idle: show the final pose
  $<HTMLInputElement>("nudgeStep").value = String(poses.length - 1);
  $("nudgeLbl").textContent = poses.length - 1 + " / " + (poses.length - 1);
}
$("nudgePlay").onclick = () => {
  if (nudgeTimer) return stopNudge("paused");
  if (!viewer.selected) return logStatus("nudge: no item selected");
  const idx = viewer.items.indexOf(viewer.selected);
  const poses = nudgePoses();
  const lbl = labelOf(viewer.selected.inst);
  if (!poses) return logStatus(`nudge: no trail for ${lbl} (idx=${idx})`);
  if (poses.length < 2) return logStatus(`nudge: ${lbl} has only ${poses.length} pose(s), nothing to play`);
  const p0 = poses[0].pos;
  const p1 = poses[poses.length - 1].pos;
  const dp = Math.hypot(p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]);
  nudgeItem = viewer.selected;
  $("nudgePlay").textContent = "Pause";
  let s = parseInt($<HTMLInputElement>("nudgeStep").value);
  if (s >= poses.length - 1) s = 0;
  logStatus(`nudge: playing ${lbl} (${poses.length} poses, start=${s}, Δpos=${dp.toFixed(3)})`);
  setNudgeStep(s);
  nudgeTimer = window.setInterval(() => {
    if (viewer.selected !== nudgeItem) return stopNudge("selection changed");
    s++;
    setNudgeStep(s);
    if (s >= poses.length - 1) stopNudge("finished");
  }, 40);
};
$("nudgeStep").oninput = (e) => {
  stopNudge();
  setNudgeStep(parseInt((e.target as HTMLInputElement).value));
};

viewer.onPick = (it: Item | null) => {
  syncNudgeUi();
  syncHideBtn();
  const el = $("selInfo");
  if (!it) {
    el.textContent = "click an object";
  } else {
    const p = it.inst.poses[Math.min(viewer.step, it.inst.poses.length - 1)];
    el.textContent =
      labelOf(it.inst) + "  pos " + p.pos.map((v) => v.toFixed(2)).join(" ") + "  scale " + p.scale;
  }
  buildList();
  syncHash();
};

// ---- fruit library (spawn new fruit) ----
interface Fruit {
  name: string; // without .obj
  file: string;
  group: number; // -1 when unknown
  diam: number; // cm, 0 when unknown
}
let fruits: Fruit[] = [];
let groupLabels: string[] = [];

// size_groups.txt: "group N label (diameter ...)" headers, then "name.obj diameter" rows
function parseSizeGroups(text: string, into: Map<string, { group: number; diam: number }>, labels: string[]) {
  let g = -1;
  for (const raw of text.split(/\r?\n/)) {
    const line = raw.trim();
    const h = /^group\s+(\d+)\s+(.*?)\s*\(([^)]*)\)/.exec(line);
    if (h) {
      g = parseInt(h[1]);
      labels[g] = g + " " + h[2] + " (" + h[3] + ")";
      continue;
    }
    const m = /^(\S+\.obj)\s+([\d.]+)$/i.exec(line);
    if (m && g >= 0) into.set(m[1], { group: g, diam: parseFloat(m[2]) });
  }
}

async function refreshFruits() {
  try {
    const l = await api.list(state.meshDir);
    const meta = new Map<string, { group: number; diam: number }>();
    groupLabels = [];
    try {
      parseSizeGroups(await api.fetchText(state.meshDir + "/size_groups.txt"), meta, groupLabels);
    } catch {}
    fruits = l.entries
      .filter((e) => !e.dir && /\.obj$/i.test(e.name))
      .map((e) => {
        const m = meta.get(e.name);
        return { name: e.name.replace(/\.obj$/i, ""), file: e.name, group: m ? m.group : -1, diam: m ? m.diam : 0 };
      })
      .sort((a, b) => a.group - b.group || b.diam - a.diam || a.name.localeCompare(b.name));
    const sel = $<HTMLSelectElement>("fruitGroup");
    const cur = sel.value;
    sel.textContent = "";
    sel.add(new Option("all sizes", ""));
    groupLabels.forEach((t, i) => t && sel.add(new Option(t, String(i))));
    if (fruits.some((f) => f.group < 0)) sel.add(new Option("unknown", "-1"));
    sel.value = [...sel.options].some((o) => o.value === cur) ? cur : "";
  } catch (err: any) {
    fruits = [];
    status("fruits dir: " + String(err.message || err));
  }
  buildFruitList();
}

function buildFruitList() {
  const box = $("fruitList");
  box.textContent = "";
  const f = $<HTMLInputElement>("fruitFilter").value.toLowerCase();
  const g = $<HTMLSelectElement>("fruitGroup").value;
  let shown = 0;
  let lastGroup = -2;
  for (const fr of fruits) {
    if (f && !fr.name.toLowerCase().includes(f)) continue;
    if (g !== "" && String(fr.group) !== g) continue;
    if (fr.group !== lastGroup) {
      lastGroup = fr.group;
      const h = document.createElement("div");
      h.className = "grp";
      h.textContent = fr.group >= 0 ? groupLabels[fr.group] || "group " + fr.group : "unknown size";
      box.appendChild(h);
    }
    const d = document.createElement("div");
    d.className = "item";
    const n = document.createElement("span");
    n.textContent = fr.name;
    const m = document.createElement("span");
    m.className = "meta";
    m.textContent = fr.diam ? fr.diam.toFixed(1) + " cm" : "";
    d.append(n, m);
    d.onclick = () => spawnFruit(fr);
    box.appendChild(d);
    shown++;
  }
  $("fruitCount").textContent = "(" + shown + ")";
}

async function spawnFruit(fr: Fruit) {
  status("spawning " + fr.name);
  try {
    await viewer.spawn(fr.name, state.meshDir + "/" + fr.file, viewer.controls.target.clone());
    viewer.applyMaterials();
    buildList();
    $("count").textContent = "(" + viewer.items.length + ")";
    status("");
  } catch (err: any) {
    status(String(err.message || err));
  }
}

function showTab(inst: boolean) {
  $("paneInst").hidden = !inst;
  $("paneFruit").hidden = inst;
  $("tabInst").classList.toggle("on", inst);
  $("tabFruit").classList.toggle("on", !inst);
}
$("tabInst").onclick = () => showTab(true);
$("tabFruit").onclick = () => showTab(false);
$("fruitFilter").oninput = buildFruitList;
$("fruitGroup").onchange = buildFruitList;

// ---- transform gizmo ----
function setMode(m: "translate" | "rotate") {
  viewer.setGizmoMode(m);
  $("modeMove").classList.toggle("on", m === "translate");
  $("modeRot").classList.toggle("on", m === "rotate");
}
$("floatMode").onclick = () => setMode(viewer.tc.mode === "translate" ? "rotate" : "translate");
$("modeMove").onclick = () => setMode("translate");
$("modeRot").onclick = () => setMode("rotate");
function deleteSelected() {
  if (!viewer.selected) return;
  viewer.removeItem(viewer.selected);
  $("count").textContent = "(" + viewer.items.length + ")";
  buildList();
}
$("delSel").onclick = deleteSelected;
function showCollisions() {
  const it = viewer.selected;
  if (!it) return;
  const list = viewer.contacts(it);
  const f = (v: number) => v.toFixed(2);
  $("collInfo").textContent =
    labelOf(it.inst) + ": " + (list.length ? "" : "no contacts") + "\n" +
    list.map((c) => c.label + "  " + (c.depth > 0 ? "penetration " + f(c.depth) + " cm" : "gap " + f(c.gap) + " cm")).join("\n");
}
$("collSel").onclick = showCollisions;
// download the current state as new_pack.txt
function savePack() {
  const a = document.createElement("a");
  a.href = URL.createObjectURL(new Blob([viewer.exportPack()], { type: "text/plain" }));
  a.download = "new_pack.txt";
  a.click();
  status("saved new_pack.txt (" + viewer.items.length + " fruit)");
}
$("exportPack").onclick = savePack;
$("floatSave").onclick = savePack;

async function copySelected() {
  if (!viewer.selected) return;
  viewer.copyItem(viewer.selected);
  viewer.applyMaterials();
  $("count").textContent = "(" + viewer.items.length + ")";
  buildList();
}
$("copySel").onclick = copySelected;
viewer.onEdit = (it: Item) => {
  const n = viewer.items.filter((i) => i.edited).length;
  $("editInfo").textContent = n + " edited";
  const p = it.inst.poses[Math.min(viewer.step, it.inst.poses.length - 1)];
  $("selInfo").textContent =
    labelOf(it.inst) + "  pos " + p.pos.map((v) => v.toFixed(2)).join(" ") + "  scale " + p.scale;
};

// ---- permalink: everything needed to reproduce what the user sees ----
let hashTimer = 0;
function syncHash() {
  clearTimeout(hashTimer);
  hashTimer = window.setTimeout(() => {
    const p = new URLSearchParams();
    if (state.pack) p.set("pack", state.pack);
    if (state.containers.length) p.set("cont", state.containers.join("|"));
    p.set("mesh", state.meshDir);
    if (viewer.maxStep) p.set("step", String(viewer.step));
    if (viewer.selected) p.set("sel", String(viewer.items.indexOf(viewer.selected)));
    if (viewer.clipOn) p.set("clip", ["x", "y", "z"][viewer.clipAxis] + (viewer.clipFlip ? "-" : "+") + viewer.clipValue.toFixed(3));
    p.set("cam", viewer.getCamera().join(","));
    history.replaceState(null, "", "#" + p.toString());
  }, 300);
}
viewer.onCamera = syncHash;

async function restoreFromHash() {
  const p = new URLSearchParams(location.hash.slice(1));
  if (p.get("mesh")) state.meshDir = p.get("mesh");
  $("meshDirLbl").textContent = state.meshDir;
  refreshFruits();
  const conts = (p.get("cont") || "").split("|").filter(Boolean);
  for (let i = 0; i < conts.length; i++) await loadContainer(conts[i], i === 0);
  if (p.get("pack")) {
    await loadPackFile(p.get("pack"), true);
    if (p.get("step")) setStep(parseInt(p.get("step")));
    if (p.get("sel")) viewer.select(viewer.items[parseInt(p.get("sel"))] || null);
  }
  const clip = p.get("clip");
  if (clip) {
    viewer.clipAxis = "xyz".indexOf(clip[0]);
    viewer.clipFlip = clip[1] === "-";
    viewer.clipValue = parseFloat(clip.slice(2));
    viewer.clipOn = true;
    $<HTMLInputElement>("clip").checked = true;
    $<HTMLSelectElement>("clipAxis").value = clip[0];
    $<HTMLInputElement>("clipPos").value = String(viewer.clipValue);
    viewer.updateClip();
  }
  const cam = p.get("cam");
  if (cam) viewer.setCamera(cam.split(",").map(Number));
  else viewer.fit();
  if (p.get("pack")) browse(dirOf(p.get("pack")));
  else if (conts.length) browse(dirOf(conts[0]));
  else browse(state.dir);
}

const dirOf = (p: string) => p.slice(0, p.lastIndexOf("/"));

// ---- trajectory ----
let playing = 0;
function setStep(s: number) {
  s = Math.max(0, Math.min(viewer.maxStep, s));
  viewer.setStep(s);
  $<HTMLInputElement>("step").value = String(s);
  $("stepLbl").textContent = s + " / " + viewer.maxStep;
  syncHash();
}
$("step").oninput = (e) => setStep(parseInt((e.target as HTMLInputElement).value));
$("play").onclick = () => {
  if (playing) {
    clearInterval(playing);
    playing = 0;
    $("play").textContent = "Play";
    return;
  }
  $("play").textContent = "Pause";
  playing = window.setInterval(() => setStep(viewer.step >= viewer.maxStep ? 0 : viewer.step + 1), 40);
};

// ---- panel wiring ----
$("dirGo").onclick = () => browse($<HTMLInputElement>("dir").value);
$("dir").onkeydown = (e) => e.key === "Enter" && browse($<HTMLInputElement>("dir").value);
$("dirUp").onclick = () => {
  const p = $("dirUp").dataset.parent;
  if (p) browse(p);
};
$("setMeshDir").onclick = async () => {
  state.meshDir = state.dir;
  $("meshDirLbl").textContent = state.meshDir;
  refreshFruits();
  viewer.clearGeometryCache();
  if (state.pack) await loadPackFile(state.pack, true);
  syncHash();
};
$("reload").onclick = () => {
  viewer.clearGeometryCache();
  if (state.pack) loadPackFile(state.pack, true);
  state.containers.forEach((c, i) => viewer.addContainer(c, i === 0));
};
setInterval(async () => {
  if (!$<HTMLInputElement>("autoReload").checked || !state.pack) return;
  const m = await api.mtime(state.pack);
  if (m && m !== state.packMtime) loadPackFile(state.pack, true);
}, 1500);

function appearance() {
  viewer.contOpacity = parseFloat($<HTMLInputElement>("contOpacity").value);
  viewer.fruitOpacity = parseFloat($<HTMLInputElement>("fruitOpacity").value);
  viewer.wire = $<HTMLInputElement>("wire").checked;
  viewer.contWire = $<HTMLInputElement>("contWire").checked;
  viewer.containers.visible = $<HTMLInputElement>("contShow").checked;
  viewer.applyMaterials();
}
for (const id of ["contOpacity", "fruitOpacity", "wire", "contWire", "contShow"]) $(id).oninput = appearance;
$("labels").onchange = (e) => viewer.setLabels((e.target as HTMLInputElement).checked);

function clipChanged() {
  viewer.clipOn = $<HTMLInputElement>("clip").checked;
  viewer.clipAxis = "xyz".indexOf($<HTMLSelectElement>("clipAxis").value);
  viewer.clipValue = parseFloat($<HTMLInputElement>("clipPos").value);
  viewer.updateClip();
  syncHash();
}
for (const id of ["clip", "clipAxis", "clipPos"]) $(id).oninput = clipChanged;
$("cull").oninput = () => {
  viewer.cullOn = $<HTMLInputElement>("cull").checked;
  viewer.redraw();
};
$("clipFlip").onclick = () => {
  viewer.clipFlip = !viewer.clipFlip;
  clipChanged();
};

$("fit").onclick = () => viewer.fit();
$("shot").onclick = async () => {
  const blob = await viewer.screenshot();
  if (!blob) return;
  const a = document.createElement("a");
  a.href = URL.createObjectURL(blob);
  a.download = (state.pack ? baseName(state.pack).replace(/\.txt$/, "") : "pack") + ".png";
  a.click();
};
$("link").onclick = async () => {
  clearTimeout(hashTimer);
  syncHash();
  await new Promise((r) => setTimeout(r, 350));
  await navigator.clipboard.writeText(location.href);
  status("link copied");
};
function syncHideBtn() {
  $("hideSel").textContent = viewer.selected && !viewer.selected.visible ? "Unhide" : "Hide";
}
$("filter").oninput = buildList;
$("hideSel").onclick = () => {
  if (viewer.selected) viewer.setVisible(viewer.selected, !viewer.selected.visible);
  syncHideBtn();
  buildList();
};
$("isoSel").onclick = () => {
  if (viewer.selected) viewer.isolate(viewer.selected);
  syncHideBtn();
  buildList();
};
$("showAll").onclick = () => {
  viewer.showAll();
  syncHideBtn();
  buildList();
};

// ---- rigid body sim ----
function syncForceBtns() {
  $("forcePoint").classList.toggle("on", viewer.forceMode === "point");
  $("forceDir").classList.toggle("on", viewer.forceMode === "dir");
}
$("forcePoint").onclick = () => {
  if (viewer.forceMode !== "point" && !viewer.selected) {
    status("select a fruit first");
    return;
  }
  const m = viewer.setForceMode(viewer.forceMode === "point" ? "none" : "point");
  syncForceBtns();
  if (m === "point") status("click on the selected fruit to set force point");
};
$("forceDir").onclick = () => {
  if (viewer.forceMode !== "dir" && !viewer.sim.force) {
    status("set force point first");
    return;
  }
  const m = viewer.setForceMode(viewer.forceMode === "dir" ? "none" : "dir");
  syncForceBtns();
  if (m === "dir") status("drag the rotation gizmo to aim force direction");
};
viewer.onForceChange = (f) => {
  syncForceBtns();
  if (!f) {
    $("forceInfo").textContent = "select a fruit, then click Force point";
    return;
  }
  const d = f.dir;
  const info = `${labelOf(f.item.inst)} dir (${d.x.toFixed(2)}, ${d.y.toFixed(2)}, ${d.z.toFixed(2)})`;
  $("forceInfo").textContent = info;
  if (viewer.forceMode !== "dir") logStatus(`force set on ${info}`);
};
$("forceMag").oninput = (e) => {
  const mag = parseFloat((e.target as HTMLInputElement).value);
  if (viewer.sim.force) {
    viewer.sim.force.magnitude = mag;
    viewer.updateForceArrow();
  }
};
viewer.onSimDone = () => {
  $("runSim").textContent = "Run";
  $("runSim").classList.remove("on");
  logStatus("sim: finished");
};
$("runSim").onclick = () => {
  const mag = parseFloat($<HTMLInputElement>("forceMag").value);
  const running = viewer.toggleSim(mag, 30);
  syncForceBtns();
  $("runSim").textContent = running ? "Stop" : "Run";
  $("runSim").classList.toggle("on", running);
  if (running) logStatus(`sim: running 30 steps (force=${mag})`);
};

window.addEventListener("keydown", (e) => {
  if ((e.target as HTMLElement).tagName === "INPUT") return;
  if (e.key === "h") $("hideSel").click();
  else if (e.key === "i") $("isoSel").click();
  else if (e.key === "a") $("showAll").click();
  else if (e.key === "k") showCollisions();
  else if (e.key === "w") setMode("translate");
  else if (e.key === "e") setMode("rotate");
  else if (e.key === "Delete") deleteSelected();
  else if (e.key === "c") copySelected();
  else if (e.key === "ArrowRight" && viewer.maxStep) setStep(Math.min(viewer.maxStep, viewer.step + 1));
  else if (e.key === "ArrowLeft" && viewer.maxStep) setStep(Math.max(0, viewer.step - 1));
});

// ---- collapsible sidebar sections & horizontal resize ----
document.querySelectorAll<HTMLElement>("#panel section > h3").forEach((h) => {
  h.onclick = () => h.parentElement?.classList.toggle("collapsed");
});

const resizer = $("panelResizer");
const panel = $("panel");
resizer.onpointerdown = (e) => {
  e.preventDefault();
  resizer.setPointerCapture(e.pointerId);
  resizer.classList.add("dragging");
};
resizer.onpointermove = (e) => {
  if (!resizer.hasPointerCapture(e.pointerId)) return;
  const w = Math.max(180, Math.min(window.innerWidth - 200, e.clientX));
  panel.style.width = w + "px";
};
resizer.onpointerup = resizer.onpointercancel = (e) => {
  if (resizer.hasPointerCapture(e.pointerId)) resizer.releasePointerCapture(e.pointerId);
  resizer.classList.remove("dragging");
};

restoreFromHash();
