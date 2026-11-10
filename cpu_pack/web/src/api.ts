export interface Entry {
  name: string;
  dir: boolean;
  size: number;
  mtime: number;
}

export interface Listing {
  path: string;
  root: string;
  parent: string | null;
  entries: Entry[];
}

const q = (p: string) => encodeURIComponent(p);

export async function list(path: string): Promise<Listing> {
  const r = await fetch("/api/list?path=" + q(path));
  if (!r.ok) throw new Error(await r.text());
  return r.json();
}

export async function fetchText(path: string): Promise<string> {
  const r = await fetch("/api/file?path=" + q(path));
  if (!r.ok) throw new Error(path + ": " + (await r.text()));
  return r.text();
}

export async function fetchBuffer(path: string): Promise<ArrayBuffer> {
  const r = await fetch("/api/file?path=" + q(path));
  if (!r.ok) throw new Error(path + ": " + (await r.text()));
  return r.arrayBuffer();
}

export async function mtime(path: string): Promise<number> {
  const r = await fetch("/api/mtime?path=" + q(path));
  if (!r.ok) return 0;
  return parseFloat(await r.text());
}
