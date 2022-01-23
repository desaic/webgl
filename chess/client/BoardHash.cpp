#pragma once

#include "BoardHash.h"
#include "ChessBoard.h"

#include <cstdlib>

#define a3 16
#define a4 24
#define a6 40
#define h3 23
#define h4 31
#define a5 32
#define h5 39
#define h6 47

uint64_t BoardHash::positionTable[NUM_SQUARES][NUM_PIECES];
uint64_t BoardHash::castlingTable[CASTLING_OPTIONS];
uint64_t BoardHash::enpassantTable[ENPASSANT_OPTIONS];
uint64_t BoardHash::side;

uint64_t randomNumber() {
	return
		(((uint64_t)std::rand() << 0) & 0x000000000000FFFFull)	 |
		(((uint64_t)std::rand() << 16) & 0x00000000FFFF0000ull) |
		(((uint64_t)std::rand() << 32) & 0x0000FFFF00000000ull) |
		(((uint64_t)std::rand() << 48) & 0xFFFF000000000000ull);
}
	
void BoardHash::Init() {
	std::srand(123456789);
	for (uint8_t s = 0; s < NUM_SQUARES; ++s) {
			for (uint8_t p = 0; p < NUM_PIECES; ++p) {
				positionTable[s][p] = randomNumber();
			}
	}

	for (uint8_t i = 0; i < CASTLING_OPTIONS; ++i) {
		castlingTable[i] = randomNumber();
	}

	for (uint8_t i = 0; i < CASTLING_OPTIONS; ++i) {
		enpassantTable[i] = randomNumber();
	}

	side = randomNumber();
}

// For initializing a fresh board position
void BoardHash::Set(const ChessBoard& b) {
	hash = 0;
	for (size_t i = 0; i < b.board.size(); i++) {
		hash ^= positionTable[i][b.GetPiece(i)->info];
	}

	if (b.hasEnPassant) {
		hash ^= enpassantTable[b.enPassantDst.Col()];
	}

	if (b.castleBK) hash ^= castlingTable[0];
	if (b.castleBQ) hash ^= castlingTable[1];
	if (b.castleWK) hash ^= castlingTable[2];
	if (b.castleWQ) hash ^= castlingTable[3];

	if (b.nextColor == PIECE_BLACK) {
		hash ^= side;
	}
}

void BoardHash::Update(const ChessBoard& b, const Move& m) {
	// undo the source
	hash ^= positionTable[m.src.coord][b.GetPiece(m.src.coord)->info];

	// add the dst position
	uint8_t piece = m.promo == PIECE_EMPTY ? b.GetPiece(m.src.coord)->info : m.promo.info;
	hash ^= positionTable[m.dst.coord][piece];

	// captures
	const Piece* capturedPiece = b.GetPiece(m.dst.coord);
	if (capturedPiece != PIECE_EMPTY) {
		hash ^= positionTable[m.dst.coord][capturedPiece->info];
	}

	// castles
	if (b.GetPiece(m.src.coord)->type() == PIECE_KING) {
		if (b.nextColor == PIECE_BLACK) {
			if (b.castleBK) hash ^= castlingTable[0];
			if (b.castleBQ) hash ^= castlingTable[1];
		} else if (b.nextColor == PIECE_WHITE) {
			if (b.castleWK) hash ^= castlingTable[2];
			if (b.castleWQ) hash ^= castlingTable[3];
		}

		if (m.src.coord == m.dst.coord - 2) {
			hash ^= positionTable[m.dst.coord - 1][PIECE_ROOK];
			hash ^= positionTable[m.dst.coord + 2][PIECE_ROOK];
		}
		if (m.src.coord == m.dst.coord + 2) {
			hash ^= positionTable[m.dst.coord - 1][PIECE_ROOK];
			hash ^= positionTable[m.dst.coord + 1][PIECE_ROOK];
		}
	}

	if (b.GetPiece(m.src.coord)->type() == PIECE_ROOK) {
		if (m.src.Col() == 0) {
			if (b.nextColor == PIECE_BLACK) {
				if (b.castleBQ) hash ^= castlingTable[1];
			} else {
				if (b.castleWQ) hash ^= castlingTable[3];
			}
		}
		if (m.src.Col() == 7) {
			if (b.nextColor == PIECE_BLACK) {
				if (b.castleBK) hash ^= castlingTable[0];
			} else {
				if (b.castleWK) hash ^= castlingTable[2];
			}
		}
	}

	// en passant
	if (b.hasEnPassant) {
		hash ^= enpassantTable[b.enPassantDst.Col()];
	}

	if (b.GetPiece(m.src.coord)->type() == PIECE_PAWN) {
		if (abs(m.dst.Row() - m.src.Row()) == 2) {
			bool hasEnpassant = false;
			if (m.dst.coord != a4&& m.dst.coord != a5) {
				if (b.GetPiece(m.dst.coord - 1)->type() == PIECE_PAWN) {
					hasEnpassant=true;
				}
			}
			else if (m.dst.coord != h4 && m.dst.coord != h5 && !hasEnpassant) {
				if (b.GetPiece(m.dst.coord + 1)->type() == PIECE_PAWN) {
					hasEnpassant=true;
				}
			}
			if (hasEnpassant) {
				hash ^= enpassantTable[m.src.Col()];
			}
		}

		// en passant capture
		if (m.src.Col() != m.dst.Col()) {
			if (b.GetPiece(m.dst.coord) == PIECE_EMPTY) {
				hash ^= positionTable[m.dst.coord][PIECE_PAWN];
			}
		}
	}

	//side
	hash ^= side;
}