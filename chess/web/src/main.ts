// Game controller: ties the rules, the three.js board and the engine worker together.
import { Board3D } from "./Board3D.ts";
import {
  Position, START_FEN, Move, WHITE, BLACK, colorOf, moveToUci, typeOf, P,
} from "./rules.ts";
import type { SearchReply } from "./worker.ts";

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
let over = false;
let pendingPromo: Move[] | null = null;
let viewFlipped = false;

const board = new Board3D($("board"), (s) => onSquare(s));

function humanTurn() {
  return pos.turn === humanColor && !thinking && !busy && !over && !pendingPromo;
}

function refresh() {
  board.setPosition(pos.board);
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
      playMove(m);
    };
  });
}

function playMove(m: Move) {
  sans.push(pos.san(m));
  played.push(moveToUci(m));
  pos.make(m);
  lastMove = m;
  selected = -1;
  targets = [];
  busy = true;
  highlight();
  board.animateMove(m.from, m.to, () => {
    busy = false;
    refresh();
    maybeEngineMove();
  });
}

function maybeEngineMove() {
  if (over || pos.turn === humanColor || thinking) return;
  thinking = true;
  updateStatus();
  const req = {
    id: ++reqId,
    fen: startFen,
    moves: played.join(" "),
    maxDepth: 30,
    timeMs: +$<HTMLSelectElement>("time").value,
  };
  $("info").textContent = "";
  worker.postMessage(req);
}

worker.onmessage = (e: MessageEvent<SearchReply>) => {
  const r = e.data;
  if (r.id !== reqId || !thinking) return; // stale answer (new game / undo happened meanwhile)
  thinking = false;
  const whiteScore = pos.turn === WHITE ? r.score : -r.score;
  const mate = Math.abs(r.score) > 90000;
  const scoreText = mate ? (whiteScore > 0 ? "white mates" : "black mates") : (whiteScore >= 0 ? "+" : "") + (whiteScore / 100).toFixed(2);
  $("info").textContent = `depth ${r.depth}, eval ${scoreText}, ${(r.nodes / 1000).toFixed(0)}k nodes, ${r.ms} ms`;
  const move = pos.legalMoves().find((m) => moveToUci(m) === r.best);
  if (!move) {
    updateStatus();
    return;
  }
  playMove(move);
};

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
  lastMove = null;
  selected = -1;
  targets = [];
  worker.postMessage({ type: "new" });
  viewFlipped = humanColor === BLACK;
  board.setFlipped(viewFlipped);
  $("info").textContent = "";
  refresh();
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
  }
  lastMove = null;
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
};
$<HTMLSelectElement>("side").onchange = newGame;

newGame();
