#ifndef PGN_H
#define PGN_H

#include "game.h"

/*
 * Write one complete PGN game to an already-open stream.
 *
 * moves[] is the chronological move log returned by playGame(). The writer
 * reconstructs SAN by replaying those moves from startFen (or START_FEN when
 * startFen is NULL). Returns false if a move cannot be reconstructed or the
 * output stream reports an error.
 */
bool pgnWriteGame(FILE *out,
                  int gameNumber,
                  const char *whiteName,
                  const char *blackName,
                  const char *startFen,
                  const Move moves[],
                  int moveCount,
                  GameResult result);

#endif
