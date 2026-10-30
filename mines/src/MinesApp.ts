import * as THREE from "three";

import World from "./World";
import { GridMesh } from "./GridMesh";
import { LeaderboardUI } from "./LeaderboardUI";
import { DIFFICULTIES, FLAGGED, HIDDEN, REVEALED, Minesweeper } from "./Minesweeper";

const CELL_PX = 32;
const TILE_PX = 64;
const ATLAS_N = 8;

// tile indices in the atlas: 0..8 are revealed cells with that many neighbours
const T_HIDDEN = 9;
const T_FLAG = 10;
const T_MINE = 11;
const T_EXPLODED = 12;
const T_WRONG_FLAG = 13;

const NUMBER_COLORS = [
  "", "#0000ff", "#008000", "#ff0000", "#000080", "#800000", "#008080", "#000000", "#808080",
];

const FACES = { playing: "🙂", pressed: "😮", won: "😎", lost: "😵" };

function MakeAtlas(): THREE.CanvasTexture {
  const canvas = document.createElement("canvas");
  canvas.width = canvas.height = TILE_PX * ATLAS_N;
  const ctx = canvas.getContext("2d")!;
  const T = TILE_PX;

  const origin = (i: number) => ({ ox: (i % ATLAS_N) * T, oy: Math.floor(i / ATLAS_N) * T });

  const drawRevealed = (i: number, bg = "#c0c0c0") => {
    const { ox, oy } = origin(i);
    ctx.fillStyle = "#808080";
    ctx.fillRect(ox, oy, T, T);
    ctx.fillStyle = bg;
    ctx.fillRect(ox + 2, oy + 2, T - 2, T - 2);
  };

  const drawHidden = (i: number) => {
    const { ox, oy } = origin(i);
    const b = 6;
    ctx.fillStyle = "#c0c0c0";
    ctx.fillRect(ox, oy, T, T);
    ctx.fillStyle = "#ffffff";
    ctx.beginPath();
    ctx.moveTo(ox, oy);
    ctx.lineTo(ox + T, oy);
    ctx.lineTo(ox + T - b, oy + b);
    ctx.lineTo(ox + b, oy + b);
    ctx.lineTo(ox + b, oy + T - b);
    ctx.lineTo(ox, oy + T);
    ctx.closePath();
    ctx.fill();
    ctx.fillStyle = "#808080";
    ctx.beginPath();
    ctx.moveTo(ox + T, oy + T);
    ctx.lineTo(ox, oy + T);
    ctx.lineTo(ox + b, oy + T - b);
    ctx.lineTo(ox + T - b, oy + T - b);
    ctx.lineTo(ox + T - b, oy + b);
    ctx.lineTo(ox + T, oy);
    ctx.closePath();
    ctx.fill();
  };

  const drawMine = (i: number) => {
    const { ox, oy } = origin(i);
    const cx = ox + T / 2;
    const cy = oy + T / 2;
    ctx.strokeStyle = "#000";
    ctx.lineWidth = 4;
    for (let k = 0; k < 4; k++) {
      const a = (k * Math.PI) / 4;
      ctx.beginPath();
      ctx.moveTo(cx - Math.cos(a) * 22, cy - Math.sin(a) * 22);
      ctx.lineTo(cx + Math.cos(a) * 22, cy + Math.sin(a) * 22);
      ctx.stroke();
    }
    ctx.fillStyle = "#000";
    ctx.beginPath();
    ctx.arc(cx, cy, 15, 0, Math.PI * 2);
    ctx.fill();
    ctx.fillStyle = "#fff";
    ctx.fillRect(cx - 8, cy - 8, 6, 6);
  };

  const drawFlag = (i: number) => {
    const { ox, oy } = origin(i);
    ctx.fillStyle = "#ff0000";
    ctx.beginPath();
    ctx.moveTo(ox + 34, oy + 14);
    ctx.lineTo(ox + 34, oy + 34);
    ctx.lineTo(ox + 16, oy + 24);
    ctx.closePath();
    ctx.fill();
    ctx.fillStyle = "#000";
    ctx.fillRect(ox + 32, oy + 14, 4, 34);
    ctx.fillRect(ox + 20, oy + 46, 28, 6);
  };

  for (let n = 0; n <= 8; n++) {
    drawRevealed(n);
    if (n > 0) {
      const { ox, oy } = origin(n);
      ctx.fillStyle = NUMBER_COLORS[n];
      ctx.font = "bold 46px Arial, Helvetica, sans-serif";
      ctx.textAlign = "center";
      ctx.textBaseline = "middle";
      ctx.fillText(String(n), ox + T / 2 + 1, oy + T / 2 + 3);
    }
  }
  drawHidden(T_HIDDEN);
  drawHidden(T_FLAG);
  drawFlag(T_FLAG);
  drawRevealed(T_MINE);
  drawMine(T_MINE);
  drawRevealed(T_EXPLODED, "#ff0000");
  drawMine(T_EXPLODED);
  drawRevealed(T_WRONG_FLAG);
  drawMine(T_WRONG_FLAG);
  {
    const { ox, oy } = origin(T_WRONG_FLAG);
    ctx.strokeStyle = "#ff0000";
    ctx.lineWidth = 6;
    ctx.beginPath();
    ctx.moveTo(ox + 8, oy + 8);
    ctx.lineTo(ox + T - 8, oy + T - 8);
    ctx.moveTo(ox + T - 8, oy + 8);
    ctx.lineTo(ox + 8, oy + T - 8);
    ctx.stroke();
  }

  const tex = new THREE.CanvasTexture(canvas);
  tex.colorSpace = THREE.SRGBColorSpace;
  tex.generateMipmaps = false;
  tex.minFilter = THREE.LinearFilter;
  tex.magFilter = THREE.LinearFilter;
  return tex;
}

