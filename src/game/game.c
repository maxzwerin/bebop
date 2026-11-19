#include "game.h"
#include <time.h>

double nowMsShared(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

const char *reasonName(ResultReason r) {
    switch (r) {
        case REASON_CHECKMATE: return "checkmate";
        case REASON_STALEMATE: return "stalemate";
        case REASON_FIFTY_MOVE: return "50-move rule";
        case REASON_REPETITION: return "repetition";
        case REASON_INSUFFICIENT_MATERIAL: return "insufficient material";
        case REASON_TIMEOUT: return "timeout";
        case REASON_MOVE_LIMIT: return "move limit";
        default: return "illegal move (bug)";
    }
}

static int popcountU64(U64 x) { return BITS(x); }

static bool insufficientMaterial(Board *b) {
    if (b->pawnBB || b->rookBB || b->queenBB) return false;

    int whiteMinor = popcountU64(b->whiteBB & (b->knightBB | b->bishopBB));
    int blackMinor = popcountU64(b->blackBB & (b->knightBB | b->bishopBB));

    if (whiteMinor == 0 && blackMinor == 0) return true; /* K v K */
    if (whiteMinor + blackMinor > 2) return false;

    if (whiteMinor <= 1 && blackMinor == 0) return true;
    if (blackMinor <= 1 && whiteMinor == 0) return true;

    if (whiteMinor == 1 && blackMinor == 1 && popcountU64(b->bishopBB) == 2) {
        U64 lightSquares = 0xAA55AA55AA55AA55ULL;
        U64 wBishop = b->bishopBB & b->whiteBB;
        U64 bBishop = b->bishopBB & b->blackBB;
        bool wLight = (wBishop & lightSquares) != 0;
        bool bLight = (bBishop & lightSquares) != 0;
        return wLight == bLight;
    }

    return false;
}

#define MAX_HISTORY (MAX_GAME_PLIES + 8)

GameResult playGame(BotFunc whiteFn, BotFunc blackFn, const char *startFen,
                     double timeMs, double incMs,
                     Move *moveLog, int *moveCountOut) {
    Board board;
    setFen(&board, startFen ? (char *)startFen : START_FEN);

    double clocks[2] = { timeMs, timeMs };

    U64 history[MAX_HISTORY];
    int historyLen = 0;
    history[historyLen++] = zobristHash(&board);

    int ply = 0;
    int moveCount = 0;

    for (;;) {
        Move legal[MAX_MOVES];
        int legalCount = legalMoves(&board, legal);

        int turn = board.turn;
        int kingSq = LSB(board.kingBB & (turn ? board.whiteBB : board.blackBB));
        bool inCheck = isSquareAttacked(&board, kingSq, !turn);

        if (legalCount == 0) {
            GameResult r;
            r.plyCount = ply;
            if (inCheck) {
                r.reason = REASON_CHECKMATE;
                r.winner = turn ? RESULT_BLACK_WIN : RESULT_WHITE_WIN;
            } else {
                r.reason = REASON_STALEMATE;
                r.winner = RESULT_DRAW;
            }
            if (moveCountOut) *moveCountOut = moveCount;
            return r;
        }

        if (board.halfmove >= 100) {
            GameResult r = { RESULT_DRAW, REASON_FIFTY_MOVE, ply };
            if (moveCountOut) *moveCountOut = moveCount;
            return r;
        }

        if (insufficientMaterial(&board)) {
            GameResult r = { RESULT_DRAW, REASON_INSUFFICIENT_MATERIAL, ply };
            if (moveCountOut) *moveCountOut = moveCount;
            return r;
        }

        {
            int reps = 0;
            for (int i = 0; i < historyLen; i++) if (history[i] == history[historyLen - 1]) reps++;
            if (reps >= 3) {
                GameResult r = { RESULT_DRAW, REASON_REPETITION, ply };
                if (moveCountOut) *moveCountOut = moveCount;
                return r;
            }
        }

        if (ply >= MAX_GAME_PLIES) {
            GameResult r = { RESULT_DRAW, REASON_MOVE_LIMIT, ply };
            if (moveCountOut) *moveCountOut = moveCount;
            return r;
        }

        BotFunc toMove = turn ? whiteFn : blackFn;

        double t0 = nowMsShared();
        BotContext context = {
            .positionHistory = history,
            .positionHistoryCount = historyLen,
            .positionHash = zobristHash
        };

        Move chosen = toMove(&board, legal, legalCount,
                             clocks[turn], incMs, &context);
        double elapsed = nowMsShared() - t0;

        clocks[turn] -= elapsed;
        if (clocks[turn] <= 0.0) {
            GameResult r;
            r.plyCount = ply;
            r.reason = REASON_TIMEOUT;
            r.winner = turn ? RESULT_BLACK_WIN : RESULT_WHITE_WIN;
            if (moveCountOut) *moveCountOut = moveCount;
            return r;
        }
        clocks[turn] += incMs;

        bool found = false;
        for (int i = 0; i < legalCount; i++) if (legal[i] == chosen) { found = true; break; }
        if (!found) {
            GameResult r;
            r.plyCount = ply;
            r.reason = REASON_ILLEGAL_NO_MOVE;
            r.winner = turn ? RESULT_BLACK_WIN : RESULT_WHITE_WIN;
            if (moveCountOut) *moveCountOut = moveCount;
            return r;
        }

        makeMove(&board, chosen);
        if (moveLog && moveCount < MAX_GAME_PLIES) moveLog[moveCount] = chosen;
        moveCount++;
        ply++;

        if (historyLen < MAX_HISTORY) history[historyLen++] = zobristHash(&board);
        else history[MAX_HISTORY - 1] = zobristHash(&board);
    }
}
