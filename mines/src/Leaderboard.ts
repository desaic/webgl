export interface Entry {
  name: string;
  ms: number;
  date: number;
}

const KEY = "mines.leaderboard.v1";
export const NAME_KEY = "mines.lastName";
export const MAX_ENTRIES = 10;

type Boards = Record<string, Entry[]>;

function load(): Boards {
  try {
    const raw = localStorage.getItem(KEY);
    const parsed = raw ? JSON.parse(raw) : {};
    return parsed && typeof parsed === "object" ? parsed : {};
  } catch {
    return {};
  }
}

function save(b: Boards) {
  try {
    localStorage.setItem(KEY, JSON.stringify(b));
  } catch {
    // storage full or blocked: the board just won't persist
  }
}

export function formatMs(ms: number): string {
  return (Math.floor(ms / 10) / 100).toFixed(2) + "s";
}

export function getEntries(cat: string): Entry[] {
  const list = load()[cat];
  return Array.isArray(list) ? list : [];
}

export function qualifies(cat: string, ms: number): boolean {
  const list = getEntries(cat);
  return list.length < MAX_ENTRIES || ms < list[list.length - 1].ms;
}

// returns the new entry's 0-based rank, or -1 if it didn't make the board
export function addEntry(cat: string, name: string, ms: number): number {
  const boards = load();
  const list = getEntries(cat);
  // ties keep the older entry ahead
  let rank = list.findIndex((e) => ms < e.ms);
  if (rank < 0) rank = list.length;
  list.splice(rank, 0, { name, ms, date: Date.now() });
  list.length = Math.min(list.length, MAX_ENTRIES);
  boards[cat] = list;
  save(boards);
  return rank < MAX_ENTRIES ? rank : -1;
}

export function clearEntries(cat: string) {
  const boards = load();
  delete boards[cat];
  save(boards);
}
