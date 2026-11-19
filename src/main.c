#include "game.h"
#include "pgn.h"
#include "movegen.h"
#include "bot.h"
#include "gui.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The selected engine bot is called through this wrapper during a human game. */
static BotFunc g_opponentBot = NULL;
static const char *g_opponentName = "Bot";

static void printUsage(const char *prog) {
    fprintf(stderr,
        "usage:\n"
        "  %s <botA> <botB> [numGames=100] [timeMs=60000] [incMs=0] [seed=42]\n"
        "  %s --play <bot> [white|black] [timeMs=60000] [incMs=0] [seed=42]\n"
        "  %s --gui <bot> [seed=42]\n"
        "  %s --list\n\n",
        prog, prog, prog, prog);
    fprintf(stderr, "available bots:\n");
    listBots();
}

static int pieceTypeAt(const Board *board, int square) {
    U64 bit = BIT(square);
    if (board->pawnBB   & bit) return PAWN;
    if (board->knightBB & bit) return KNIGHT;
    if (board->bishopBB & bit) return BISHOP;
    if (board->rookBB   & bit) return ROOK;
    if (board->queenBB  & bit) return QUEEN;
    if (board->kingBB   & bit) return KING;
    return -1;
}

static void printBoard(const Board *board) {
    static const char pieceChars[] = "PNBRQK";

    putchar('\n');
    for (int rank = 7; rank >= 0; --rank) {
        printf("%d  ", rank + 1);
        for (int file = 0; file < 8; ++file) {
            int square = rank * 8 + file;
            int piece = pieceTypeAt(board, square);
            char symbol = piece < 0 ? '.' : pieceChars[piece];

            if (piece >= 0 && !(board->whiteBB & BIT(square)))
                symbol = (char)tolower((unsigned char)symbol);

            printf("%c ", symbol);
        }
        putchar('\n');
    }
    puts("   a b c d e f g h");
    printf("   %s to move\n\n", board->turn ? "White" : "Black");
}

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

/* Convert the engine's encoded move to coordinate notation (UCI-style). */
static void moveToUci(Move move, char out[6]) {
    int from = EXTRACT_FROM(move);
    int to = EXTRACT_TO(move);
    int flag = EXTRACT_FLAG(move);

    out[0] = (char)('a' + (from & 7));
    out[1] = (char)('1' + (from >> 3));
    out[2] = (char)('a' + (to & 7));
    out[3] = (char)('1' + (to >> 3));

    char suffix = promotionSuffix(flag);
    if (suffix) {
        out[4] = suffix;
        out[5] = '\0';
    } else {
        out[4] = '\0';
    }
}

static int equalsIgnoreCase(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

static int equalsIgnoreCaseN(const char *a, const char *b, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (!a[i] || !b[i] ||
            tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]))
            return 0;
    }
    return 1;
}

static void printLegalMoves(const Move legal[], int legalCount) {
    printf("Legal moves (%d):\n", legalCount);
    for (int i = 0; i < legalCount; ++i) {
        char uci[6];
        moveToUci(legal[i], uci);
        printf("%s%s", uci, (i + 1) % 8 == 0 || i + 1 == legalCount ? "\n" : "  ");
    }
}

