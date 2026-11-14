export interface Pose {
  pos: [number, number, number];
  rot: number[]; // row-major 3x3
  scale: number;
}

export interface Inst {
  id: number;
  name: string;
  meshPath: string | null; // absolute path from a trajectory file, else null
  poses: Pose[]; // 1 for a pack, N for a trajectory (last = final)
}

function parsePose(t: string[]): Pose | null {
  const ip = t.indexOf("pos");
  const ir = t.indexOf("rot");
  if (ip < 0 || ir < 0) return null;
  const pos = [+t[ip + 1], +t[ip + 2], +t[ip + 3]] as [number, number, number];
  const rot: number[] = [];
  for (let i = 0; i < 9; i++) rot.push(+t[ir + 1 + i]);
  const is = t.indexOf("scale");
  const scale = is >= 0 ? +t[is + 1] : 1;
  if (pos.some(isNaN) || rot.some(isNaN) || isNaN(scale)) return null;
  return { pos, rot, scale };
}

export function isTraj(text: string): boolean {
  return /^Instance\s/m.test(text);
}

// pack file: first line = count, then "name pos x y z rot r0..r8 scale s"
export function parsePack(text: string): Inst[] {
  const out: Inst[] = [];
  const lines = text.split(/\r?\n/);
  let start = 0;
  if (lines.length && /^\d+\s*$/.test(lines[0])) start = 1;
  for (let i = start; i < lines.length; i++) {
    const t = lines[i].trim().split(/\s+/);
    if (t.length < 16) continue;
    const pose = parsePose(t);
    if (!pose) continue;
    out.push({ id: out.length, name: t[0], meshPath: null, poses: [pose] });
  }
  return out;
}

// nudge trails written by the packer next to a pack: "row N count", then count pose lines
export function parseNudge(text: string): Map<number, Pose[]> {
  const out = new Map<number, Pose[]>();
  let cur: Pose[] | null = null;
  for (const raw of text.split(/\r?\n/)) {
    const line = raw.trim();
    if (line.startsWith("row")) {
      cur = [];
      out.set(parseInt(line.split(/\s+/)[1]), cur);
    } else if (cur && line.startsWith("pos")) {
      const pose = parsePose(line.split(/\s+/));
      if (pose) cur.push(pose);
    }
  }
  return out;
}

// trajectory file: "Instance N Mesh path" then Step/Final lines
export function parseTraj(text: string): Inst[] {
  const out: Inst[] = [];
  let cur: Inst | null = null;
  for (const raw of text.split(/\r?\n/)) {
    const line = raw.trim();
    if (!line) continue;
    if (line.startsWith("Instance")) {
      const t = line.split(/\s+/);
      const mi = t.indexOf("Mesh");
      const meshPath = mi >= 0 ? t.slice(mi + 1).join(" ") : "";
      const base = meshPath.split("/").pop() || "cube";
      cur = {
        id: parseInt(t[1]),
        name: base.replace(/\.[^.]+$/, ""),
        meshPath: meshPath || null,
        poses: [],
      };
      out.push(cur);
    } else if (cur && (line.startsWith("Step") || line.startsWith("Final") || line.startsWith("Initial"))) {
      const pose = parsePose(line.split(/\s+/));
      if (pose) {
        if (line.startsWith("Initial")) cur.poses.unshift(pose);
        else cur.poses.push(pose);
      }
    }
  }
  return out.filter((i) => i.poses.length > 0);
}
