// Game controller: ties the rules, the three.js board and the engine worker together.
import { Board3D } from "./Board3D.ts";
import { loadPieces } from "./Pieces.ts";
import { initSounds, playSound, SoundName } from "./Sounds.ts";
import { detectOpening, type OpeningRow } from "./opening.ts";
import { compilePlans, fittingPlans, type OpeningPlan } from "./openingPlans.ts";
import {
  Position, START_FEN, Move, WHITE, BLACK, colorOf, moveToUci, typeOf, P,
} from "./rules.ts";
import type { SearchReply, BookDepthReply } from "./worker.ts";

const $ = <T extends HTMLElement>(id: string) => document.getElementById(id) as T;

const worker = new Worker(new URL("./worker.ts", import.meta.url), { type: "module" });

const startFen = START_FEN;
let pos = new Position(startFen);
let played: string[] = []; // coordinate moves, sent to the engine so it sees repetitions
let sans: string[] = [];
let humanColor = WHITE;
let lastMove: Move | null = null;
let selected = -1;
let targets: Move[] = [];
let thinking = false;
let busy = false; // a move animation is running
let reqId = 0;
let depthReqId = 0;
// opening detection: hash of the position after every ply (index 0 = start), the named openings
// loaded from openings.json, and how many of the moves played are still engine book moves
let plyKeys: number[] = [];
let bookDepth = 0;
const openings = new Map<number, OpeningRow>();
// the 20 popular openings the engine rotates through, and the one it is following in this game
const plans = compilePlans();
let currentPlan: OpeningPlan | null = null;
let over = false;
let pendingPromo: Move[] | null = null;
let viewFlipped = false;

const board = new Board3D($("board"), {
  onSquare: (s) => onSquare(s),
  // only the side to move, and only on the human's turn, can pick pieces up
  canDrag: (s) => humanTurn() && pos.board[s] !== 0 && colorOf(pos.board[s]) === pos.turn,
  onDragStart: (s) => select(s),
  onDrop: (from, to) => dropMove(from, to),
});

function humanTurn() {
  return pos.turn === humanColor && !thinking && !busy && !over && !pendingPromo;
}

function refresh() {
  board.setPosition(pos.board);
  board.setCaptured(pos.capturedPieces());
  highlight();
  updateStatus();
}

function highlight() {
  const king = pos.inCheck() ? pos.kingSquare(pos.turn) : -1;
  board.setHighlights({
    selected,
    targets: targets.map((m) => m.to),
    captures: targets.filter((m) => pos.board[m.to] || (typeOf(pos.board[m.from]) === P && m.to === pos.ep)).map((m) => m.to),
    last: lastMove ? [lastMove.from, lastMove.to] : [],
    check: king,
  });
}

const STATUS_TEXT: Record<string, string> = {
  stalemate: "Draw by stalemate",
  insufficient: "Draw: insufficient material",
  fifty: "Draw by the fifty-move rule",
  repetition: "Draw by threefold repetition",
};

function updateStatus() {
  const st = pos.status();
  over = st !== "playing";
  let text: string;
  if (st === "checkmate") text = (pos.turn === WHITE ? "Black" : "White") + " wins by checkmate";
  else if (over) text = STATUS_TEXT[st];
  else if (thinking) text = "Engine is thinking...";
  else text = (pos.turn === WHITE ? "White" : "Black") + " to move" + (pos.inCheck() ? " - check" : "");
  $("status").textContent = text;
  $("status").classList.toggle("over", over);
  renderMoveList();
}

function renderMoveList() {
  updateOpening();
  const el = $("moves");
  let html = "";
  for (let i = 0; i < sans.length; i += 2) {
    html += `<span class="n">${i / 2 + 1}.</span><span>${sans[i]}</span><span>${sans[i + 1] ?? ""}</span>`;
  }
  el.innerHTML = html;
  el.scrollTop = el.scrollHeight;
}

function onSquare(s: number) {
  if (!humanTurn()) return;
  const move = targets.filter((m) => m.to === s);
  if (selected >= 0 && move.length) {
    if (move.length > 1) {
      askPromotion(move); // four moves, one per promotion piece
    } else {
      playMove(move[0]);
    }
    return;
  }
  select(s);
}

// select the piece on s (showing its legal targets), or clear the selection
function select(s: number) {
  const p = pos.board[s];
  if (p && colorOf(p) === pos.turn) {
    selected = s;
    targets = pos.legalMovesFrom(s);
  } else {
    selected = -1;
    targets = [];
  }
  highlight();
}