/* Human callback: reads a coordinate move and returns the matching legal move. */
static Move humanBot(Board *board, Move legal[], int legalCount,
                     double timeLeftMs, double incMs,
                     const BotContext *context) {
    (void)timeLeftMs;
    (void)incMs;
    (void)context;

    printBoard(board);
    puts("Enter a move like e2e4 or e7e8q. Type 'moves' to list legal moves.");

    char input[128];
    for (;;) {
        printf("Your move > ");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) {
            fprintf(stderr, "\nInput closed; cannot continue the game.\n");
            exit(EXIT_FAILURE);
        }

        input[strcspn(input, "\r\n")] = '\0';
        if (input[0] == '\0') continue;

        if (equalsIgnoreCase(input, "moves") || equalsIgnoreCase(input, "legal")) {
            printLegalMoves(legal, legalCount);
            continue;
        }
        if (equalsIgnoreCase(input, "help") || equalsIgnoreCase(input, "?")) {
            puts("Use coordinate notation: e2e4, g1f3, e7e8q (promotion).\n"
                 "Type 'moves' to see every legal move.");
            continue;
        }

        size_t inputLength = strlen(input);
        for (int i = 0; i < legalCount; ++i) {
            char uci[6];
            moveToUci(legal[i], uci);
            if (equalsIgnoreCase(input, uci)) return legal[i];

            /* Let a four-character promotion move default to a queen. */
            if (inputLength == 4 && IS_PROMO(EXTRACT_FLAG(legal[i])) &&
                promotionSuffix(EXTRACT_FLAG(legal[i])) == 'q' &&
                equalsIgnoreCaseN(input, uci, 4)) {
                return legal[i];
            }
        }

        puts("That is not a legal move. Enter a move from the current position.");
    }
}

/* Bot callback wrapper so the user can see the engine's move in the terminal. */
static Move opponentBot(Board *board, Move legal[], int legalCount,
                        double timeLeftMs, double incMs,
                        const BotContext *context) {
    Move chosen = g_opponentBot(board, legal, legalCount,
                                timeLeftMs, incMs, context);

    for (int i = 0; i < legalCount; ++i) {
        if (legal[i] == chosen) {
            char uci[6];
            moveToUci(chosen, uci);
            printf("\n%s plays %s\n", g_opponentName, uci);
            fflush(stdout);
            return chosen;
        }
    }

    fprintf(stderr, "\n%s returned a move that is not in the legal move list.\n",
            g_opponentName);
    return chosen;
}

static const char *resultReasonName(ResultReason reason) {
    switch (reason) {
        case REASON_CHECKMATE: return "checkmate";
        case REASON_STALEMATE: return "stalemate";
        case REASON_FIFTY_MOVE: return "50-move rule";
        case REASON_REPETITION: return "repetition";
        case REASON_INSUFFICIENT_MATERIAL: return "insufficient material";
        case REASON_TIMEOUT: return "time";
        case REASON_MOVE_LIMIT: return "move limit";
        default: return "illegal move";
    }
}

