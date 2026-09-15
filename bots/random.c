#include "../bot.h"

static Move randomBot(Board *board, Move legal[], int legalCount, double timeLeftMs, double incMs) {
    (void)board; (void)timeLeftMs; (void)incMs;
    int idx = (int)(botRngNext(&g_botRng) % (U64)legalCount);
    return legal[idx];
}

REGISTER_BOT("random", randomBot);
