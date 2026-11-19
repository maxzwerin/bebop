#ifndef GAME_H
#define GAME_H

#include "zobrist.h"
#include "bot.h"

typedef enum {
    RESULT_WHITE_WIN,
    RESULT_BLACK_WIN,
    RESULT_DRAW
} ResultWinner;

typedef enum {
    REASON_CHECKMATE,
    REASON_STALEMATE,
    REASON_FIFTY_MOVE,
    REASON_REPETITION,
    REASON_INSUFFICIENT_MATERIAL,
    REASON_TIMEOUT,
    REASON_MOVE_LIMIT,
    REASON_ILLEGAL_NO_MOVE, /* defensive fallback, should not happen */
    REASON_COUNT
} ResultReason;

typedef struct {
    ResultWinner winner;
    ResultReason reason;
    int plyCount;
} GameResult;

#define MAX_GAME_PLIES 600 /* ~300 full moves; avoids runaway games */

double nowMsShared(void);

/* Plays one game. `whiteFn`/`blackFn` are the bots assigned to each
 * color for this particular game. `startFen` may be NULL for the
 * standard start position. `timeMs`/`incMs` set the bullet time
 * control (milliseconds). If `moveLog` is non-NULL, encoded moves are
 * appended to it (caller must provide a buffer of at least
 * MAX_GAME_PLIES Moves) and `*moveCountOut` is set. */
GameResult playGame(BotFunc whiteFn, BotFunc blackFn, const char *startFen,
                     double timeMs, double incMs,
                     Move *moveLog, int *moveCountOut);

const char *reasonName(ResultReason r);

#endif
