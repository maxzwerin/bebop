#ifndef ZOBRIST_H
#define ZOBRIST_H

#include "movegen.h"

void initZobrist(void);
U64 zobristHash(Board *b);

#endif
