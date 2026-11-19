#ifndef UTILS_H
#define UTILS_H

#include "typedefs.h"

void reset(Board* board);
void setFen(Board* board, char* fen);
void printChessboard(Board b);
void printBitboard(U64 bb);
void ppBB(U64 bb, int s, int p);
void prettyBB(U64 bb, int s, char p);

#endif