function FormatLcd(n: number): string {
  if (n < 0) return "-" + String(Math.min(-n, 99)).padStart(2, "0");
  return String(Math.min(n, 999)).padStart(3, "0");
}

// elapsed ms as NNN.XX (centiseconds, truncated, capped at 999.99)
function FormatTime(ms: number): string {
  const cs = Math.min(Math.floor(ms / 10), 99999);
  return String(Math.floor(cs / 100)).padStart(3, "0") + "." + String(cs % 100).padStart(2, "0");
}

export class MinesApp {
  private canvas: HTMLCanvasElement;
  private renderer: THREE.WebGLRenderer;
  private world: World;
  private material: THREE.MeshBasicMaterial;

  private game: Minesweeper;
  private boardMesh: GridMesh | null = null;
  private dirty = true;

  // cell under the mouse while a button is held (null when nothing is pressed)
  private pressed: { x: number; y: number } | null = null;
  private chordMode = false;
  private suppressLeftUp = false;

  // performance.now() at the first reveal, and when the game ended (0 while running)
  private timerStart = 0;
  private timerEnd = 0;

  private faceEl: HTMLElement;
  private mineEl: HTMLElement;
  private timeEl: HTMLElement;
  private diffEl: HTMLSelectElement;
  private leaderboard = new LeaderboardUI();
  // true once the player has planted any flag this game (even if later removed)
  private flagUsed = false;
  // set by the on-screen flag button: the next board click/tap plants a flag, then it clears
  private flagArmed = false;
  private flagBtn: HTMLElement;

