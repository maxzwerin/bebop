#ifndef BOARD_H
#define BOARD_H

#include "typedefs.h"

void boardReset(Board *board);
void boardSetFen(Board *board, const char *fen);

#endif
