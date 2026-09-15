#include "../bot.h"

/* Random, but always takes a free capture if one exists. Mainly useful
 * as a weak-but-not-trivial opponent to sanity check the match runner
 * and Elo estimate against. */
static Move greedyBot(Board *board, Move legal[], int legalCount, double timeLeftMs, double incMs) {
    (void)timeLeftMs; (void)incMs;
    U64 enemy = board->turn ? board->blackBB : board->whiteBB;

    int captureIdx[MAX_MOVES];
    int captureCount = 0;

    for (int i = 0; i < legalCount; i++) {
        int to = EXTRACT_TO(legal[i]);
        if (BIT(to) & enemy) captureIdx[captureCount++] = i;
    }

    if (captureCount > 0) {
        int pick = (int)(botRngNext(&g_botRng) % (U64)captureCount);
        return legal[captureIdx[pick]];
    }

    int idx = (int)(botRngNext(&g_botRng) % (U64)legalCount);
    return legal[idx];
}

REGISTER_BOT("greedy", greedyBot);