  constructor(canvas: HTMLCanvasElement) {
    this.canvas = canvas;
    this.faceEl = document.getElementById("face")!;
    this.mineEl = document.getElementById("mineCount")!;
    this.timeEl = document.getElementById("timer")!;
    this.diffEl = document.getElementById("difficulty") as HTMLSelectElement;
    this.flagBtn = document.getElementById("flagBtn")!;

    this.renderer = new THREE.WebGLRenderer({ canvas, antialias: false });
    this.renderer.setPixelRatio(window.devicePixelRatio || 1);
    this.world = new World();
    this.material = new THREE.MeshBasicMaterial({ map: MakeAtlas() });

    canvas.addEventListener("mousedown", (e) => this.onMouseDown(e));
    canvas.addEventListener("mousemove", (e) => this.onMouseMove(e));
    canvas.addEventListener("contextmenu", (e) => e.preventDefault());
    canvas.addEventListener("auxclick", (e) => e.preventDefault());
    window.addEventListener("mouseup", (e) => this.onMouseUp(e));
    document.getElementById("lbBtn")!.addEventListener("click", () => this.leaderboard.show(this.diffEl.value));
    this.flagBtn.addEventListener("click", () => this.setFlagArmed(!this.flagArmed));
    this.faceEl.addEventListener("click", () => this.newGame());
    this.diffEl.addEventListener("change", () => this.newGame());
    window.addEventListener("keydown", (e) => {
      if (e.key === "F2" && !this.leaderboard.isOpen()) {
        e.preventDefault();
        this.newGame();
      }
    });

    this.newGame();
  }

  public newGame() {
    this.timerStart = 0;
    this.timerEnd = 0;
    this.flagUsed = false;
    this.setFlagArmed(false);
    this.pressed = null;
    this.chordMode = false;
    this.suppressLeftUp = false;

    this.game = new Minesweeper(DIFFICULTIES[this.diffEl.value] ?? DIFFICULTIES.beginner);
    const { cols, rows } = this.game;

    if (this.boardMesh) {
      this.world.Remove(this.boardMesh.mesh);
      this.boardMesh.dispose();
    }
    this.boardMesh = new GridMesh(cols, rows);
    this.boardMesh.mesh.material = this.material;
    this.world.Add(this.boardMesh.mesh);
    this.world.SetBoardSize(cols, rows);

    this.canvas.style.width = cols * CELL_PX + "px";
    this.canvas.style.height = rows * CELL_PX + "px";
    this.renderer.setSize(cols * CELL_PX, rows * CELL_PX);
    document.getElementById("game")!.style.width = cols * CELL_PX + 24 + "px";

    this.updateHud();
    this.refreshBoard();
  }

  // grid cell under a mouse event, or null if outside the board
  private cellAt(e: MouseEvent): { x: number; y: number } | null {
    const r = this.canvas.getBoundingClientRect();
    const x = Math.floor((e.clientX - r.left) / CELL_PX);
    const y = this.game.rows - 1 - Math.floor((e.clientY - r.top) / CELL_PX);
    return this.game.inBounds(x, y) ? { x, y } : null;
  }

  private setFlagArmed(on: boolean) {
    this.flagArmed = on;
    this.flagBtn.classList.toggle("armed", on);
    this.flagBtn.setAttribute("aria-pressed", String(on));
  }

  private onMouseDown(e: MouseEvent) {
    e.preventDefault();
    if (this.game.isOver()) return;
    const cell = this.cellAt(e);
    if (!cell) return;

    const left = e.button === 0;

    if (this.flagArmed && left) {
      this.setFlagArmed(false);
      if (this.game.toggleFlag(cell.x, cell.y)) {
        this.flagUsed = true;
        this.updateHud();
      }
      this.refreshBoard();
      return;
    }
    const right = e.button === 2;
    const middle = e.button === 1;
    const both = (left && (e.buttons & 2) !== 0) || (right && (e.buttons & 1) !== 0);

    // a plain left click on a revealed number also chords
    const onNumber =
      left && this.game.getState(cell.x, cell.y) === REVEALED && this.game.adjacent(cell.x, cell.y) > 0;

    if (middle || both || onNumber) {
      this.chordMode = true;
      this.pressed = cell;
    } else if (right) {
      if (this.game.toggleFlag(cell.x, cell.y)) {
        this.flagUsed = true;
        this.updateHud();
      }
    } else if (left) {
      this.pressed = cell;
    }
    this.refreshBoard();
  }