static int playAgainstBot(const char *botName, int humanIsWhite,
                          double timeMs, double incMs, U64 seed) {
    BotFunc bot = findBot(botName);
    if (!bot) {
        fprintf(stderr, "unknown bot: %s\n\n", botName);
        printUsage("chess");
        return 1;
    }

    initMoveGen();
    initZobrist();
    botRngSeed(&g_botRng, seed);

    g_opponentBot = bot;
    g_opponentName = botName;

    BotFunc whiteFn = humanIsWhite ? humanBot : opponentBot;
    BotFunc blackFn = humanIsWhite ? opponentBot : humanBot;

    printf("Playing %s as %s.\n", botName, humanIsWhite ? "White" : "Black");
    printf("Clock: %.0f ms per side, increment: %.0f ms.\n", timeMs, incMs);

    Move moveLog[MAX_GAME_PLIES];
    int moveCount = 0;
    GameResult result = playGame(whiteFn, blackFn, NULL,
                                 timeMs, incMs, moveLog, &moveCount);

    /* Rebuild and display the final position after playGame returns. */
    Board finalBoard;
    setFen(&finalBoard, (char *)START_FEN);
    for (int i = 0; i < moveCount; ++i)
        makeMove(&finalBoard, moveLog[i]);
    printBoard(&finalBoard);

    if (result.winner == RESULT_DRAW) {
        printf("Draw by %s.\n", resultReasonName(result.reason));
    } else {
        int humanWon = (humanIsWhite && result.winner == RESULT_WHITE_WIN) ||
                       (!humanIsWhite && result.winner == RESULT_BLACK_WIN);
        printf("%s by %s.\n", humanWon ? "You win" : "Bot wins",
               resultReasonName(result.reason));
    }

    FILE *pgnOut = fopen("human_vs_bot.pgn", "w");
    if (!pgnOut) {
        perror("Could not create human_vs_bot.pgn");
        return 1;
    }

    const char *whiteName = humanIsWhite ? "Human" : botName;
    const char *blackName = humanIsWhite ? botName : "Human";
    bool pgnOk = pgnWriteGame(pgnOut, 1, whiteName, blackName,
                              NULL, moveLog, moveCount, result);
    fclose(pgnOut);

    if (!pgnOk) {
        fprintf(stderr, "Could not write the completed game to PGN.\n");
        return 1;
    }

    puts("Game saved to human_vs_bot.pgn");
    return 0;
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--list") == 0) {
        printf("available bots:\n");
        listBots();
        return 0;
    }

    if (argc > 1 && strcmp(argv[1], "--gui") == 0) {
        if (argc < 3) {
            printUsage(argv[0]);
            return 1;
        }
        U64 seed = argc > 3 ? (U64)strtoull(argv[3], NULL, 10) : 42ULL;
        return runChessGui(argv[2], seed);
    }

    if (argc > 1 && strcmp(argv[1], "--play") == 0) {
        if (argc < 3) {
            printUsage(argv[0]);
            return 1;
        }

        const char *botName = argv[2];
        int humanIsWhite = 1;
        int arg = 3;

        if (arg < argc &&
            (equalsIgnoreCase(argv[arg], "white") ||
             equalsIgnoreCase(argv[arg], "black"))) {
            humanIsWhite = equalsIgnoreCase(argv[arg], "white");
            ++arg;
        }

        double timeMs = arg < argc ? atof(argv[arg++]) : 60000.0;
        double incMs = arg < argc ? atof(argv[arg++]) : 0.0;
        U64 seed = arg < argc ? (U64)strtoull(argv[arg++], NULL, 10) : 42ULL;

        return playAgainstBot(botName, humanIsWhite, timeMs, incMs, seed);
    }

    if (argc < 3) {
        printUsage(argv[0]);
        return 1;
    }

    const char *botAName = argv[1];
    const char *botBName = argv[2];
    int numGames = argc > 3 ? atoi(argv[3]) : 100;
    double timeMs = argc > 4 ? atof(argv[4]) : 60000.0;
    double incMs = argc > 5 ? atof(argv[5]) : 0.0;
    U64 seed = argc > 6 ? (U64)strtoull(argv[6], NULL, 10) : 42ULL;

    BotFunc botA = findBot(botAName);
    BotFunc botB = findBot(botBName);

    if (!botA) { fprintf(stderr, "unknown bot: %s\n\n", botAName); printUsage(argv[0]); return 1; }
    if (!botB) { fprintf(stderr, "unknown bot: %s\n\n", botBName); printUsage(argv[0]); return 1; }

    initMoveGen();
    initZobrist();
    botRngSeed(&g_botRng, seed);

    for (int g = 0; g < numGames; g++) {
        bool aIsWhite = (g % 2 == 0);

        BotFunc whiteFn = aIsWhite ? botA : botB;
        BotFunc blackFn = aIsWhite ? botB : botA;

        Move moveLog[MAX_GAME_PLIES];
        int moveCount = 0;

        GameResult res = playGame(
            whiteFn, blackFn, NULL,
            timeMs, incMs,
            moveLog, &moveCount
        );

        const char *whiteName = aIsWhite ? botAName : botBName;
        const char *blackName = aIsWhite ? botBName : botAName;

        if (!pgnWriteGame(
                stdout,
                g + 1,
                whiteName,
                blackName,
                NULL,
                moveLog,
                moveCount,
                res
            )) {
            fprintf(stderr, "Failed to write PGN for game %d\n", g + 1);
        }

        fflush(stdout);
    }

    fclose(stdout);
    return 0;
}
