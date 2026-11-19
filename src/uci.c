/*
 * UCI (Universal Chess Interface) driver.
 *
 * Speaks the subset of UCI that real GUIs/match tools actually rely on:
 *   uci / isready / ucinewgame / position / go / stop / quit
 * "setoption" and "debug" are accepted and ignored (bebop has no tunable
 * UCI options yet). "ponderhit" is not implemented; bebop never ponders,
 * so a GUI that sends it will just get a late bestmove for the real think.
 *
 * "go" runs the search on a background thread so that "stop" (and a new
 * "position"/"ucinewgame"/"quit") can interrupt a long-running search
 * (e.g. "go infinite" or "go depth N") instead of blocking stdin forever.
 */

#define _POSIX_C_SOURCE 200809L

#include "board.h"
#include "movegen.h"
#include "perft.h"
#include "search.h"
#include "zobrist.h"

#include <pthread.h>
#include <stdatomic.h>
#include <inttypes.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> /* strcasecmp/strncasecmp */
#include <time.h>

static double nowMsUci(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

#define UCI_MAX_HISTORY 2048

static Board g_board;
static U64 g_history[UCI_MAX_HISTORY];
static int g_historyLen;

static pthread_t g_searchThread;
static int g_searchRunning = 0;
static atomic_bool g_stopRequested = ATOMIC_VAR_INIT(false);

/* Parameters handed to the search thread by handleGo(). */
static double g_goBudgetMs;
static int g_goMaxDepth;

/* ---- small helpers for move <-> UCI-string conversion ---- */

static char promotionSuffix(int flag) {
    if (!IS_PROMO(flag)) return '\0';
    switch (PROMO_PT(flag)) {
        case QUEEN:  return 'q';
        case ROOK:   return 'r';
        case BISHOP: return 'b';
        case KNIGHT: return 'n';
        default:     return '\0';
    }
}

static void moveToUciStr(Move move, char out[6]) {
    int from = EXTRACT_FROM(move);
    int to = EXTRACT_TO(move);
    int flag = EXTRACT_FLAG(move);

    out[0] = (char)('a' + (from & 7));
    out[1] = (char)('1' + (from >> 3));
    out[2] = (char)('a' + (to & 7));
    out[3] = (char)('1' + (to >> 3));

    char suffix = promotionSuffix(flag);
    if (suffix) { out[4] = suffix; out[5] = '\0'; }
    else out[4] = '\0';
}

static void pushHistory(void) {
    U64 hash = zobristHash(&g_board);
    if (g_historyLen == UCI_MAX_HISTORY) {
        memmove(g_history, g_history + 1,
                (UCI_MAX_HISTORY - 1) * sizeof(g_history[0]));
        --g_historyLen;
    }
    g_history[g_historyLen++] = hash;
}

static void resetGame(void) {
    boardSetFen(&g_board, START_FEN);
    g_historyLen = 0;
    pushHistory();
}

/* Applies one "e2e4"/"e7e8q"-style token, if it matches a legal move. A
 * malformed or stale token (a desynced GUI) is silently ignored rather
 * than crashing the engine. */
static void applyUciMove(const char *token) {
    Move legal[MAX_MOVES];
    int count = legalMoves(&g_board, legal);
    size_t tokLen = strlen(token);

    for (int i = 0; i < count; ++i) {
        char buf[6];
        moveToUciStr(legal[i], buf);
        if (strcasecmp(buf, token) == 0) {
            makeMove(&g_board, legal[i]);
            pushHistory();
            return;
        }
        /* A 4-char token against a 5-char (promotion) move defaults to
         * queen, matching the human-input convenience in main.c. */
        if (tokLen == 4 && strlen(buf) == 5 &&
            strncasecmp(buf, token, 4) == 0 && buf[4] == 'q') {
            makeMove(&g_board, legal[i]);
            pushHistory();
            return;
        }
    }
}

/* ---------------------------- search thread ---------------------------- */

static void *searchThreadMain(void *arg) {
    (void)arg;

    Move legal[MAX_MOVES];
    int legalCount = legalMoves(&g_board, legal);

    if (legalCount == 0) {
        /* No legal move (checkmate/stalemate reached); UCI has no clean way
         * to say "no move", 0000 is the conventional null move. */
        printf("bestmove 0000\n");
        fflush(stdout);
        return NULL;
    }

    SearchContext context = {
        .positionHistory = g_history,
        .positionHistoryCount = g_historyLen
    };

    double t0 = nowMsUci();
    SearchInfo info;
    Move best = searchBestMove(&g_board, legal, legalCount,
                            g_goBudgetMs, g_goMaxDepth,
                            &g_stopRequested, &context, &info);
    double elapsedMs = nowMsUci() - t0;
    if (elapsedMs < 1.0) elapsedMs = 1.0;

    long nps = (long)(info.nodes / (elapsedMs / 1000.0));
    if (info.isMate) {
        printf("info depth %d score mate %d nodes %ld nps %ld time %.0f\n",
               info.depthCompleted, info.mateInMoves, info.nodes, nps, elapsedMs);
    } else {
        printf("info depth %d score cp %d nodes %ld nps %ld time %.0f\n",
               info.depthCompleted, info.scoreCp, info.nodes, nps, elapsedMs);
    }

    char uci[6];
    moveToUciStr(best, uci);
    printf("bestmove %s\n", uci);
    fflush(stdout);
    return NULL;
}

/* Blocks until any in-flight search has stopped. Safe to call even when
 * no search is running. */
static void stopSearchAndWait(void) {
    if (!g_searchRunning) return;
    atomic_store_explicit(&g_stopRequested, true, memory_order_relaxed);
    pthread_join(g_searchThread, NULL);
    g_searchRunning = 0;
}

/* ------------------------------ "position" ------------------------------ */

static void handlePosition(char *args) {
    char *saveptr;
    char *tok = strtok_r(args, " \t", &saveptr);
    if (!tok) return;

    if (strcmp(tok, "startpos") == 0) {
        resetGame();
        tok = strtok_r(NULL, " \t", &saveptr);
    } else if (strcmp(tok, "fen") == 0) {
        char fen[256] = {0};
        int first = 1;
        while ((tok = strtok_r(NULL, " \t", &saveptr)) != NULL) {
            if (strcmp(tok, "moves") == 0) break;
            if (!first) strncat(fen, " ", sizeof(fen) - strlen(fen) - 1);
            strncat(fen, tok, sizeof(fen) - strlen(fen) - 1);
            first = 0;
        }
        boardSetFen(&g_board, fen);
        g_historyLen = 0;
        pushHistory();
    } else {
        return; /* malformed "position" line; ignore rather than guess */
    }

    if (tok && strcmp(tok, "moves") == 0) {
        while ((tok = strtok_r(NULL, " \t", &saveptr)) != NULL)
            applyUciMove(tok);
    }
}

/* ---------------------------------- "go" --------------------------------- */

static void handleGo(char *args) {
    double wtime = 0, btime = 0, winc = 0, binc = 0, movetime = -1;
    int depth = -1, infinite = 0;

    char *saveptr;
    char *tok = strtok_r(args, " \t", &saveptr);
    if (tok && strcmp(tok, "perft") == 0) {
        char *depthToken = strtok_r(NULL, " \t", &saveptr);
        char *extra = strtok_r(NULL, " \t", &saveptr);
        char *end = NULL;
        errno = 0;
        long perftDepth = depthToken ? strtol(depthToken, &end, 10) : -1;
        if (!depthToken || !*depthToken || !end || *end != '\0' || errno ||
            extra || perftDepth < 0 || perftDepth > 20) {
            printf("info string invalid go perft depth (expected 0..20)\n");
        } else {
            uint64_t nodes = perftNodes(&g_board, (int)perftDepth);
            printf("Nodes: %" PRIu64 "\n", nodes);
        }
        fflush(stdout);
        return;
    }
    while (tok) {
        if      (!strcmp(tok, "wtime"))    { tok = strtok_r(NULL, " \t", &saveptr); if (tok) wtime = atof(tok); }
        else if (!strcmp(tok, "btime"))    { tok = strtok_r(NULL, " \t", &saveptr); if (tok) btime = atof(tok); }
        else if (!strcmp(tok, "winc"))     { tok = strtok_r(NULL, " \t", &saveptr); if (tok) winc = atof(tok); }
        else if (!strcmp(tok, "binc"))     { tok = strtok_r(NULL, " \t", &saveptr); if (tok) binc = atof(tok); }
        else if (!strcmp(tok, "movetime")) { tok = strtok_r(NULL, " \t", &saveptr); if (tok) movetime = atof(tok); }
        else if (!strcmp(tok, "depth"))    { tok = strtok_r(NULL, " \t", &saveptr); if (tok) depth = atoi(tok); }
        else if (!strcmp(tok, "infinite")) { infinite = 1; }
        /* "ponder", "nodes", "mate", "searchmoves" are accepted but not
         * specially handled yet. */
        tok = strtok_r(NULL, " \t", &saveptr);
    }

    double budgetMs;
    int maxDepth = (depth > 0) ? depth : 32;

    if (movetime >= 0) {
        /* Leave a small safety margin to actually print bestmove in time. */
        budgetMs = movetime - 20.0;
        if (budgetMs < 5.0) budgetMs = 5.0;
    } else if (infinite || (wtime <= 0 && btime <= 0)) {
        budgetMs = 1e9; /* only "stop" (or a found mate) ends this */
    } else {
        double timeLeftMs = g_board.turn ? wtime : btime;
        double incMs = g_board.turn ? winc : binc;
        double safeTime = timeLeftMs > 0 ? timeLeftMs : 0.1;

        /* Allocate a small fraction of the remaining clock plus most of
         * the increment, with a hard cap to preserve time for later moves. */
        budgetMs = safeTime / 28.0 + (incMs > 0 ? incMs * 0.70 : 0.0);
        double cap = safeTime * 0.33;
        if (budgetMs > cap) budgetMs = cap;
        if (budgetMs < 5.0) budgetMs = 5.0;
    }

    g_goBudgetMs = budgetMs;
    g_goMaxDepth = maxDepth;
    atomic_store_explicit(&g_stopRequested, false, memory_order_relaxed);
    int err = pthread_create(&g_searchThread, NULL, searchThreadMain, NULL);
    if (err != 0) {
        g_searchRunning = 0;
        printf("info string could not start search thread: %s\n", strerror(err));
        printf("bestmove 0000\n");
        fflush(stdout);
        return;
    }
    g_searchRunning = 1;
}

/* --------------------------------- main loop ------------------------------ */

static void trim(char *s) {
    s[strcspn(s, "\r\n")] = '\0';
}

int runUci(void) {
    initMoveGen();
    initZobrist();
    resetGame();

    char *line = NULL;
    size_t cap = 0;
    ssize_t len;

    while ((len = getline(&line, &cap, stdin)) >= 0) {
        (void)len;
        trim(line);

        char *cmd = line;
        while (*cmd == ' ' || *cmd == '\t') ++cmd;
        if (*cmd == '\0') continue;

        if (!strcmp(cmd, "uci")) {
            printf("id name Bebop\n");
            printf("id author maxzwerin\n");
            printf("uciok\n");
            fflush(stdout);
        } else if (!strcmp(cmd, "isready")) {
            printf("readyok\n");
            fflush(stdout);
        } else if (!strcmp(cmd, "ucinewgame")) {
            stopSearchAndWait();
            resetGame();
        } else if (!strncmp(cmd, "position", 8)) {
            stopSearchAndWait();
            handlePosition(cmd + 8);
        } else if (!strncmp(cmd, "go", 2) && (cmd[2] == '\0' || cmd[2] == ' ')) {
            stopSearchAndWait();
            handleGo(cmd + 2);
        } else if (!strcmp(cmd, "stop")) {
            stopSearchAndWait();
        } else if (!strcmp(cmd, "quit")) {
            stopSearchAndWait();
            break;
        }
    }

    stopSearchAndWait();
    free(line);
    return 0;
}