  private onMouseMove(e: MouseEvent) {
    if (!this.pressed) return;
    const cell = this.cellAt(e);
    if (cell?.x === this.pressed.x && cell?.y === this.pressed.y) return;
    this.pressed = cell;
    this.refreshBoard();
  }

  private onMouseUp(e: MouseEvent) {
    const cell = this.pressed;
    const wasChord = this.chordMode;

    if (wasChord) {
      // a chord fires when the first of its buttons is released
      this.chordMode = false;
      this.pressed = null;
      if (cell) {
        this.game.chord(cell.x, cell.y);
        this.afterMove();
      }
      this.suppressLeftUp = (e.buttons & 1) !== 0;
    } else if (e.button === 0) {
      if (this.suppressLeftUp) {
        this.suppressLeftUp = false;
      } else if (cell) {
        this.pressed = null;
        this.game.reveal(cell.x, cell.y);
        this.afterMove();
      }
    }

    if (this.pressed || e.button === 0) this.pressed = null;
    this.refreshBoard();
  }

  private afterMove() {
    const g = this.game;
    if (g.status !== "ready" && !this.timerStart) this.timerStart = performance.now();
    if (g.isOver() && !this.timerEnd) {
      this.timerEnd = performance.now();
      if (g.status === "won") {
        this.leaderboard.offerRecord(this.diffEl.value, !this.flagUsed, this.timerEnd - this.timerStart);
      }
    }
    this.updateHud();
  }

  private elapsedMs(): number {
    if (!this.timerStart) return 0;
    const end = this.timerEnd || performance.now();
    return end - this.timerStart;
  }

  private updateHud() {
    this.mineEl.textContent = FormatLcd(this.game.minesLeft());
    this.timeEl.textContent = FormatTime(this.elapsedMs());
    this.faceEl.textContent = this.faceFor();
  }

  private faceFor(): string {
    const s = this.game.status;
    if (s === "won") return FACES.won;
    if (s === "lost") return FACES.lost;
    return this.pressed ? FACES.pressed : FACES.playing;
  }

  private tileFor(x: number, y: number): number {
    const g = this.game;
    const st = g.getState(x, y);
    const mine = g.isMine(x, y);

    if (g.status === "lost") {
      if (mine) {
        if (g.idx(x, y) === g.exploded) return T_EXPLODED;
        return st === FLAGGED ? T_FLAG : T_MINE;
      }
      if (st === FLAGGED) return T_WRONG_FLAG;
    }
    if (st === FLAGGED) return T_FLAG;
    if (st === HIDDEN) return this.isPressed(x, y) ? 0 : T_HIDDEN;
    return g.adjacent(x, y);
  }

  private isPressed(x: number, y: number): boolean {
    const p = this.pressed;
    if (!p || this.game.isOver()) return false;
    if (!this.chordMode) return p.x === x && p.y === y;
    return Math.abs(p.x - x) <= 1 && Math.abs(p.y - y) <= 1;
  }

  private refreshBoard() {
    const mesh = this.boardMesh!;
    const { cols, rows } = this.game;
    for (let y = 0; y < rows; y++) {
      for (let x = 0; x < cols; x++) {
        const t = this.tileFor(x, y);
        // GridMesh uv rows count from the bottom of the texture, atlas rows from the top
        const uvIndex = (ATLAS_N - 1 - Math.floor(t / ATLAS_N)) * ATLAS_N + (t % ATLAS_N);
        mesh.SetCellUV(x, y, uvIndex);
      }
    }
    mesh.mesh.geometry.attributes.uv.needsUpdate = true;
    this.faceEl.textContent = this.faceFor();
    this.dirty = true;
  }

  public render(): void {
    // clock is redrawn every animation frame while the game is running
    if (this.timerStart && !this.timerEnd) {
      this.timeEl.textContent = FormatTime(this.elapsedMs());
    }
    if (!this.dirty) return;
    this.dirty = false;
    this.renderer.render(this.world.scene, this.world.camera);
  }
}
