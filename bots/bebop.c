#include "bot.h"
#include <time.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define MATE_SCORE       1000000
#define MAX_PLY          64
#define MAX_QPLY         12
#define MAX_CHECK_EXT    2
#define INF_SCORE        (MATE_SCORE + 1)
#define KILLER_SCORE_1   800000
#define KILLER_SCORE_2   790000

static double nowMs(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

static const int PIECE_VALUE[6] = { 100, 320, 330, 500, 900, 0 };

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

static int PST[6][64];
static bool g_tablesReady = false;

static void initEvalTables(void) {
    const int *visual[6] = {
        PAWN_PST_VISUAL, KNIGHT_PST_VISUAL, BISHOP_PST_VISUAL,
        ROOK_PST_VISUAL, QUEEN_PST_VISUAL, KING_PST_VISUAL
    };

    for (int p = 0; p < 6; ++p) {
        for (int v = 0; v < 64; ++v) {
            int rankFromTop = v / 8;
            int file = v % 8;
            int sq = (7 - rankFromTop) * 8 + file;
            PST[p][sq] = visual[p][v];
        }
    }
    g_tablesReady = true;
}

static int pieceAt(Board *board, int sq, int *isWhiteOut) {
    U64 mask = BIT(sq);
    U64 *bbs[6] = {
        &board->pawnBB, &board->knightBB, &board->bishopBB,
        &board->rookBB, &board->queenBB, &board->kingBB
    };

    for (int p = 0; p < 6; ++p) {
        if (*bbs[p] & mask) {
            if (isWhiteOut) *isWhiteOut = (board->whiteBB & mask) != 0;
            return p;
        }
    }
    return -1;
}

static int kingSquare(Board *board, bool white) {
    U64 king = board->kingBB & (white ? board->whiteBB : board->blackBB);
    return king ? LSB(king) : -1;
}

static bool sideInCheck(Board *board) {
    int sq = kingSquare(board, board->turn);
    if (sq < 0) return false;
    return isSquareAttacked(board, sq, !board->turn);
}

static int centralizationBonus(int sq) {
    int file = sq & 7;
    int rank = sq >> 3;
    int distance = abs(2 * file - 7) + abs(2 * rank - 7);
    return (14 - distance) * 5;
}

/* Returns a White-perspective pawn-structure score. */
static int evaluatePawns(Board *board, int phase) {
    int score = 0;
    int whiteFiles[8] = {0};
    int blackFiles[8] = {0};
    int whiteSquares[16], blackSquares[16];
    int whiteCount = 0, blackCount = 0;

    U64 whitePawns = board->pawnBB & board->whiteBB;
    U64 blackPawns = board->pawnBB & board->blackBB;
    U64 tmp = whitePawns;
    while (tmp) {
        int sq = POP_LSB(tmp);
        if (whiteCount < 16) whiteSquares[whiteCount++] = sq;
        whiteFiles[sq & 7]++;
    }
    tmp = blackPawns;
    while (tmp) {
        int sq = POP_LSB(tmp);
        if (blackCount < 16) blackSquares[blackCount++] = sq;
        blackFiles[sq & 7]++;
    }

    /* Doubled and isolated pawns. */
    for (int f = 0; f < 8; ++f) {
        if (whiteFiles[f] > 1) score -= 14 * (whiteFiles[f] - 1);
        if (blackFiles[f] > 1) score += 14 * (blackFiles[f] - 1);

        if (whiteFiles[f] > 0 && (f == 0 ? whiteFiles[1] == 0 :
            f == 7 ? whiteFiles[6] == 0 :
            whiteFiles[f - 1] == 0 && whiteFiles[f + 1] == 0)) {
            score -= 12 * whiteFiles[f];
        }
        if (blackFiles[f] > 0 && (f == 0 ? blackFiles[1] == 0 :
            f == 7 ? blackFiles[6] == 0 :
            blackFiles[f - 1] == 0 && blackFiles[f + 1] == 0)) {
            score += 12 * blackFiles[f];
        }
    }

    /* Passed pawns: no opposing pawn ahead on the same or adjacent file. */
    for (int i = 0; i < whiteCount; ++i) {
        int sq = whiteSquares[i];
        int f = sq & 7, r = sq >> 3;
        bool passed = true;
        for (int j = 0; j < blackCount; ++j) {
            int enemySq = blackSquares[j];
            int ef = enemySq & 7, er = enemySq >> 3;
            if (abs(ef - f) <= 1 && er > r) { passed = false; break; }
        }
        if (passed) {
            int bonus = 8 + r * r * 2;
            bonus = bonus * (24 - phase) / 24 + bonus / 3;
            score += bonus;
        }
    }
    for (int i = 0; i < blackCount; ++i) {
        int sq = blackSquares[i];
        int f = sq & 7, r = sq >> 3;
        bool passed = true;
        for (int j = 0; j < whiteCount; ++j) {
            int enemySq = whiteSquares[j];
            int ef = enemySq & 7, er = enemySq >> 3;
            if (abs(ef - f) <= 1 && er < r) { passed = false; break; }
        }
        if (passed) {
            int bonus = 8 + (7 - r) * (7 - r) * 2;
            bonus = bonus * (24 - phase) / 24 + bonus / 3;
            score -= bonus;
        }
    }

    return score;
}

static int evaluate(Board *board) {
    U64 *bbs[6] = {
        &board->pawnBB, &board->knightBB, &board->bishopBB,
        &board->rookBB, &board->queenBB, &board->kingBB
    };

    int mgScore = 0;
    int egScore = 0;
    int phase = 0;
    int whiteBishops = __builtin_popcountll(board->bishopBB & board->whiteBB);
    int blackBishops = __builtin_popcountll(board->bishopBB & board->blackBB);

    /* Standard 24-point material phase. */
    phase += __builtin_popcountll(board->knightBB) * 1;
    phase += __builtin_popcountll(board->bishopBB) * 1;
    phase += __builtin_popcountll(board->rookBB) * 2;
    phase += __builtin_popcountll(board->queenBB) * 4;
    if (phase > 24) phase = 24;

    for (int p = 0; p < 6; ++p) {
        U64 white = *bbs[p] & board->whiteBB;
        U64 black = *bbs[p] & board->blackBB;

        while (white) {
            int sq = POP_LSB(white);
            mgScore += PIECE_VALUE[p];
            egScore += PIECE_VALUE[p];
            if (p == 5) {
                mgScore += PST[p][sq];
                egScore += centralizationBonus(sq);
            } else {
                mgScore += PST[p][sq];
                egScore += PST[p][sq];
            }
        }
        while (black) {
            int sq = POP_LSB(black);
            int mirrored = sq ^ 56;
            mgScore -= PIECE_VALUE[p];
            egScore -= PIECE_VALUE[p];
            if (p == 5) {
                mgScore -= PST[p][mirrored];
                egScore -= centralizationBonus(mirrored);
            } else {
                mgScore -= PST[p][mirrored];
                egScore -= PST[p][mirrored];
            }
        }
    }

    int score = (mgScore * phase + egScore * (24 - phase)) / 24;

    /* Small but useful positional terms. */
    if (whiteBishops >= 2) score += 30;
    if (blackBishops >= 2) score -= 30;
    score += evaluatePawns(board, phase);

    /* Rooks like open and semi-open files. */
    U64 whiteRooks = board->rookBB & board->whiteBB;
    U64 blackRooks = board->rookBB & board->blackBB;
    while (whiteRooks) {
        int sq = POP_LSB(whiteRooks);
        int f = sq & 7;
        bool ownPawn = false, enemyPawn = false;
        for (int r = 0; r < 8; ++r) {
            U64 mask = BIT(r * 8 + f);
            ownPawn |= (board->pawnBB & board->whiteBB & mask) != 0;
            enemyPawn |= (board->pawnBB & board->blackBB & mask) != 0;
        }
        if (!ownPawn) score += enemyPawn ? 12 : 22;
    }
    while (blackRooks) {
        int sq = POP_LSB(blackRooks);
        int f = sq & 7;
        bool ownPawn = false, enemyPawn = false;
        for (int r = 0; r < 8; ++r) {
            U64 mask = BIT(r * 8 + f);
            ownPawn |= (board->pawnBB & board->blackBB & mask) != 0;
            enemyPawn |= (board->pawnBB & board->whiteBB & mask) != 0;
        }
        if (!ownPawn) score -= enemyPawn ? 12 : 22;
    }

    /* Reward a modest pawn shield in front of each king. */
    int wk = kingSquare(board, true);
    int bk = kingSquare(board, false);
    if (wk >= 0) {
        int kf = wk & 7, kr = wk >> 3;
        for (int df = -1; df <= 1; ++df) {
            int f = kf + df;
            if (f < 0 || f > 7) continue;
            for (int dr = 1; dr <= 2; ++dr) {
                int r = kr + dr;
                if (r >= 0 && r < 8 && (board->pawnBB & board->whiteBB & BIT(r * 8 + f)))
                    score += 7;
            }
        }
    }
    if (bk >= 0) {
        int kf = bk & 7, kr = bk >> 3;
        for (int df = -1; df <= 1; ++df) {
            int f = kf + df;
            if (f < 0 || f > 7) continue;
            for (int dr = 1; dr <= 2; ++dr) {
                int r = kr - dr;
                if (r >= 0 && r < 8 && (board->pawnBB & board->blackBB & BIT(r * 8 + f)))
                    score -= 7;
            }
        }
    }

    /* Tempo: being the side to move has a small practical value. */
    score += board->turn ? 8 : -8;
    return score;
}

static double g_deadlineMs;
static bool g_timeUp;
static long g_nodes;
static Move g_killers[MAX_PLY][2];
static bool g_killerValid[MAX_PLY][2];
static U64 g_searchHashes[MAX_PLY];

/* Called after a hypothetical move has been made. The real game history does
 * not yet contain this search position, so count prior real-game occurrences
 * plus earlier positions on the current search line. If there are already two,
 * the current hypothetical position would be the third occurrence.
 */
static bool isThirdRepetition(Board *board, int ply,
                              const BotContext *context) {
    /* The game history supplied by playGame() already includes the root
     * position. The current hypothetical position is not in that history.
     * Search-path hashes [1, ply) are the hypothetical ancestors between
     * the root and this candidate position; exclude the candidate itself.
     */
    if (ply <= 0 || !context || !context->positionHash) return false;

    U64 hash = context->positionHash(board);
    int priorOccurrences = 0;

    if (context->positionHistory) {
        for (int i = 0; i < context->positionHistoryCount; ++i) {
            if (context->positionHistory[i] == hash && ++priorOccurrences >= 2)
                return true;
        }
    }

    for (int i = 1; i < ply && priorOccurrences < 2; ++i) {
        if (g_searchHashes[i] == hash) ++priorOccurrences;
    }

    return priorOccurrences >= 2;
}

static bool moveIsCapture(Board *board, Move move) {
    int to = EXTRACT_TO(move);
    int sideIsWhite = 0;
    int captured = pieceAt(board, to, &sideIsWhite);
    return captured >= 0 && sideIsWhite != board->turn;
}

static bool moveIsPromotion(Move move) {
    return IS_PROMO(EXTRACT_FLAG(move));
}

static int scoreMove(Board *board, Move move, int ply) {
    int score = 0;
    int to = EXTRACT_TO(move);
    int capturedIsWhite = 0;
    int captured = pieceAt(board, to, &capturedIsWhite);

    if (captured >= 0 && capturedIsWhite != board->turn) {
        /* MVV ordering: queen/rook captures before smaller victims. */
        score += 1000000 + PIECE_VALUE[captured] * 10;
    }
    if (moveIsPromotion(move)) score += 900000;

    if (ply < MAX_PLY) {
        if (g_killerValid[ply][0] && move == g_killers[ply][0]) score += KILLER_SCORE_1;
        else if (g_killerValid[ply][1] && move == g_killers[ply][1]) score += KILLER_SCORE_2;
    }
    return score;
}

static void orderMoves(Board *board, Move moves[], int count, int ply) {
    int scores[MAX_MOVES];
    for (int i = 0; i < count; ++i) scores[i] = scoreMove(board, moves[i], ply);

    /* Insertion sort is fast enough for normal chess move-list sizes. */
    for (int i = 1; i < count; ++i) {
        Move move = moves[i];
        int score = scores[i];
        int j = i - 1;
        while (j >= 0 && scores[j] < score) {
            moves[j + 1] = moves[j];
            scores[j + 1] = scores[j];
            --j;
        }
        moves[j + 1] = move;
        scores[j + 1] = score;
    }
}

static void rememberKiller(Move move, int ply) {
    if (ply < 0 || ply >= MAX_PLY) return;
    if (g_killerValid[ply][0] && g_killers[ply][0] == move) return;
    g_killers[ply][1] = g_killers[ply][0];
    g_killerValid[ply][1] = g_killerValid[ply][0];
    g_killers[ply][0] = move;
    g_killerValid[ply][0] = true;
}

static bool timeExpired(void) {
    if ((g_nodes++ & 255) == 0 && nowMs() >= g_deadlineMs) g_timeUp = true;
    return g_timeUp;
}

/*
 * Quiescence search. Critical detail: when the side to move is in check,
 * stand-pat is illegal and every legal evasion must be searched, not just
 * captures/promotions.
 */
static int quiescence(Board *board, int alpha, int beta, int ply, int qply,
                      const BotContext *context) {
    if (timeExpired()) return 0;
    if (ply >= MAX_PLY - 1) return evaluate(board) * (board->turn ? 1 : -1);

    bool inCheck = sideInCheck(board);

    /* Detect mate/stalemate before stand-pat or beta cutoffs. */
    Move moves[MAX_MOVES];
    int count = legalMoves(board, moves);
    if (count == 0) return inCheck ? -(MATE_SCORE - ply) : 0;

    int standPat = evaluate(board) * (board->turn ? 1 : -1);
    if (!inCheck) {
        if (standPat >= beta) return beta;
        if (standPat > alpha) alpha = standPat;
        if (qply >= MAX_QPLY) return alpha;
    }

    orderMoves(board, moves, count, ply);
    for (int i = 0; i < count; ++i) {
        Move move = moves[i];
        if (!inCheck && !moveIsCapture(board, move) && !moveIsPromotion(move))
            continue;

        Undo undo;
        doMove(board, move, &undo);

        /* Store this child position so deeper search can detect repetitions
         * created by the hypothetical line as well as the real game history.
         */
        g_searchHashes[ply + 1] = context && context->positionHash
            ? context->positionHash(board) : 0;

        /* A draw scores 0: good when losing, but worse than any winning
         * continuation when ahead.
         */
        int score = isThirdRepetition(board, ply + 1, context)
            ? 0
            : -quiescence(board, -beta, -alpha, ply + 1, qply + 1, context);

        undoMove(board, move, &undo);

        if (g_timeUp) return 0;
        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    return alpha;
}

static int negamax(Board *board, int depth, int alpha, int beta, int ply,
                   int checkExtensions, Move *bestOut,
                   const BotContext *context) {
    if (timeExpired()) return 0;
    if (ply >= MAX_PLY - 1) return evaluate(board) * (board->turn ? 1 : -1);

    bool inCheck = sideInCheck(board);
    if (depth <= 0) return quiescence(board, alpha, beta, ply, 0, context);

    /* A small number of check extensions helps avoid overlooking forced tactics. */
    if (inCheck && checkExtensions < MAX_CHECK_EXT) {
        ++depth;
        ++checkExtensions;
    }

    Move moves[MAX_MOVES];
    int count = legalMoves(board, moves);
    if (count == 0) return inCheck ? -(MATE_SCORE - ply) : 0;

    orderMoves(board, moves, count, ply);

    int bestScore = -INF_SCORE;
    Move bestMove = moves[0];
    bool firstMove = true;

    for (int i = 0; i < count; ++i) {
        Move move = moves[i];
        bool capture = moveIsCapture(board, move);
        bool promotion = moveIsPromotion(move);
        Undo undo;
        doMove(board, move, &undo);

        /* Track the hypothetical child position for repetition detection in
         * this line of search. */
        g_searchHashes[ply + 1] = context && context->positionHash
            ? context->positionHash(board) : 0;

        int score;
        if (isThirdRepetition(board, ply + 1, context)) {
            /* Treat a third occurrence as a draw rather than preferring it
             * blindly, regardless of whether the side to move is ahead or
             * behind. Negamax will prefer 0 when losing and avoid it when
             * a winning continuation scores above 0.
             */
            score = 0;
        } else if (firstMove) {
            score = -negamax(board, depth - 1, -beta, -alpha, ply + 1,
                             checkExtensions, NULL, context);
        } else {
            /* Principal-variation search: scout later moves with a null window. */
            score = -negamax(board, depth - 1, -alpha - 1, -alpha, ply + 1,
                             checkExtensions, NULL, context);
            if (!g_timeUp && score > alpha && score < beta) {
                score = -negamax(board, depth - 1, -beta, -alpha, ply + 1,
                                 checkExtensions, NULL, context);
            }
        }
        undoMove(board, move, &undo);

        if (g_timeUp) return 0;
        firstMove = false;

        if (score > bestScore) {
            bestScore = score;
            bestMove = move;
        }
        if (score > alpha) alpha = score;

        if (alpha >= beta) {
            if (!capture && !promotion) rememberKiller(move, ply);
            break;
        }
    }

    if (bestOut) *bestOut = bestMove;
    return bestScore;
}

static Move bebopBot(Board *board, Move legal[], int legalCount,
                            double timeLeftMs, double incMs,
                            const BotContext *context) {
    if (!g_tablesReady) initEvalTables();
    if (legalCount <= 0) return 0;
    if (legalCount == 1) return legal[0];

    /* Keep a larger safety margin than the baseline and scale with the clock. */
    double safeTime = timeLeftMs > 0.0 ? timeLeftMs : 0.1;
    double budget = safeTime / 28.0 + (incMs > 0.0 ? incMs * 0.70 : 0.0);
    double cap = safeTime * 0.12;
    if (budget > cap) budget = cap;
    if (budget > 300.0) budget = 300.0;
    double minimumBudget = safeTime < 2.0 ? safeTime * 0.05 : 0.2;
    if (budget < minimumBudget) budget = minimumBudget;

    g_deadlineMs = nowMs() + budget;
    g_timeUp = false;
    g_nodes = 0;
    memset(g_killerValid, 0, sizeof(g_killerValid));

    Move bestMove = legal[0];
    int previousScore = 0;

    for (int depth = 1; depth <= 32; ++depth) {
        Move depthBest = bestMove;
        int score;

        if (depth == 1) {
            score = negamax(board, depth, -INF_SCORE, INF_SCORE, 0, 0, &depthBest, context);
        } else {
            /* Aspiration window; widen and retry if the score falls outside it. */
            int window = 35;
            int alpha = previousScore - window;
            int beta = previousScore + window;
            for (;;) {
                score = negamax(board, depth, alpha, beta, 0, 0,
                                &depthBest, context);
                if (g_timeUp) break;
                if (score <= alpha) {
                    window *= 2;
                    alpha = previousScore - window;
                    if (alpha < -INF_SCORE) alpha = -INF_SCORE;
                    continue;
                }
                if (score >= beta) {
                    window *= 2;
                    beta = previousScore + window;
                    if (beta > INF_SCORE) beta = INF_SCORE;
                    continue;
                }
                break;
            }
        }

        if (g_timeUp) break;
        bestMove = depthBest;
        previousScore = score;
        if (abs(score) >= MATE_SCORE - 1000) break;
        if (nowMs() >= g_deadlineMs) break;
    }

    return bestMove;
}

REGISTER_BOT("bebop", bebopBot);
