#include "../bot.h"
#include <time.h>
#include <limits.h>

/* ===========================================================================
 * "material" bot: classic material + piece-square-table evaluation, with
 * negamax/alpha-beta search, iterative deepening, and a small quiescence
 * search on captures to reduce the horizon effect.
 *
 * This file is meant to be a template: copy it (e.g. bots/material_v2.c),
 * change EVALUATE / search, and register it under a new name to compare
 * against this baseline. Nothing outside bots/ needs to change - see
 * bot.h for the REGISTER_BOT mechanism.
 * ===========================================================================
 */

static double nowMs(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

/* ---------------------------------------------------------------------
 * Evaluation: material + piece-square tables, from White's perspective
 * (positive = good for White). All tables are written visually,
 * rank 8 first (as you'd read a board diagram), and converted to the
 * engine's a1=0 square indexing at startup in initEvalTables().
 * ------------------------------------------------------------------- */

static const int PIECE_VALUE[6] = {
    100,  /* PAWN   */
    320,  /* KNIGHT */
    330,  /* BISHOP */
    500,  /* ROOK   */
    900,  /* QUEEN  */
    0,    /* KING - not counted as material */
};

static const int PAWN_PST_VISUAL[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
     5,  5, 10, 25, 25, 10,  5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5, -5,-10,  0,  0,-10, -5,  5,
     5, 10, 10,-20,-20, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0,
};
static const int KNIGHT_PST_VISUAL[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50,
};
static const int BISHOP_PST_VISUAL[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20,
};
static const int ROOK_PST_VISUAL[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10, 10, 10, 10, 10,  5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     0,  0,  0,  5,  5,  0,  0,  0,
};
static const int QUEEN_PST_VISUAL[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
     -5,  0,  5,  5,  5,  5,  0, -5,
      0,  0,  5,  5,  5,  5,  0, -5,
    -10,  5,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20,
};
/* King safety in the middlegame: stay behind pawns, away from the center.
 * (No separate endgame king table yet - a natural thing to add if you
 * fork this file.) */
static const int KING_PST_VISUAL[64] = {
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -10,-20,-20,-20,-20,-20,-20,-10,
     20, 20,  0,  0,  0,  0, 20, 20,
     20, 30, 10,  0,  0, 10, 30, 20,
};

static int PST[6][64]; /* [piece][square], White's perspective */

static void initEvalTables(void) {
    const int *visual[6] = {
        PAWN_PST_VISUAL, KNIGHT_PST_VISUAL, BISHOP_PST_VISUAL,
        ROOK_PST_VISUAL, QUEEN_PST_VISUAL, KING_PST_VISUAL,
    };
    for (int p = 0; p < 6; p++) {
        for (int vIdx = 0; vIdx < 64; vIdx++) {
            int rankFromTop = vIdx / 8;
            int file = vIdx % 8;
            int sq = (7 - rankFromTop) * 8 + file; /* a1=0 indexing */
            PST[p][sq] = visual[p][vIdx];
        }
    }
}

static int pieceAt(Board *b, int sq, int *isWhiteOut) {
    U64 mask = BIT(sq);
    U64 *bbs[6] = { &b->pawnBB, &b->knightBB, &b->bishopBB, &b->rookBB, &b->queenBB, &b->kingBB };
    for (int p = 0; p < 6; p++) {
        if (*bbs[p] & mask) {
            *isWhiteOut = (b->whiteBB & mask) != 0;
            return p;
        }
    }
    return -1;
}

static int evaluate(Board *b) {
    int score = 0;
    U64 *bbs[6] = { &b->pawnBB, &b->knightBB, &b->bishopBB, &b->rookBB, &b->queenBB, &b->kingBB };

    for (int p = 0; p < 6; p++) {
        U64 white = *bbs[p] & b->whiteBB;
        U64 black = *bbs[p] & b->blackBB;

        while (white) {
            int sq = POP_LSB(white);
            score += PIECE_VALUE[p] + PST[p][sq];
        }
        while (black) {
            int sq = POP_LSB(black);
            score -= PIECE_VALUE[p] + PST[p][sq ^ 56]; /* mirror rank for Black */
        }
    }

    return score;
}

/* ---------------------------------------------------------------------
 * Move ordering: captures first (highest captured value first), then
 * everything else in generation order. Cheap MVV-only ordering - a
 * natural upgrade is MVV-LVA, killer moves, or a TT move hint.
 * ------------------------------------------------------------------- */
static void orderMoves(Board *board, Move moves[], int count) {
    int scores[MAX_MOVES];
    for (int i = 0; i < count; i++) {
        int to = EXTRACT_TO(moves[i]);
        int isWhite;
        int captured = pieceAt(board, to, &isWhite);
        scores[i] = (captured >= 0) ? (100 + PIECE_VALUE[captured]) : 0;
    }
    /* insertion sort descending by score - fine for MAX_MOVES ~ 218 */
    for (int i = 1; i < count; i++) {
        Move m = moves[i];
        int s = scores[i];
        int j = i - 1;
        while (j >= 0 && scores[j] < s) {
            moves[j + 1] = moves[j];
            scores[j + 1] = scores[j];
            j--;
        }
        moves[j + 1] = m;
        scores[j + 1] = s;
    }
}

/* ---------------------------------------------------------------------
 * Search: negamax with alpha-beta, iterative deepening, and a
 * captures-only quiescence search at the leaves.
 * ------------------------------------------------------------------- */

#define MATE_SCORE 1000000
#define MAX_PLY 64

static double g_deadlineMs;
static bool g_timeUp;
static long g_nodes;

static int quiescence(Board *board, int alpha, int beta, int ply) {
    if ((g_nodes++ & 1023) == 0 && nowMs() > g_deadlineMs) g_timeUp = true;
    if (g_timeUp || ply >= MAX_PLY) return evaluate(board) * (board->turn ? 1 : -1);

    int standPat = evaluate(board) * (board->turn ? 1 : -1);
    if (standPat >= beta) return beta;
    if (standPat > alpha) alpha = standPat;

    Move legal[MAX_MOVES];
    int count = legalMoves(board, legal);
    orderMoves(board, legal, count);

    U64 enemy = board->turn ? board->blackBB : board->whiteBB;

    for (int i = 0; i < count; i++) {
        int to = EXTRACT_TO(legal[i]);
        if (!(BIT(to) & enemy) && !IS_PROMO(EXTRACT_FLAG(legal[i]))) continue; /* captures/promos only */

        Undo undo;
        doMove(board, legal[i], &undo);
        int score = -quiescence(board, -beta, -alpha, ply + 1);
        undoMove(board, legal[i], &undo);

        if (g_timeUp) return alpha;

        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }

    return alpha;
}

static int negamax(Board *board, int depth, int alpha, int beta, int ply, Move *bestOut) {
    if ((g_nodes++ & 1023) == 0 && nowMs() > g_deadlineMs) { g_timeUp = true; return 0; }

    Move legal[MAX_MOVES];
    int count = legalMoves(board, legal);

    if (count == 0) {
        int kingSq = LSB(board->kingBB & (board->turn ? board->whiteBB : board->blackBB));
        bool inCheck = isSquareAttacked(board, kingSq, !board->turn);
        return inCheck ? -(MATE_SCORE - ply) : 0;
    }

    if (depth == 0) return quiescence(board, alpha, beta, ply);

    orderMoves(board, legal, count);

    Move localBest = legal[0];
    int best = INT_MIN;

    for (int i = 0; i < count; i++) {
        Undo undo;
        doMove(board, legal[i], &undo);
        int score = -negamax(board, depth - 1, -beta, -alpha, ply + 1, NULL);
        undoMove(board, legal[i], &undo);

        if (g_timeUp) return 0;

        if (score > best) {
            best = score;
            localBest = legal[i];
        }
        if (score > alpha) alpha = score;
        if (alpha >= beta) break; /* beta cutoff */
    }

    if (bestOut) *bestOut = localBest;
    return best;
}

static Move materialBot(Board *board, Move legal[], int legalCount, double timeLeftMs, double incMs) {
    static bool initialized = false;
    if (!initialized) { initEvalTables(); initialized = true; }

    if (legalCount == 1) return legal[0]; /* forced move, don't burn time searching */

    /* Simple time budget: assume ~30 moves left, bank most of the
     * increment, keep a safety margin so we don't flag. */
    double budget = timeLeftMs / 30.0 + incMs * 0.8;
    if (budget > timeLeftMs * 0.5) budget = timeLeftMs * 0.5; /* never use more than half the clock on one move */
    if (budget < 5.0) budget = 5.0;

    g_deadlineMs = nowMs() + budget;
    g_timeUp = false;
    g_nodes = 0;

    Move bestMove = legal[0];

    for (int depth = 1; depth <= 32; depth++) {
        Move depthBest;
        negamax(board, depth, -MATE_SCORE - 1, MATE_SCORE + 1, 0, &depthBest);

        if (g_timeUp) break;
        bestMove = depthBest;

        if (nowMs() > g_deadlineMs) break;
    }

    return bestMove;
}

REGISTER_BOT("material", materialBot);
