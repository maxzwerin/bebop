#include "game.h"
#include <math.h>
#include <string.h>

typedef struct {
    int wins, losses, draws;
    int winsAsWhite, winsAsBlack;
    int reasonCounts[REASON_COUNT];
} Stats;

/* Standard "how many Elo does the score difference represent" estimate. */
static double eloDiff(double scoreFraction) {
    if (scoreFraction <= 0.0) return -800.0;
    if (scoreFraction >= 1.0) return 800.0;
    return -400.0 * log10(1.0 / scoreFraction - 1.0);
}

static void printUsage(const char *prog) {
    fprintf(stderr, "usage: %s <botA> <botB> [numGames=100] [timeMs=60000] [incMs=0] [seed=42]\n", prog);
    fprintf(stderr, "       %s --list\n\n", prog);
    fprintf(stderr, "available bots:\n");
    listBots();
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--list") == 0) {
        printf("available bots:\n");
        listBots();
        return 0;
    }

    if (argc < 3) {
        printUsage(argv[0]);
        return 1;
    }

    const char *botAName = argv[1];
    const char *botBName = argv[2];
    int numGames = argc > 3 ? atoi(argv[3]) : 100;
    double timeMs = argc > 4 ? atof(argv[4]) : 60000.0; /* 60+0 bullet by default */
    double incMs = argc > 5 ? atof(argv[5]) : 0.0;
    U64 seed = argc > 6 ? (U64)strtoull(argv[6], NULL, 10) : 42ULL;

    BotFunc botA = findBot(botAName);
    BotFunc botB = findBot(botBName);

    if (!botA) { fprintf(stderr, "unknown bot: %s\n\n", botAName); printUsage(argv[0]); return 1; }
    if (!botB) { fprintf(stderr, "unknown bot: %s\n\n", botBName); printUsage(argv[0]); return 1; }

    initMoveGen();
    initZobrist();
    botRngSeed(&g_botRng, seed);

    Stats statsA = {0};
    Stats statsB = {0};

    double t0 = nowMsShared();

    for (int g = 0; g < numGames; g++) {
        bool aIsWhite = (g % 2 == 0);

        BotFunc whiteFn = aIsWhite ? botA : botB;
        BotFunc blackFn = aIsWhite ? botB : botA;

        GameResult res = playGame(whiteFn, blackFn, NULL, timeMs, incMs, NULL, NULL);

        if (res.winner == RESULT_DRAW) {
            statsA.draws++; statsB.draws++;
            statsA.reasonCounts[res.reason]++;
        } else {
            bool aWon = (res.winner == RESULT_WHITE_WIN) == aIsWhite;
            if (aWon) {
                statsA.wins++; statsB.losses++;
                if (aIsWhite) statsA.winsAsWhite++; else statsA.winsAsBlack++;
            } else {
                statsB.wins++; statsA.losses++;
                if (!aIsWhite) statsB.winsAsWhite++; else statsB.winsAsBlack++;
            }
            statsA.reasonCounts[res.reason]++;
        }

        if ((g + 1) % 100 == 0 || g + 1 == numGames) {
            fprintf(stderr, "\rgames played: %d/%d (%s %d-%d-%d %s)",
                    g + 1, numGames, botAName, statsA.wins, statsA.losses, statsA.draws, botBName);
            fflush(stderr);
        }
    }

    double elapsed = (nowMsShared() - t0) / 1000.0;
    fprintf(stderr, "\n");

    double scoreA = (statsA.wins + 0.5 * statsA.draws) / (double)numGames;

    printf("\n=================== match result ===================\n");
    printf("%-10s  vs  %-10s   (%d games, %.0f+%.0f ms bullet)\n", botAName, botBName, numGames, timeMs, incMs);
    printf("------------------------------------------------------\n");
    printf("%-10s : %3d wins, %3d losses, %3d draws   (score %.1f%%)\n",
           botAName, statsA.wins, statsA.losses, statsA.draws, 100.0 * scoreA);
    printf("%-10s : %3d wins, %3d losses, %3d draws   (score %.1f%%)\n",
           botBName, statsB.wins, statsB.losses, statsB.draws, 100.0 * (1.0 - scoreA));
    printf("------------------------------------------------------\n");
    printf("%-10s as White: %d wins   as Black: %d wins\n", botAName, statsA.winsAsWhite, statsA.winsAsBlack);
    printf("%-10s as White: %d wins   as Black: %d wins\n", botBName, statsB.winsAsWhite, statsB.winsAsBlack);
    printf("------------------------------------------------------\n");
    printf("estimated Elo diff (%s - %s): %+.0f\n", botAName, botBName, eloDiff(scoreA));
    printf("------------------------------------------------------\n");
    printf("game end reasons:\n");
    for (int i = 0; i < REASON_COUNT; i++) {
        if (statsA.reasonCounts[i] > 0)
            printf("  %-24s %d\n", reasonName((ResultReason)i), statsA.reasonCounts[i]);
    }
    printf("------------------------------------------------------\n");
    printf("total time: %.2fs  (%.2f ms/game)\n", elapsed, 1000.0 * elapsed / numGames);
    printf("=======================================================\n");

    return 0;
}
