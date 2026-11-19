#include "perft.h"

uint64_t perftNodes(Board *board, int depth) {
    if (depth <= 0) return 1;

    Move moves[MAX_MOVES];
    int count = legalMoves(board, moves);
    if (depth == 1) return (uint64_t)count;

    uint64_t nodes = 0;
    for (int i = 0; i < count; ++i) {
        Undo undo;
        doMove(board, moves[i], &undo);
        nodes += perftNodes(board, depth - 1);
        undoMove(board, moves[i], &undo);
    }
    return nodes;
}
