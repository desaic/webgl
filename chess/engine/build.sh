#!/bin/sh
# Builds the C++ engine (existing rules in ../client + new Search) to WebAssembly.
# Needs emsdk: set EM_CONFIG to its .emscripten file, e.g. EM_CONFIG=/f/github/emsdk/.emscripten
set -e
cd "$(dirname "$0")"
EMCC=${EMCC:-/f/github/emsdk/upstream/emscripten/emcc}
mkdir -p ../web/src/engine
$EMCC -O3 -std=c++17 -I../client wasm_api.cpp Search.cpp Book.cpp ../client/ChessBoard.cpp ../client/BoardHash.cpp \
  ../client/BoardEval.cpp -o ../web/src/engine/engine.mjs \
  -sMODULARIZE -sEXPORT_ES6 -sENVIRONMENT=worker,node -sALLOW_MEMORY_GROWTH \
  -sEXPORTED_FUNCTIONS=_engine_search,_engine_book_depth,_engine_new_game,_engine_legal_moves,_engine_perft,_malloc,_free \
  -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString -sSINGLE_FILE
