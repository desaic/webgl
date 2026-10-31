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

## Tests

    cd web && npm test

Checks the JS rules against published perft counts, and compares JS legal moves with the C++ move
generator on thousands of random-playout positions. `engine/test_search.mjs` has mate-in-N and
time-budget checks for the search, and `engine/perft.cpp` is the C++ perft suite.
