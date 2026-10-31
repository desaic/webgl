# Chess

Client-only chess: everything runs in the browser, nothing talks to a server.

- `web/` - UI in TypeScript + three.js (Vite). `src/rules.ts` is the rules code the UI uses for
  legal-move highlighting, promotion, check/mate/draw detection and move notation.
- `engine/` - the AI, C++ compiled to WebAssembly with emscripten and run in a web worker
  (`web/src/worker.ts`). `Search.cpp` is a plain recursive negamax alpha-beta with quiescence,
  a transposition table, killer/MVV-LVA move ordering and iterative deepening. The search polls a
  deadline and unwinds, so a move always comes back within the think-time budget.
- `client/` - the original C++ board/rules code (`ChessBoard`, `BoardHash`, `BoardEval`). The engine
  reuses it unchanged except for one fix (en passant squares in FEN input). The old websocket client
  and its rolled-out search are no longer used.

## Build and run

    sh engine/build.sh          # needs emsdk; writes web/src/engine/engine.mjs (checked in, so optional)
    cd web && npm install
    npm run dev                 # http://localhost:9006
    npm run build               # static site in web/dist

## Piece meshes

The pieces come from `ignore/pieces.blend` (one mesh made of 16 loose parts: a single set of pieces).
Only the checked-in results in `web/public/pieces/*.obj` are needed to run the game; to regenerate:

    blender -b ignore/pieces.blend --python tools/extract_pieces.py   # full-res OBJs in tools/raw/ (git-ignored)
    cd web && npm run pieces                                   # meshoptimizer -> web/public/pieces/

The OBJs contain only positions and triangles (no normals, UVs or colors). `simplify.mjs` reduces
each piece from 95k-447k triangles to 1.2k-3.6k, and `Pieces.ts` draws them flat shaded in one
solid color per side. `tools/inspect_*.py` list what is in the blend.

## Sounds

`ignore/wood_table.wav` (29 s of wood taps, 96 kHz) is the source for the four move sounds in
`web/public/sounds/` (move, capture, castle, promotion). `tools/analyze_sounds.py` lists every hit in
the recording (time, level, brightness); `tools/extract_sounds.py` cuts the chosen ones, with a short
fade, normalised level and mono 44.1 kHz output. Edit the `SOUNDS` table (start/end seconds) to pick other
hits. Needs Python with numpy and ffmpeg. `Sounds.ts` plays them with Web Audio when a piece lands, and
`Position.kind()` in `rules.ts` decides which one (promotion, castle, capture or plain move).

## Opening book and opening names

Two small generated tables, both from data kept in the git-ignored `ignore/` folder:

- **Engine book** (`engine/book_data.h`, compiled into the wasm). Built from the TWIC game collections
  in `F:\github\chess` (`twic1489.pgn`, `twic920.pgn`): games where both players are rated 2300+, the
  first 16 half-moves, and every (position, move) pair seen at least twice. It is 3,745 entries
  (30 KB of tables, about 44 KB added to the engine). At run time the engine looks the position up by its
  Zobrist hash and plays a random book move, weighted by how often it was played, without searching.
  The "Opening book" checkbox switches it off.

      node tools/pgn_to_lines.mjs ignore/book_lines.txt F:/github/chess/twic1489.pgn F:/github/chess/twic920.pgn
      sh engine/build_book.sh            # lines -> engine/book_data.h (same emscripten as the engine)
      sh engine/build.sh                 # rebuild the wasm

- **Opening names** (`web/public/openings.json`, 3,236 positions, about 55 KB gzipped, loaded after the
  first paint). From the lichess chess-openings TSVs (https://github.com/lichess-org/chess-openings, CC0,
  downloaded to `ignore/openings/{a,b,c,d,e}.tsv`), lines up to 14 half-moves. Positions are keyed by hash,
  so transpositions are recognised.

      node tools/build_openings.mjs ignore/openings

**Variety.** `web/src/openingPlans.ts` holds 20 popular openings as full lines (Ruy Lopez, Italian,
Scotch, Petrov, three Sicilians, French, Caro-Kann, Scandinavian, Pirc, QGD, QGA, Slav, King's Indian,
Nimzo-Indian, Grünfeld, London, English, Réti). On each of its turns the engine picks one at random from
the lines that fit the game so far and plays its side of it; if you deviate it picks another fitting line,
and when no line fits (or the line ends) it falls back to the weighted book above, then to search. So it
opens differently from game to game, as White or Black. The "Opening book" checkbox switches all of
this off.

The box above the move list shows the name of the latest named opening in the game and the moves that
make up the opening (up to the last named position or last book move). With no name it shows only the moves;
with neither it is hidden (`web/src/opening.ts`).

## Saved game

The game in progress is kept in `localStorage` (key `chess.game.v1`) after every move, undo, flip and
settings change: the moves from the start position, the current FEN, which side you play, the view, the
think time and the book setting. On page load every saved move is replayed against the rules and the game
is restored (if the engine was to move it carries on); a missing or damaged save falls back to the default
start position. "New game" always starts from the default position and replaces the save.

## Captured pieces

Captured pieces are kept on two trays beside the board in the 3D scene (white ones on the viewer's left,
black on the right, swapping with the view when the board is flipped), at about 60% size, sorted pawn,
knight, bishop, rook, queen, filling 4 rows of 4: a row runs outward from the board and the first row is
at the viewer's end (`Board3D.setCaptured`).
`Position.capturedPieces()` records captures as they happen, so promotions and undo are handled exactly.

## Tests

    cd web && npm test

Checks the JS rules against published perft counts, and compares JS legal moves with the C++ move
generator on thousands of random-playout positions. `engine/test_search.mjs` has mate-in-N and
time-budget checks for the search, and `engine/perft.cpp` is the C++ perft suite.