// a piece was dropped on a square: true if that is a legal move (it is then played)
function dropMove(from: number, to: number): boolean {
  const moves = pos.legalMovesFrom(from).filter((m) => m.to === to);
  if (!moves.length) return false;
  if (moves.length > 1) askPromotion(moves);
  else playMove(moves[0], false);
  return true;
}

function askPromotion(moves: Move[]) {
  pendingPromo = moves;
  const el = $("promo");
  el.classList.remove("hidden");
  el.querySelectorAll("button").forEach((b) => {
    (b as HTMLElement).onclick = () => {
      const piece = +(b as HTMLElement).dataset.piece!;
      const m = moves.find((x) => x.promo === piece)!;
      el.classList.add("hidden");
      pendingPromo = null;
      playMove(m, false);
    };
  });
}

// animate=false when the piece was already dragged onto its target square
function playMove(m: Move, animate = true) {
  const sound: SoundName = pos.kind(m); // the move kinds are the sound names
  sans.push(pos.san(m));
  played.push(moveToUci(m));
  pos.make(m);
  plyKeys.push(pos.keyHash());
  requestBookDepth();
  lastMove = m;
  saveGame();
  selected = -1;
  targets = [];
  const done = () => {
    playSound(sound); // when the piece lands
    busy = false;
    refresh();
    maybeEngineMove();
  };
  if (!animate) return done();
  busy = true;
  highlight();
  board.animateMove(m.from, m.to, done);
}

// The engine's next move from one of the popular openings, if the game still fits one: keep to the
// current plan while the opponent follows it, otherwise pick another that fits at random.
function planMove(): { move: Move; name: string } | null {
  if (!currentPlan || !fittingPlans([currentPlan], played).length) {
    const fits = fittingPlans(plans, played);
    currentPlan = fits.length ? fits[Math.floor(Math.random() * fits.length)] : null;
  }
  if (!currentPlan) return null;
  const uci = currentPlan.moves[played.length];
  const move = pos.legalMoves().find((m) => moveToUci(m) === uci);
  return move ? { move, name: currentPlan.name } : null;
}

function maybeEngineMove() {
  if (over || pos.turn === humanColor || thinking) return;
  thinking = true;
  updateStatus();
  const plan = $<HTMLInputElement>("book").checked ? planMove() : null;
  if (plan) {
    // a short pause so the reply does not feel instant, like the searched moves
    const id = ++reqId;
    $("info").textContent = `opening line: ${plan.name}`;
    window.setTimeout(() => {
      if (id !== reqId || !thinking) return; // new game or undo in the meantime
      thinking = false;
      playMove(plan.move);
    }, 350);
    return;
  }
  const req = {
    type: "search" as const,
    id: ++reqId,
    fen: startFen,
    moves: played.join(" "),
    maxDepth: 30,
    timeMs: +$<HTMLSelectElement>("time").value,
    useBook: $<HTMLInputElement>("book").checked,
  };
  $("info").textContent = "";
  worker.postMessage(req);
}

worker.onmessage = (e: MessageEvent<SearchReply | BookDepthReply>) => {
  const r = e.data;
  if (r.type === "bookDepth") {
    if (r.id === depthReqId) {
      bookDepth = r.depth;
      updateOpening();
    }
    return;
  }
  if (r.id !== reqId || !thinking) return; // stale answer (new game / undo happened meanwhile)
  thinking = false;
  const whiteScore = pos.turn === WHITE ? r.score : -r.score;
  const mate = Math.abs(r.score) > 90000;
  const scoreText = mate ? (whiteScore > 0 ? "white mates" : "black mates") : (whiteScore >= 0 ? "+" : "") + (whiteScore / 100).toFixed(2);
  $("info").textContent = r.book
    ? `book move (played in ${r.book} games)`
    : `depth ${r.depth}, eval ${scoreText}, ${(r.nodes / 1000).toFixed(0)}k nodes, ${r.ms} ms`;
  const move = pos.legalMoves().find((m) => moveToUci(m) === r.best);
  if (!move) {
    updateStatus();
    return;
  }
  playMove(move);
};

// ask the engine how many of the moves played are still book moves (it answers before any search)
function requestBookDepth() {
  worker.postMessage({ type: "bookDepth", id: ++depthReqId, fen: startFen, moves: played.join(" ") });
}

// The box above the move list (see detectOpening): the name if there is one, then the moves.
function updateOpening() {
  const found = detectOpening(openings, plyKeys, sans, bookDepth);
  const box = $("opening");
  box.replaceChildren();
  box.classList.toggle("hidden", !found);
  if (!found) return;
  if (found.name) {
    const title = document.createElement("div");
    title.className = "name";
    const code = document.createElement("span");
    code.className = "eco";
    code.textContent = found.eco!;
    title.append(code, found.name);
    box.append(title);
  }
  const line = document.createElement("div");
  line.className = "line";
  line.textContent = found.line;
  box.append(line);
}

