#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "utils.h"

extern U64 KNIGHT_REF[64];
extern U64 KING_REF[64];
extern U64 ROOK_REF[64];
extern U64 BISHOP_REF[64];
extern U64 PAWN_ATTACKS[2][64];

void initMoveGen(void);

U64 getBishopAttacks(int square, U64 occupancy);
U64 getRookAttacks(int square, U64 occ);
U64 getKnightAttacks(int sq);
U64 getKingAttacks(int sq);
U64 getPawnMoves(int square, Board *board, int color);

bool isSquareAttacked(Board *b, int sq, int attacker);
bool isSquareAttackedWithOcc(Board *b, int sq, int attacker, U64 occ);
void clearCastleRights(Board *board, int clear);

void doMove(Board *board, Move move, Undo *undo);
void undoMove(Board *board, Move move, Undo *undo);
void makeMove(Board *board, Move move);

bool canCastle(Board *board, int right, U64 occ);
int pseudoMoves(Board *board, Move moves[]);
int legalMoves(Board *board, Move legal[]);

#endif
