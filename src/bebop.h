#ifndef BEBOP_H
#define BEBOP_H

#include "bot.h"

/* Summary of a bebopSearch() call, for callers (UCI) that want to report
 * "info depth ... score ... nodes ..." lines. */
typedef struct {
    int depthCompleted; /* last fully-searched depth; 0 if none completed */
    int scoreCp;         /* score in centipawns, from the root side to move */
    bool isMate;          /* true if scoreCp above should be read as a mate */
    int mateInMoves;      /* valid only if isMate; +N engine mates in N, -N gets mated in N */
    long nodes;           /* approximate node count for this search */
} BebopSearchInfo;

/*
 * Direct search entry point for driver code (such as the UCI layer) that
 * needs explicit control over the search budget, depth and cancellation,
 * bypassing the self-play clock heuristics used by the "bebop" BotFunc
 * registered for engine-vs-engine games.
 *
 * budgetMs: hard time budget in milliseconds for this search.
 * maxDepth: hard ply cap; <= 0 (or > 32) means "use the engine's normal
 *           cap" (32).
 * stopFlag: optional. If non-NULL, the search polls *stopFlag periodically
 *           (roughly every 256 nodes) and, once it becomes true, returns
 *           the best move found by the last fully-completed depth as soon
 *           as it is safe to unwind - not instantly, but quickly.
 * infoOut: optional. If non-NULL, filled in with a summary of the search.
 */
Move bebopSearch(Board *board, Move legal[], int legalCount,
                  double budgetMs, int maxDepth,
                  const volatile bool *stopFlag,
                  const BotContext *context,
                  BebopSearchInfo *infoOut);

#endif
