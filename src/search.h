#ifndef SEARCH_H
#define SEARCH_H

#include "movegen.h"
#include <stdatomic.h>

typedef struct {
    const U64 *positionHistory;
    int positionHistoryCount;
} SearchContext;

typedef struct {
    int depthCompleted;
    int scoreCp;
    bool isMate;
    int mateInMoves;
    long nodes;
} SearchInfo;

Move searchBestMove(Board *board, Move legal[], int legalCount,
                    double budgetMs, int maxDepth,
                    const atomic_bool *stopFlag,
                    const SearchContext *context, SearchInfo *infoOut);

#endif
