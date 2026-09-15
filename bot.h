#ifndef BOT_H
#define BOT_H

#include "movegen.h"

/* ---------------------------------------------------------------------
 * Bot interface.
 *
 * A bot is a function that picks one of the legal moves it's handed.
 * `timeLeftMs` / `incMs` are the bot's own clock (bullet time control),
 * in case it wants to budget a search; a bot is free to ignore them
 * entirely (the random bot does).
 * ------------------------------------------------------------------- */
typedef Move (*BotFunc)(Board *board, Move legal[], int legalCount,
                         double timeLeftMs, double incMs);

/* Small xorshift RNG. Bots that want randomness should keep their own
 * BotRng (seeded via botRngSeed) rather than reach for rand(), so
 * matches stay reproducible given a --seed. */
typedef struct {
    U64 state;
} BotRng;

void botRngSeed(BotRng *r, U64 seed);
U64 botRngNext(BotRng *r);

/* Shared scratch RNG available to any bot that doesn't need its own
 * seeded stream. Games are played sequentially, so this is safe. */
extern BotRng g_botRng;

/* ---------------------------------------------------------------------
 * Registry.
 *
 * Each file in bots/ registers itself at program startup using
 * REGISTER_BOT below - nothing else needs to be edited to add a bot.
 * ------------------------------------------------------------------- */
void registerBot(const char *name, BotFunc fn);
BotFunc findBot(const char *name);
void listBots(void); /* prints registered bot names to stdout */

/* Put this at file scope in a bots/ source file:
 *     REGISTER_BOT("mybot", myBotFunction);
 * It runs before main() via a constructor attribute, so simply
 * compiling+linking the file is enough to make the bot available. */
#define REGISTER_BOT(NAME, FN) \
    static void __attribute__((constructor)) register_##FN(void) { \
        registerBot(NAME, FN); \
    }

#endif
