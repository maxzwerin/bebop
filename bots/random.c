#include "bot.h"

static Move randomBot(Board *board, Move legal[], int legalCount,
                          double timeLeftMs, double incMs,
                          const BotContext *context)
{
    (void)board; (void)timeLeftMs; (void)incMs; (void)context;
    int idx = (int)(botRngNext(&g_botRng) % (U64)legalCount);
    return legal[idx];
}

REGISTER_BOT("random", randomBot);