// the opening names are loaded after the first paint; the box updates once they are there
fetch("openings.json")
  .then((r) => r.json())
  .then((rows: OpeningRow[]) => {
    for (const row of rows) openings.set(row[0], row);
    updateOpening();
  })
  .catch((e) => console.warn("cannot load openings.json", e));

// ---- keep the ongoing game across page reloads (localStorage) ----
const SAVE_KEY = "chess.game.v1";

// the game so far: moves from the start position (that is all a replay needs), plus the current FEN
// for reference, which side the human plays, the view and the settings
function saveGame() {
  try {
    localStorage.setItem(SAVE_KEY, JSON.stringify({
      startFen,
      moves: played,
      fen: pos.getFen(),
      human: humanColor === BLACK ? "b" : "w",
      flipped: viewFlipped,
      time: +$<HTMLSelectElement>("time").value,
      book: $<HTMLInputElement>("book").checked,
    }));
  } catch {
    // storage unavailable (private window, quota): the game just is not remembered
  }
}

// Loads the saved game if there is a valid one. Every saved move is replayed against the rules, so
// a damaged or outdated save is ignored rather than trusted. Returns false if nothing was restored.
function restoreGame(): boolean {
  let data: any;
  try {
    data = JSON.parse(localStorage.getItem(SAVE_KEY) ?? "null");
  } catch {
    return false;
  }
  if (!data || data.startFen !== startFen || !Array.isArray(data.moves)) return false;
  const p = new Position(startFen);
  const ucis: string[] = [], ss: string[] = [], keys = [p.keyHash()];
  let last: Move | null = null;
  for (const u of data.moves) {
    const mv = p.legalMoves().find((x) => moveToUci(x) === u);
    if (!mv) return false;
    ss.push(p.san(mv));
    p.make(mv);
    ucis.push(u);
    keys.push(p.keyHash());
    last = mv;
  }
  reqId++;
  thinking = false;
  busy = false;
  pendingPromo = null;
  $("promo").classList.add("hidden");
  pos = p;
  played = ucis;
  sans = ss;
  plyKeys = keys;
  lastMove = last;
  selected = -1;
  targets = [];
  currentPlan = null;
  humanColor = data.human === "b" ? BLACK : WHITE;
  $<HTMLSelectElement>("side").value = humanColor === BLACK ? "b" : "w";
  const time = $<HTMLSelectElement>("time");
  if ([...time.options].some((o) => o.value === String(data.time))) time.value = String(data.time);
  $<HTMLInputElement>("book").checked = data.book !== false;
  viewFlipped = typeof data.flipped === "boolean" ? data.flipped : humanColor === BLACK;
  worker.postMessage({ type: "new" });
  board.setFlipped(viewFlipped);
  $("info").textContent = "";
  refresh();
  requestBookDepth();
  maybeEngineMove(); // the engine was to move when the page closed: it picks up where it left off
  return true;
}

function newGame() {
  reqId++;
  thinking = false;
  busy = false;
  pendingPromo = null;
  $("promo").classList.add("hidden");
  humanColor = $<HTMLSelectElement>("side").value === "b" ? BLACK : WHITE;
  pos = new Position(startFen);
  played = [];
  sans = [];
  currentPlan = null;
  plyKeys = [pos.keyHash()];
  bookDepth = 0;
  lastMove = null;
  selected = -1;
  targets = [];
  worker.postMessage({ type: "new" });
  viewFlipped = humanColor === BLACK;
  board.setFlipped(viewFlipped);
  $("info").textContent = "";
  refresh();
  saveGame();
  maybeEngineMove();
}

function undo() {
  if (busy || pendingPromo) return;
  // cancel a running search, then take back until it is the human's turn again
  const plies = thinking ? 1 : 2;
  reqId++;
  thinking = false;
  for (let i = 0; i < plies && played.length; i++) {
    pos.unmake();
    played.pop();
    sans.pop();
    plyKeys.pop();
  }
  requestBookDepth();
  lastMove = null;
  saveGame();
  selected = -1;
  targets = [];
  refresh();
  maybeEngineMove();
}

$("newGame").onclick = newGame;
$("undo").onclick = undo;
$("flip").onclick = () => {
  viewFlipped = !viewFlipped;
  board.setFlipped(viewFlipped);
  saveGame();
};
$<HTMLSelectElement>("side").onchange = newGame;
$("time").onchange = saveGame;
$("book").onchange = saveGame;

initSounds();
// a game in progress when the page was last open is picked up again; otherwise the default start
loadPieces().then(() => {
  if (!restoreGame()) newGame();
});
