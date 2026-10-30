export const HIDDEN = 0;
export const REVEALED = 1;
export const FLAGGED = 2;

export type GameStatus = "ready" | "playing" | "won" | "lost";

export interface Difficulty {
  cols: number;
  rows: number;
  mines: number;
}

export const DIFFICULTIES: Record<string, Difficulty> = {
  beginner: { cols: 9, rows: 9, mines: 10 },
  intermediate: { cols: 16, rows: 16, mines: 40 },
  expert: { cols: 30, rows: 16, mines: 99 },
};

// Pure game logic, no rendering. Cell (x,y) lives at index y * cols + x.
export class Minesweeper {
  public readonly cols: number;
  public readonly rows: number;
  public readonly mines: number;
  public status: GameStatus = "ready";
  public flags = 0;
  // index of the mine the player stepped on, -1 if none
  public exploded = -1;

  private mine: Uint8Array;
  private adj: Uint8Array;
  private state: Uint8Array;
  private revealedCount = 0;

  constructor(d: Difficulty) {
    this.cols = d.cols;
    this.rows = d.rows;
    // always leave room for the first-click safe cell
    this.mines = Math.min(d.mines, d.cols * d.rows - 1);
    const n = this.cols * this.rows;
    this.mine = new Uint8Array(n);
    this.adj = new Uint8Array(n);
    this.state = new Uint8Array(n);
  }

  public idx(x: number, y: number) {
    return y * this.cols + x;
  }

  public inBounds(x: number, y: number) {
    return x >= 0 && y >= 0 && x < this.cols && y < this.rows;
  }

  public getState(x: number, y: number) {
    return this.state[this.idx(x, y)];
  }

  public isMine(x: number, y: number) {
    return this.mine[this.idx(x, y)] === 1;
  }

  public adjacent(x: number, y: number) {
    return this.adj[this.idx(x, y)];
  }

  public minesLeft() {
    return this.mines - this.flags;
  }

  public isOver() {
    return this.status === "won" || this.status === "lost";
  }

  public toggleFlag(x: number, y: number): boolean {
    if (this.isOver()) return false;
    const i = this.idx(x, y);
    if (this.state[i] === REVEALED) return false;
    if (this.state[i] === FLAGGED) {
      this.state[i] = HIDDEN;
      this.flags--;
    } else {
      this.state[i] = FLAGGED;
      this.flags++;
    }
    return true;
  }

  public reveal(x: number, y: number): boolean {
    if (this.isOver()) return false;
    const i = this.idx(x, y);
    if (this.state[i] !== HIDDEN) return false;

    if (this.status === "ready") {
      this.placeMines(x, y);
      this.status = "playing";
    }

    if (this.mine[i]) {
      this.exploded = i;
      this.status = "lost";
      return true;
    }

    this.floodReveal(x, y);
    this.checkWin();
    return true;
  }

  // Reveal all unflagged neighbours of a revealed number whose flag count matches it.
  public chord(x: number, y: number): boolean {
    if (this.status !== "playing") return false;
    const i = this.idx(x, y);
    if (this.state[i] !== REVEALED || this.adj[i] === 0) return false;

    let flagged = 0;
    this.forNeighbors(x, y, (nx, ny) => {
      if (this.state[this.idx(nx, ny)] === FLAGGED) flagged++;
    });
    if (flagged !== this.adj[i]) return false;

    let changed = false;
    this.forNeighbors(x, y, (nx, ny) => {
      if (this.reveal(nx, ny)) changed = true;
    });
    return changed;
  }

  public forNeighbors(x: number, y: number, fn: (nx: number, ny: number) => void) {
    for (let dy = -1; dy <= 1; dy++) {
      for (let dx = -1; dx <= 1; dx++) {
        if (dx === 0 && dy === 0) continue;
        const nx = x + dx;
        const ny = y + dy;
        if (this.inBounds(nx, ny)) fn(nx, ny);
      }
    }
  }

  private placeMines(safeX: number, safeY: number) {
    const n = this.cols * this.rows;
    const safe = this.idx(safeX, safeY);
    // partial Fisher-Yates over every cell except the first-clicked one
    const cells: number[] = [];
    for (let i = 0; i < n; i++) if (i !== safe) cells.push(i);
    for (let k = 0; k < this.mines; k++) {
      const j = k + Math.floor(Math.random() * (cells.length - k));
      [cells[k], cells[j]] = [cells[j], cells[k]];
      this.mine[cells[k]] = 1;
    }
    for (let y = 0; y < this.rows; y++) {
      for (let x = 0; x < this.cols; x++) {
        let c = 0;
        this.forNeighbors(x, y, (nx, ny) => {
          c += this.mine[this.idx(nx, ny)];
        });
        this.adj[this.idx(x, y)] = c;
      }
    }
  }

  private floodReveal(x: number, y: number) {
    const stack = [this.idx(x, y)];
    while (stack.length) {
      const i = stack.pop()!;
      if (this.state[i] !== HIDDEN) continue;
      this.state[i] = REVEALED;
      this.revealedCount++;
      if (this.adj[i] !== 0) continue;
      const cx = i % this.cols;
      const cy = Math.floor(i / this.cols);
      this.forNeighbors(cx, cy, (nx, ny) => {
        const ni = this.idx(nx, ny);
        if (this.state[ni] === HIDDEN) stack.push(ni);
      });
    }
  }

  private checkWin() {
    if (this.revealedCount !== this.cols * this.rows - this.mines) return;
    this.status = "won";
    // flag every remaining mine, like the original
    for (let i = 0; i < this.state.length; i++) {
      if (this.mine[i]) this.state[i] = FLAGGED;
    }
    this.flags = this.mines;
  }
}
