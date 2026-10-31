#!/bin/sh
# Regenerates engine/book_data.h (the opening book compiled into the wasm engine).
#
#   node tools/pgn_to_lines.mjs ignore/book_lines.txt <games.pgn>...     # strong games -> opening lines
#   sh engine/build_book.sh [min_count] [max_ply]                        # lines -> book_data.h
#
# The book keys use the engine's own hash, so this is built with the same emscripten as the engine.
set -e
cd "$(dirname "$0")"
EMCC=${EMCC:-/f/github/emsdk/upstream/emscripten/emcc}
MIN=${1:-2}
PLY=${2:-16}
$EMCC -O2 -std=c++17 -I../client make_book.cpp ../client/ChessBoard.cpp ../client/BoardHash.cpp \
  -o make_book.js -sNODERAWFS=1 -sENVIRONMENT=node -sALLOW_MEMORY_GROWTH 2>/dev/null
node make_book.js ../ignore/book_lines.txt book_data.h "$MIN" "$PLY"
rm -f make_book.js make_book.wasm
