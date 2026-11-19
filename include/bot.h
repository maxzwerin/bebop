#ifndef BOT_H
#define BOT_H

#include "movegen.h"

/*
 * Read-only information about the current game, supplied by playGame() for
 * the duration of a bot invocation. The history includes the current root
 * position. Bots may inspect it but must not retain the pointers after they
 * return.
 *
 * positionHash is the engine's canonical board hash function. Supplying it
 * here keeps bots from depending on game.c or duplicating Zobrist hashing.
 */
typedef U64 (*BotPositionHashFn)(Board *board);

typedef struct {
    const U64 *positionHistory;
    int positionHistoryCount;
    BotPositionHashFn positionHash;
} BotContext;

/* ---------------------------------------------------------------------
 * Bot interface.
 *
 * A bot picks one of the legal moves it is handed. timeLeftMs / incMs are
 * the bot's own clock. context provides read-only game-level information
 * such as repetition history; it is valid only during this call.
 * ------------------------------------------------------------------- */
typedef Move (*BotFunc)(Board *board, Move legal[], int legalCount,
                        double timeLeftMs, double incMs,
                        const BotContext *context);

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
